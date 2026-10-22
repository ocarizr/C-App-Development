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
    A_EXPORT,
    A_COUNT
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
    CommandContext commands[A_COUNT];
};

enum
{
    PREVIEW_BYTES = 65000
};

static const char PREVIEW_TRUNCATED[] =
    "\n[Previa limitada; exporte o relatorio completo em CSV.]\n";

_Static_assert(PREVIEW_BYTES + sizeof(PREVIEW_TRUNCATED) <= MVVM_VALUE_MAX + 1,
               "A truncated preview must fit the binding, including its terminator");

static const char *binding_value(WorkforceViewModel *vm, const char *name)
{
    return mvvm_get(vm->model, name);
}

static bool publish(WorkforceViewModel *vm, const char *name, const char *value)
{
    return mvvm_publish(vm->model, name, value);
}

static void publish_integer(WorkforceViewModel *vm, const char *name, int64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%" PRId64, value);
    (void)publish(vm, name, text);
}

static void publish_money(WorkforceViewModel *vm, const char *name, int64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%" PRId64 ".%02" PRId64, value / 100, value % 100);
    (void)publish(vm, name, text);
}

static bool parse_positive_integer(WorkforceViewModel *vm, const char *name, int64_t max,
                                   int64_t *out)
{
    const char *text = binding_value(vm, name);
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

static bool copy_profile_field(WorkforceViewModel *vm, const char *name,
                               char out[WORKFORCE_NAME_SIZE])
{
    const char *text = binding_value(vm, name);
    size_t length = strlen(text);
    if (length == 0 || length >= WORKFORCE_NAME_SIZE)
    {
        return false;
    }

    memcpy(out, text, length + 1);
    return true;
}

static bool publish_status(WorkforceViewModel *vm, WorkforceStatus result)
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

/* Consumes file on every path. The registry copies text before the temporary
 * buffer is freed; the preview limit never limits the exported snapshot. */
static bool publish_stream(WorkforceViewModel *vm, const char *name, FILE *file)
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

    text = malloc(MVVM_VALUE_MAX + 1);
    if (text == NULL)
    {
        fclose(file);
        return false;
    }

    length = fread(text, 1, PREVIEW_BYTES, file);
    if (!feof(file))
    {
        /* End at a newline so a UTF-8 character is never split. */
        while (length > 0 && text[length - 1] != '\n')
        {
            --length;
        }

        memcpy(text + length, PREVIEW_TRUNCATED, sizeof(PREVIEW_TRUNCATED) - 1);
        length += sizeof(PREVIEW_TRUNCATED) - 1;
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

static bool publish_worker_tables(WorkforceViewModel *vm, const Worker *worker)
{
    FILE *file = tmpfile();
    if (file == NULL)
    {
        return false;
    }

    if (worker_write_contracts(file, worker) != WORKFORCE_OK)
    {
        fclose(file);
        return false;
    }

    if (!publish_stream(vm, "contracts", file) || (file = tmpfile()) == NULL)
    {
        return false;
    }

    if (worker_write_history(file, worker) != WORKFORCE_OK)
    {
        fclose(file);
        return false;
    }

    return publish_stream(vm, "history", file);
}

static bool refresh_team(WorkforceViewModel *vm)
{
    Team team = {0};
    WorkforceStatus result = store_list(vm->store, &team);
    FILE *file;
    if (result != WORKFORCE_OK)
    {
        (void)publish(vm, "workers", "Consulta indisponivel; atualize novamente.");
        return publish_status(vm, result);
    }

    file = tmpfile();
    if (file == NULL)
    {
        team_dispose(&team);
        return false;
    }

    result = team_write_text(file, &team);
    if (result != WORKFORCE_OK)
    {
        fclose(file);
        team_dispose(&team);
        return publish_status(vm, result);
    }

    team_dispose(&team);
    return publish_stream(vm, "workers", file);
}

static WorkforceStatus find_worker(WorkforceViewModel *vm, Worker *worker)
{
    int64_t id;
    return parse_positive_integer(vm, "worker.id", UINT32_MAX, &id)
               ? store_find(vm->store, (uint32_t)id, worker)
               : WORKFORCE_INVALID;
}

static bool publish_worker(WorkforceViewModel *vm, const Worker *worker)
{
    char selection[160];
    (void)publish(vm, "worker.name", worker->profile.name);
    (void)publish(vm, "worker.profession", worker->profile.profession);
    (void)publish(vm, "worker.department", worker->profile.department);
    (void)publish(vm, "worker.level", worker_level_name(worker->profile.level));
    publish_integer(vm, "worker.age", worker->profile.age);
    publish_money(vm, "worker.salary", worker->profile.salary_cents);
    snprintf(selection, sizeof(selection), "Funcionario %" PRIu32 ": %s", worker->id,
             worker->profile.name);
    (void)publish(vm, "selection", selection);
    return publish_worker_tables(vm, worker);
}

static WorkforceStatus parse_profile(WorkforceViewModel *vm, WorkerProfile *out, bool create)
{
    int64_t age;
    if (!copy_profile_field(vm, "worker.name", out->name) ||
        !copy_profile_field(vm, "worker.profession", out->profession) ||
        !copy_profile_field(vm, "worker.department", out->department) ||
        !parse_positive_integer(vm, "worker.age", INT_MAX, &age) ||
        worker_level_parse(binding_value(vm, "worker.level"), &out->level) != WORKFORCE_OK ||
        (create && !money_parse(binding_value(vm, "worker.salary"), &out->salary_cents)))
    {
        return WORKFORCE_INVALID;
    }

    out->age = (int)age;
    return WORKFORCE_OK;
}

static WorkforceStatus execute_contract(WorkforceViewModel *vm, const Worker *w, Action action)
{
    int64_t id = 0, year, month, hours;
    HourContract c = {0};
    if (action != A_CONTRACT_ADD && !parse_positive_integer(vm, "contract.id", INT64_MAX, &id))
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

        publish_integer(vm, "contract.year", found->year);
        publish_integer(vm, "contract.month", found->month);
        publish_integer(vm, "contract.hours", found->hours);
        publish_money(vm, "contract.rate", found->hourly_rate_cents);
        return WORKFORCE_OK;
    }

    if (action == A_CONTRACT_DELETE)
    {
        return store_remove_contract(vm->store, w->id, id);
    }

    if (!parse_positive_integer(vm, "contract.year", INT_MAX, &year) ||
        !parse_positive_integer(vm, "contract.month", INT_MAX, &month) ||
        !parse_positive_integer(vm, "contract.hours", INT_MAX, &hours) ||
        !money_parse(binding_value(vm, "contract.rate"), &c.hourly_rate_cents))
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
        publish_integer(vm, "contract.id", id);
    }

    return result;
}

