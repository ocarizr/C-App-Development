#include "money.h"
#include "workforce_application.h"
#include "workforce_export.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    A_LIST,
    A_CREATE,
    A_LOAD,
    A_SAVE,
    A_DELETE,
    A_CONTRACT_LOAD,
    A_CONTRACT_ADD,
    A_CONTRACT_SAVE,
    A_CONTRACT_DELETE,
    A_SALARY_SAVE,
    A_SALARY_DELETE,
    A_REPORT,
    A_EXPORT
} Action;
typedef struct
{
    struct WorkforceViewModel *vm;
    Action action;
} CommandContext;
struct WorkforceViewModel
{
    WorkforceStore *store;
    Mvvm *model;
    CommandContext commands[13];
};
static const char *get(WorkforceViewModel *vm, const char *name)
{
    return mvvm_get(vm->model, name);
}
static bool publish(WorkforceViewModel *vm, const char *name, const char *value)
{
    return mvvm_publish(vm->model, name, value);
}
static void integer(WorkforceViewModel *vm, const char *name, int64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%" PRId64, value);
    (void)publish(vm, name, text);
}
static void money_field(WorkforceViewModel *vm, const char *name, int64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%" PRId64 ".%02" PRId64, value / 100, value % 100);
    (void)publish(vm, name, text);
}
static bool number(WorkforceViewModel *vm, const char *name, int64_t max, int64_t *out)
{
    const char *text = get(vm, name);
    char *end;
    int64_t value;
    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 1 || value > max)
    {
        return false;
    }
    *out = value;
    return true;
}
static bool copy_field(WorkforceViewModel *vm, const char *name, char out[WORKFORCE_NAME_SIZE])
{
    const char *text = get(vm, name);
    size_t length = strlen(text);
    if (length == 0 || length >= WORKFORCE_NAME_SIZE)
    {
        return false;
    }
    memcpy(out, text, length + 1);
    return true;
}
static bool status(WorkforceViewModel *vm, WorkforceStatus result)
{
    static const char *const messages[] = {
        "Operacao concluida.",       "Dados invalidos. Confira os campos.",
        "Registro nao encontrado.",  "Memoria insuficiente.",
        "Limite numerico excedido.", "Falha de acesso ao banco/arquivo."};
    if (result == WORKFORCE_IO_ERROR)
    {
        char text[512];
        snprintf(text, sizeof(text), "%s %s", messages[result], store_error(vm->store));
        (void)publish(vm, "status", text);
    }
    else
    {
        (void)publish(vm, "status", messages[result]);
    }
    return result == WORKFORCE_OK;
}
/* Presentation adapters consume snapshots only, with no payroll calculation.
 * The bounded on-screen preview does not limit reports exported to CSV. */
static bool stream_property(WorkforceViewModel *vm, const char *name, FILE *file)
{
    char *text;
    size_t length;
    bool ok;
    if (file == NULL)
    {
        return false;
    }
    if (fflush(file) != 0 || ferror(file) || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    text = malloc(65536);
    if (text == NULL)
    {
        fclose(file);
        return false;
    }
    length = fread(text, 1, 65000, file);
    if (!feof(file))
    {
        /* End at a newline so a UTF-8 character is never split. */
        while (length > 0 && text[length - 1] != '\n')
        {
            --length;
        }
        const char *notice = "\n[Previa limitada; exporte o relatorio completo em CSV.]\n";
        memcpy(text + length, notice, strlen(notice));
        length += strlen(notice);
    }
    text[length] = '\0';
    ok = !ferror(file) && publish(vm, name, text);
    free(text);
    if (fclose(file) != 0)
    {
        ok = false;
    }
    return ok;
}
static bool worker_tables(WorkforceViewModel *vm, const Worker *worker)
{
    FILE *file = tmpfile();
    if (file == NULL)
    {
        return false;
    }
    fputs("ID | Competencia | Horas | Valor/hora\n", file);
    for (size_t i = 0; i < worker->contract_count; ++i)
    {
        const HourContract *c = &worker->contracts[i];
        fprintf(file, "%" PRId64 " | %04d-%02d | %d | %" PRId64 ".%02" PRId64 "\n", c->id, c->year,
                c->month, c->hours, c->hourly_rate_cents / 100, c->hourly_rate_cents % 100);
    }
    if (!stream_property(vm, "contracts", file) || (file = tmpfile()) == NULL)
    {
        return false;
    }
    fprintf(file, "Salario inicial: %" PRId64 ".%02" PRId64 "\nCompetencia | Salario vigente\n",
            worker->profile.salary_cents / 100, worker->profile.salary_cents % 100);
    for (size_t i = 0; i < worker->salary_count; ++i)
    {
        const SalaryChange *s = &worker->salary_changes[i];
        fprintf(file, "%04d-%02d | %" PRId64 ".%02" PRId64 "\n", s->year, s->month,
                s->salary_cents / 100, s->salary_cents % 100);
    }
    return stream_property(vm, "history", file);
}
static bool refresh(WorkforceViewModel *vm)
{
    Team team = {0};
    WorkforceStatus result = store_list(vm->store, &team);
    FILE *file;
    if (result != WORKFORCE_OK)
    {
        (void)publish(vm, "workers", "Consulta indisponivel; atualize novamente.");
        return status(vm, result);
    }
    file = tmpfile();
    if (file == NULL)
    {
        team_dispose(&team);
        return false;
    }
    fputs("ID | Nome | Idade | Cargo | Departamento | Nivel\n", file);
    for (size_t i = 0; i < team.count; ++i)
    {
        const Worker *w = &team.workers[i];
        fprintf(file, "%" PRIu32 " | %s | %d | %s | %s | %s\n", w->id, w->profile.name,
                w->profile.age, w->profile.profession, w->profile.department,
                worker_level_name(w->profile.level));
    }
    team_dispose(&team);
    return stream_property(vm, "workers", file);
}
static WorkforceStatus find(WorkforceViewModel *vm, Worker *worker)
{
    int64_t id;
    return number(vm, "worker.id", UINT32_MAX, &id) ? store_find(vm->store, (uint32_t)id, worker)
                                                    : WORKFORCE_INVALID;
}
static bool load_worker(WorkforceViewModel *vm, const Worker *worker)
{
    char selection[160];
    (void)publish(vm, "worker.name", worker->profile.name);
    (void)publish(vm, "worker.profession", worker->profile.profession);
    (void)publish(vm, "worker.department", worker->profile.department);
    (void)publish(vm, "worker.level", worker_level_name(worker->profile.level));
    integer(vm, "worker.age", worker->profile.age);
    money_field(vm, "worker.salary", worker->profile.salary_cents);
    snprintf(selection, sizeof(selection), "Funcionario %" PRIu32 ": %s", worker->id,
             worker->profile.name);
    (void)publish(vm, "selection", selection);
    return worker_tables(vm, worker);
}
static WorkforceStatus profile(WorkforceViewModel *vm, WorkerProfile *out, bool create)
{
    int64_t age;
    if (!copy_field(vm, "worker.name", out->name) ||
        !copy_field(vm, "worker.profession", out->profession) ||
        !copy_field(vm, "worker.department", out->department) ||
        !number(vm, "worker.age", INT_MAX, &age) ||
        worker_level_parse(get(vm, "worker.level"), &out->level) != WORKFORCE_OK ||
        (create && !money_parse(get(vm, "worker.salary"), &out->salary_cents)))
    {
        return WORKFORCE_INVALID;
    }
    out->age = (int)age;
    return WORKFORCE_OK;
}
static WorkforceStatus contract_action(WorkforceViewModel *vm, const Worker *w, Action action)
{
    int64_t id = 0, year, month, hours;
    HourContract c = {0};
    if (action != A_CONTRACT_ADD && !number(vm, "contract.id", INT64_MAX, &id))
    {
        return WORKFORCE_INVALID;
    }
    if (action == A_CONTRACT_LOAD)
    {
        const HourContract *found = worker_find_contract(w, id);
        if (found == NULL)
        {
            return WORKFORCE_NOT_FOUND;
        }
        integer(vm, "contract.year", found->year);
        integer(vm, "contract.month", found->month);
        integer(vm, "contract.hours", found->hours);
        money_field(vm, "contract.rate", found->hourly_rate_cents);
        return WORKFORCE_OK;
    }
    if (action == A_CONTRACT_DELETE)
    {
        return store_remove_contract(vm->store, w->id, id);
    }
    if (!number(vm, "contract.year", INT_MAX, &year) ||
        !number(vm, "contract.month", INT_MAX, &month) ||
        !number(vm, "contract.hours", INT_MAX, &hours) ||
        !money_parse(get(vm, "contract.rate"), &c.hourly_rate_cents))
    {
        return WORKFORCE_INVALID;
    }
    c.year = (int)year;
    c.month = (int)month;
    c.hours = (int)hours;
    WorkforceStatus result = action == A_CONTRACT_ADD
                                 ? store_add_contract(vm->store, w->id, c, &id)
                                 : store_update_contract(vm->store, w->id, id, c);
    if (result == WORKFORCE_OK)
    {
        integer(vm, "contract.id", id);
    }
    return result;
}
static WorkforceStatus salary_action(WorkforceViewModel *vm, uint32_t id, Action action)
{
    SalaryChange change = {0};
    int64_t year, month;
    if (!number(vm, "salary.year", INT_MAX, &year) ||
        !number(vm, "salary.month", INT_MAX, &month) ||
        (action == A_SALARY_SAVE && !money_parse(get(vm, "salary.amount"), &change.salary_cents)))
    {
        return WORKFORCE_INVALID;
    }
    change.year = (int)year;
    change.month = (int)month;
    return action == A_SALARY_SAVE ? store_set_salary(vm->store, id, change)
                                   : store_remove_salary(vm->store, id, change.year, change.month);
}
static WorkforceStatus report_action(WorkforceViewModel *vm, Action action)
{
    int64_t year, month;
    Team team = {0};
    ReportFilter filter = {0};
    PayrollReport report = {0};
    const char *department = get(vm, "report.department"), *level = get(vm, "report.level");
    WorkforceStatus result;
    (void)publish(vm, "report", "Consulta/exportacao nao concluida; confira o status.");
    if (!number(vm, "report.year", INT_MAX, &year) ||
        !number(vm, "report.month", INT_MAX, &month) ||
        (strcmp(department, "*") != 0 && *department != '\0' &&
         !copy_field(vm, "report.department", filter.department)))
    {
        return WORKFORCE_INVALID;
    }
    if (*level != '\0' && strcmp(level, "*") != 0)
    {
        if (worker_level_parse(level, &filter.level) != WORKFORCE_OK)
        {
            return WORKFORCE_INVALID;
        }
        filter.has_level = true;
    }
    result = store_list(vm->store, &team);
    if (result == WORKFORCE_OK)
    {
        result = report_build(&team, (int)year, (int)month, &filter, &report);
    }
    team_dispose(&team);
    if (result == WORKFORCE_OK)
    {
        FILE *file = tmpfile();
        if (file == NULL)
        {
            result = WORKFORCE_IO_ERROR;
        }
        else
        {
            result = report_write_text(file, &report);
            if (result != WORKFORCE_OK)
            {
                fclose(file);
            }
            else if (!stream_property(vm, "report", file))
            {
                result = WORKFORCE_IO_ERROR;
            }
        }
    }
    if (result == WORKFORCE_OK && action == A_EXPORT)
    {
        result = report_export_csv(get(vm, "report.path"), &report);
    }
    report_dispose(&report);
    if (result != WORKFORCE_OK)
    {
        (void)publish(vm, "report", "Consulta/exportacao nao concluida; confira o status.");
    }
    return result;
}
static bool execute(void *context)
{
    CommandContext *command = context;
    WorkforceViewModel *vm = command->vm;
    Action action = command->action;
    WorkforceStatus result = WORKFORCE_OK;
    Worker worker = {0};
    if (action == A_LIST)
    {
        return refresh(vm) ? status(vm, WORKFORCE_OK) : false;
    }
    if (action == A_REPORT || action == A_EXPORT)
    {
        return status(vm, report_action(vm, action));
    }
    if (action == A_CREATE)
    {
        WorkerProfile p = {0};
        uint32_t id;
        result = profile(vm, &p, true);
        if (result == WORKFORCE_OK)
        {
            result = store_add_worker(vm->store, &p, &id);
            if (result == WORKFORCE_OK)
            {
                integer(vm, "worker.id", id);
            }
        }
    }
    else
    {
        result = find(vm, &worker);
        if (result == WORKFORCE_OK)
        {
            if (action == A_LOAD)
            {
                result = load_worker(vm, &worker) ? WORKFORCE_OK : WORKFORCE_IO_ERROR;
            }
            else if (action == A_SAVE)
            {
                WorkerProfile p = worker.profile;
                result = profile(vm, &p, false);
                if (result == WORKFORCE_OK)
                {
                    result = store_update_profile(vm->store, worker.id, &p);
                }
            }
            else if (action == A_DELETE)
            {
                result = store_remove_worker(vm->store, worker.id);
            }
            else if (action >= A_CONTRACT_LOAD && action <= A_CONTRACT_DELETE)
            {
                result = contract_action(vm, &worker, action);
            }
            else
            {
                result = salary_action(vm, worker.id, action);
            }
        }
    }
    worker_dispose(&worker);
    if (!status(vm, result))
    {
        if (action == A_LOAD)
        {
            (void)publish(vm, "selection", "Funcionario nao carregado; confira o ID e o status.");
            (void)publish(vm, "contracts", "");
            (void)publish(vm, "history", "");
        }
        return false;
    }
    /* Invalidate snapshots after a write so stale report totals are not presented. */
    if (action != A_LOAD && action != A_CONTRACT_LOAD)
    {
        (void)publish(vm, "report", "Gere o relatorio para consultar os dados atuais.");
    }
    if (!refresh(vm))
    {
        (void)publish(vm, "status", "Operacao concluida; falha ao atualizar a visualizacao.");
        return false;
    }
    if (action == A_DELETE)
    {
        (void)publish(vm, "selection", "Nenhum funcionario carregado.");
        (void)publish(vm, "contracts", "");
        (void)publish(vm, "history", "");
        (void)publish(vm, "worker.id", "");
    }
    else
    {
        Worker fresh = {0};
        result = find(vm, &fresh);
        bool ok = result == WORKFORCE_OK && worker_tables(vm, &fresh);
        if (ok)
        {
            char selected[160];
            snprintf(selected, sizeof(selected), "Funcionario %" PRIu32 ": %s", fresh.id,
                     fresh.profile.name);
            (void)publish(vm, "selection", selected);
        }
        worker_dispose(&fresh);
        if (!ok)
        {
            (void)publish(vm, "contracts", "Consulta indisponivel.");
            (void)publish(vm, "history", "Consulta indisponivel.");
            return status(vm, result == WORKFORCE_OK ? WORKFORCE_IO_ERROR : result);
        }
    }
    return true;
}
WorkforceViewModel *workforce_vm_create(WorkforceStore *store)
{
    static const struct
    {
        const char *name, *value;
        bool writable;
    } properties[] = {{"worker.id", "", true},
                      {"worker.name", "", true},
                      {"worker.age", "", true},
                      {"worker.profession", "", true},
                      {"worker.department", "", true},
                      {"worker.level", "Junior", true},
                      {"worker.salary", "0.00", true},
                      {"contract.id", "", true},
                      {"contract.year", "2026", true},
                      {"contract.month", "1", true},
                      {"contract.hours", "1", true},
                      {"contract.rate", "0.00", true},
                      {"salary.year", "2026", true},
                      {"salary.month", "1", true},
                      {"salary.amount", "0.00", true},
                      {"report.year", "2026", true},
                      {"report.month", "1", true},
                      {"report.department", "*", true},
                      {"report.level", "*", true},
                      {"report.path", "report.csv", true},
                      {"workers", "", false},
                      {"contracts", "", false},
                      {"history", "", false},
                      {"report", "Gere um relatorio.", false},
                      {"selection", "Informe um ID e carregue o funcionario.", false},
                      {"status", "Pronto.", false}};
    static const char *const commands[] = {
        "worker.list",   "worker.create", "worker.load",   "worker.save",     "worker.delete",
        "contract.load", "contract.add",  "contract.save", "contract.delete", "salary.save",
        "salary.delete", "report.build",  "report.export"};
    WorkforceViewModel *vm;
    if (store == NULL || (vm = calloc(1, sizeof(*vm))) == NULL)
    {
        return NULL;
    }
    vm->store = store;
    vm->model = mvvm_create();
    if (vm->model == NULL)
    {
        free(vm);
        return NULL;
    }
    for (size_t i = 0; i < sizeof(properties) / sizeof(properties[0]); ++i)
    {
        if (!mvvm_property(vm->model, properties[i].name, properties[i].value,
                           properties[i].writable))
        {
            workforce_vm_destroy(vm);
            return NULL;
        }
    }
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
    {
        vm->commands[i] = (CommandContext){vm, (Action)i};
        if (!mvvm_command(vm->model, commands[i], execute, &vm->commands[i]))
        {
            workforce_vm_destroy(vm);
            return NULL;
        }
    }
    if (!refresh(vm))
    {
        workforce_vm_destroy(vm);
        return NULL;
    }
    return vm;
}
Mvvm *workforce_vm_bindings(WorkforceViewModel *vm)
{
    return vm == NULL ? NULL : vm->model;
}
void workforce_vm_destroy(WorkforceViewModel *vm)
{
    if (vm != NULL)
    {
        mvvm_destroy(vm->model);
        free(vm);
    }
}