static WorkforceStatus execute_salary(WorkforceViewModel *vm, uint32_t id, Action action)
{
    SalaryChange change = {0};
    int64_t year, month;
    if (!parse_positive_integer(vm, "salary.year", INT_MAX, &year) ||
        !parse_positive_integer(vm, "salary.month", INT_MAX, &month) ||
        (action == A_SALARY_SAVE &&
         !money_parse(binding_value(vm, "salary.amount"), &change.salary_cents)))
    {
        return WORKFORCE_INVALID;
    }

    change.year = (int)year;
    change.month = (int)month;
    return action == A_SALARY_SAVE ? store_set_salary(vm->store, id, change)
                                   : store_remove_salary(vm->store, id, change.year, change.month);
}

static WorkforceStatus execute_report(WorkforceViewModel *vm, Action action)
{
    int64_t year, month;
    Team team = {0};
    ReportFilter filter = {0};
    PayrollReport report = {0};
    const char *department = binding_value(vm, "report.department"),
               *level = binding_value(vm, "report.level");
    WorkforceStatus result;
    (void)publish(vm, "report", "Consulta/exportacao nao concluida; confira o status.");
    if (!parse_positive_integer(vm, "report.year", INT_MAX, &year) ||
        !parse_positive_integer(vm, "report.month", INT_MAX, &month) ||
        (strcmp(department, "*") != 0 && *department != '\0' &&
         !copy_profile_field(vm, "report.department", filter.department)))
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
            else if (!publish_stream(vm, "report", file))
            {
                result = WORKFORCE_IO_ERROR;
            }
        }
    }

    if (result == WORKFORCE_OK && action == A_EXPORT)
    {
        result = report_export_csv(binding_value(vm, "report.path"), &report);
    }

    report_dispose(&report);
    if (result != WORKFORCE_OK)
    {
        (void)publish(vm, "report", "Consulta/exportacao nao concluida; confira o status.");
    }

    return result;
}

static bool execute_command(void *context)
{
    CommandContext *command = context;
    WorkforceViewModel *vm = command->vm;
    Action action = command->action;
    WorkforceStatus result = WORKFORCE_OK;
    Worker worker = {0};
    if (action == A_LIST)
    {
        return refresh_team(vm) ? publish_status(vm, WORKFORCE_OK) : false;
    }

    if (action == A_REPORT || action == A_EXPORT)
    {
        return publish_status(vm, execute_report(vm, action));
    }

    if (action == A_CREATE)
    {
        WorkerProfile p = {0};
        uint32_t id;
        result = parse_profile(vm, &p, true);
        if (result == WORKFORCE_OK)
        {
            result = store_add_worker(vm->store, &p, &id);
            if (result == WORKFORCE_OK)
            {
                publish_integer(vm, "worker.id", id);
            }
        }
    }
    else
    {
        result = find_worker(vm, &worker);
        if (result == WORKFORCE_OK)
        {
            if (action == A_LOAD)
            {
                result = publish_worker(vm, &worker) ? WORKFORCE_OK : WORKFORCE_IO_ERROR;
            }
            else if (action == A_SAVE)
            {
                WorkerProfile p = worker.profile;
                result = parse_profile(vm, &p, false);
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
                result = execute_contract(vm, &worker, action);
            }
            else
            {
                result = execute_salary(vm, worker.id, action);
            }
        }
    }

    worker_dispose(&worker);
    if (!publish_status(vm, result))
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

    if (!refresh_team(vm))
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
        result = find_worker(vm, &fresh);
        bool ok = result == WORKFORCE_OK && publish_worker_tables(vm, &fresh);
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
            return publish_status(vm, result == WORKFORCE_OK ? WORKFORCE_IO_ERROR : result);
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

    static const struct
    {
        const char *name;
        Action action;
    } commands[] = {{"worker.list", A_LIST},
                    {"worker.create", A_CREATE},
                    {"worker.load", A_LOAD},
                    {"worker.save", A_SAVE},
                    {"worker.delete", A_DELETE},
                    {"contract.load", A_CONTRACT_LOAD},
                    {"contract.add", A_CONTRACT_ADD},
                    {"contract.save", A_CONTRACT_SAVE},
                    {"contract.delete", A_CONTRACT_DELETE},
                    {"salary.save", A_SALARY_SAVE},
                    {"salary.delete", A_SALARY_DELETE},
                    {"report.build", A_REPORT},
                    {"report.export", A_EXPORT}};

    _Static_assert(sizeof(commands) / sizeof(commands[0]) == A_COUNT,
                   "Every application action needs an explicit command binding");
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
        vm->commands[i] = (CommandContext){vm, commands[i].action};
        if (!mvvm_command(vm->model, commands[i].name, execute_command, &vm->commands[i]))
        {
            workforce_vm_destroy(vm);
            return NULL;
        }
    }

    if (!refresh_team(vm))
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
