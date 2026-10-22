#include "console.h"
#include "money.h"
#include "workforce.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    FILE *input;
    FILE *output;
    WorkforceStore *store;
} Console;

static int read_text(Console *console, const char *prompt, char *text, size_t capacity)
{
    char line[256];
    for (;;)
    {
        char *start;
        size_t length;
        fputs(prompt, console->output);
        fflush(console->output);
        if (fgets(line, (int)sizeof(line), console->input) == NULL)
        {
            return 0;
        }
        length = strlen(line);
        if (length > 0 && line[length - 1] != '\n')
        {
            int next = fgetc(console->input);
            if (next != '\n' && next != EOF)
            {
                while ((next = fgetc(console->input)) != '\n' && next != EOF)
                {
                    /* Discard the entire overlong line before retrying. */
                }
                fputs("Entrada longa demais.\n", console->output);
                continue;
            }
        }
        start = line;
        while (isspace((unsigned char)*start))
        {
            ++start;
        }
        length = strlen(start);
        while (length > 0 && isspace((unsigned char)start[length - 1]))
        {
            start[--length] = '\0';
        }
        if (length == 0 || length >= capacity)
        {
            fputs("Texto vazio ou longo demais.\n", console->output);
            continue;
        }
        memcpy(text, start, length + 1);
        return 1;
    }
}

static int read_number(Console *console, const char *prompt, int64_t min, int64_t max,
                       int64_t *value)
{
    char text[256];
    while (read_text(console, prompt, text, sizeof(text)))
    {
        char *end;
        int64_t parsed;
        errno = 0;
        parsed = strtoll(text, &end, 10);
        if (errno == 0 && end != text && *end == '\0' && parsed >= min && parsed <= max)
        {
            *value = parsed;
            return 1;
        }
        fprintf(console->output, "Informe um inteiro entre %" PRId64 " e %" PRId64 ".\n", min, max);
    }
    return 0;
}

static int read_money(Console *console, const char *prompt, int64_t *cents)
{
    char text[256];
    while (read_text(console, prompt, text, sizeof(text)))
    {
        if (money_parse(text, cents))
        {
            return 1;
        }
        fputs("Valor invalido. Use, por exemplo, 1250.50 (sem separador de milhar).\n",
              console->output);
    }
    return 0;
}

static void print_money(FILE *output, int64_t cents)
{
    fprintf(output, "%" PRId64 ".%02" PRId64, cents / 100, cents % 100);
}

static void print_status(Console *console, WorkforceStatus status)
{
    switch (status)
    {
    case WORKFORCE_OK:
        fputs("Operacao concluida.\n", console->output);
        break;
    case WORKFORCE_INVALID:
        fputs("Dados invalidos.\n", console->output);
        break;
    case WORKFORCE_NOT_FOUND:
        fputs("Registro nao encontrado.\n", console->output);
        break;
    case WORKFORCE_NO_MEMORY:
        fputs("Memoria insuficiente.\n", console->output);
        break;
    case WORKFORCE_OVERFLOW:
        fputs("Limite numerico excedido.\n", console->output);
        break;
    case WORKFORCE_IO_ERROR:
        fprintf(console->output, "Erro de banco de dados: %s\n", store_error(console->store));
        break;
    }
}

static int register_worker(Console *console)
{
    WorkerProfile profile = {0};
    int64_t age;
    char level[WORKFORCE_NAME_SIZE];
    uint32_t id;
    WorkforceStatus status;
    if (!read_text(console, "Nome: ", profile.name, sizeof(profile.name)) ||
        !read_number(console, "Idade (1-150): ", 1, 150, &age) ||
        !read_text(console, "Cargo: ", profile.profession, sizeof(profile.profession)) ||
        !read_text(console, "Departamento: ", profile.department, sizeof(profile.department)))
    {
        return 0;
    }
    profile.age = (int)age;
    for (;;)
    {
        if (!read_text(console, "Nivel (Trainee/Junior/MidLevel/Senior): ", level, sizeof(level)))
        {
            return 0;
        }
        if (worker_level_parse(level, &profile.level) == WORKFORCE_OK)
        {
            break;
        }
        fputs("Nivel invalido.\n", console->output);
    }
    if (!read_money(console, "Salario mensal: ", &profile.salary_cents))
    {
        return 0;
    }
    status = store_add_worker(console->store, &profile, &id);
    if (status == WORKFORCE_OK)
    {
        fprintf(console->output, "Funcionario cadastrado. ID: %" PRIu32 "\n", id);
    }
    else
    {
        print_status(console, status);
    }
    return 1;
}

static void print_worker(Console *console, const Worker *worker)
{
    size_t j;
    fprintf(console->output,
            "ID %" PRIu32 " | %s | %d anos | %s %s | %s | Salario inicial: ", worker->id,
            worker->profile.name, worker->profile.age, worker_level_name(worker->profile.level),
            worker->profile.profession, worker->profile.department);
    print_money(console->output, worker->profile.salary_cents);
    fputc('\n', console->output);
    for (j = 0; j < worker->contract_count; ++j)
    {
        const HourContract *contract = &worker->contracts[j];
        fprintf(console->output, "  Contrato %zu | %04d-%02d | %d horas | Valor/hora: ", j + 1,
                contract->year, contract->month, contract->hours);
        print_money(console->output, contract->hourly_rate_cents);
        fputc('\n', console->output);
    }
}

static void list_workers(Console *console, const Team *team)
{
    if (team->count == 0)
    {
        fputs("Nenhum funcionario cadastrado.\n", console->output);
    }
    for (size_t i = 0; i < team->count; ++i)
    {
        print_worker(console, &team->workers[i]);
    }
}

static void print_history(Console *console, const Worker *worker)
{
    fputs("Historico salarial (vigencia a partir da competencia):\n", console->output);
    fputs("  Antes da primeira alteracao: ", console->output);
    print_money(console->output, worker->profile.salary_cents);
    fputc('\n', console->output);
    if (worker->salary_count == 0)
    {
        fputs("  Nenhuma alteracao salarial cadastrada.\n", console->output);
    }
    for (size_t i = 0; i < worker->salary_count; ++i)
    {
        const SalaryChange *change = &worker->salary_changes[i];
        fprintf(console->output, "  %04d-%02d: ", change->year, change->month);
        print_money(console->output, change->salary_cents);
        fputc('\n', console->output);
    }
}

static int edit_profile(Console *console, const Worker *worker)
{
    WorkerProfile profile = worker->profile;
    int64_t field;
    int64_t age;
    char level[WORKFORCE_NAME_SIZE];
    fputs("Campo: 1 Nome | 2 Idade | 3 Cargo | 4 Departamento | 5 Nivel | 0 Cancelar\n",
          console->output);
    if (!read_number(console, "Campo: ", 0, 5, &field))
    {
        return 0;
    }
    switch (field)
    {
    case 0:
        return 1;
    case 1:
        if (!read_text(console, "Novo nome: ", profile.name, sizeof(profile.name)))
        {
            return 0;
        }
        break;
    case 2:
        if (!read_number(console, "Nova idade: ", 1, 150, &age))
        {
            return 0;
        }
        profile.age = (int)age;
        break;
    case 3:
        if (!read_text(console, "Novo cargo: ", profile.profession, sizeof(profile.profession)))
        {
            return 0;
        }
        break;
    case 4:
        if (!read_text(console, "Novo departamento: ", profile.department,
                       sizeof(profile.department)))
        {
            return 0;
        }
        break;
    case 5:
        for (;;)
        {
            if (!read_text(console, "Novo nivel (Trainee/Junior/MidLevel/Senior): ", level,
                           sizeof(level)))
            {
                return 0;
            }
            if (worker_level_parse(level, &profile.level) == WORKFORCE_OK)
            {
                break;
            }
            fputs("Nivel invalido.\n", console->output);
        }
        break;
    }
    print_status(console, store_update_profile(console->store, worker->id, &profile));
    return 1;
}

static int history_action(Console *console, int option)
{
    int64_t id;
    int running = 1;
    Worker worker = {0};
    WorkforceStatus status;
    if (!read_number(console, "ID do funcionario: ", 1, UINT32_MAX, &id))
    {
        return 0;
    }
    status = store_find(console->store, (uint32_t)id, &worker);
    if (status != WORKFORCE_OK)
    {
        print_status(console, status);
        return 1;
    }
    print_worker(console, &worker);
    if (option == 8)
    {
        running = edit_profile(console, &worker);
    }
    else
    {
        print_history(console, &worker);
        if (option == 9 || option == 10)
        {
            int64_t year, month;
            SalaryChange change = {0};
            if (!read_number(console, "Ano de vigencia: ", 1, 9999, &year) ||
                !read_number(console, "Mes de vigencia: ", 1, 12, &month) ||
                (option == 9 && !read_money(console, "Salario a partir desta competencia: ",
                                            &change.salary_cents)))
            {
                running = 0;
            }
            else
            {
                change.year = (int)year;
                change.month = (int)month;
                status = option == 9 ? store_set_salary(console->store, worker.id, change)
                                     : store_remove_salary(console->store, worker.id, change.year,
                                                           change.month);
                print_status(console, status);
            }
        }
    }
    worker_dispose(&worker);
    return running;
}

static int manage_worker(Console *console, int option)
{
    int64_t id;
    Worker worker = {0};
    WorkforceStatus status;
    if (!read_number(console, "ID do funcionario: ", 1, UINT32_MAX, &id))
    {
        return 0;
    }
    status = store_find(console->store, (uint32_t)id, &worker);
    worker_dispose(&worker);
    if (status != WORKFORCE_OK)
    {
        print_status(console, status);
        return 1;
    }
    if (option == 3)
    {
        HourContract contract;
        int64_t year, month, hours;
        if (!read_number(console, "Ano: ", 1, 9999, &year) ||
            !read_number(console, "Mes: ", 1, 12, &month) ||
            !read_number(console, "Horas: ", 1, INT_MAX, &hours) ||
            !read_money(console, "Valor por hora: ", &contract.hourly_rate_cents))
        {
            return 0;
        }
        contract.year = (int)year;
        contract.month = (int)month;
        contract.hours = (int)hours;
        print_status(console, store_add_contract(console->store, (uint32_t)id, contract));
    }
    else if (option == 4)
    {
        int64_t number;
        if (!read_number(console, "Numero do contrato (consulte a listagem): ", 1, INT_MAX,
                         &number))
        {
            return 0;
        }
        print_status(console,
                     store_remove_contract(console->store, (uint32_t)id, (size_t)(number - 1)));
    }
    else
    {
        print_status(console, store_remove_worker(console->store, (uint32_t)id));
    }
    return 1;
}

static void print_payroll(Console *console, const Team *team, int year, int month)
{
    int64_t total = 0;
    size_t i;
    /* Check the full team before printing a partial payroll. */
    for (i = 0; i < team->count; ++i)
    {
        int64_t income;
        WorkforceStatus status = worker_income(&team->workers[i], (int)year, (int)month, &income);
        if (status != WORKFORCE_OK || total > INT64_MAX - income)
        {
            print_status(console, status == WORKFORCE_OK ? WORKFORCE_OVERFLOW : status);
            return;
        }
        total += income;
    }
    fprintf(console->output, "Remuneracao de %04d-%02d:\n", (int)year, (int)month);
    for (i = 0; i < team->count; ++i)
    {
        int64_t income = 0;
        (void)worker_income(&team->workers[i], (int)year, (int)month, &income);
        fprintf(console->output, "%s: ", team->workers[i].profile.name);
        print_money(console->output, income);
        fputc('\n', console->output);
    }
    fputs("Total da equipe: ", console->output);
    print_money(console->output, total);
    fputc('\n', console->output);
}

static int payroll(Console *console)
{
    Team team = {0};
    int64_t year, month;
    WorkforceStatus status;
    if (!read_number(console, "Ano: ", 1, 9999, &year) ||
        !read_number(console, "Mes: ", 1, 12, &month))
    {
        return 0;
    }
    status = store_list(console->store, &team);
    if (status == WORKFORCE_OK)
    {
        print_payroll(console, &team, (int)year, (int)month);
    }
    else
    {
        print_status(console, status);
    }
    team_dispose(&team);
    return 1;
}

int console_run(FILE *input, FILE *output, WorkforceStore *store)
{
    Console console = {input, output, store};
    int running = 1;
    if (input == NULL || output == NULL || store == NULL)
    {
        return EXIT_FAILURE;
    }
    fputs("Workforce - gerenciamento de equipe\nDados persistidos em SQLite.\n", output);
    while (running)
    {
        int64_t option;
        fputs(
            "\n1 Cadastrar funcionario\n2 Listar equipe e contratos\n3 Adicionar contrato\n"
            "4 Remover contrato\n5 Consultar remuneracao mensal\n6 Remover funcionario\n"
            "7 Consultar funcionario e historico\n8 Editar perfil\n"
            "9 Cadastrar/corrigir salario por competencia\n10 Remover vigencia salarial\n0 Sair\n",
            output);
        if (!read_number(&console, "Opcao: ", 0, 10, &option))
        {
            break;
        }
        switch (option)
        {
        case 0:
            running = 0;
            break;
        case 1:
            running = register_worker(&console);
            break;
        case 2:
        {
            Team team = {0};
            WorkforceStatus status = store_list(store, &team);
            if (status == WORKFORCE_OK)
            {
                list_workers(&console, &team);
            }
            else
            {
                print_status(&console, status);
            }
            team_dispose(&team);
            break;
        }
        case 3:
        case 4:
        case 6:
            running = manage_worker(&console, (int)option);
            break;
        case 5:
            running = payroll(&console);
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            running = history_action(&console, (int)option);
            break;
        }
    }
    fputs("Sessao encerrada.\n", output);
    return ferror(input) || ferror(output) ? EXIT_FAILURE : EXIT_SUCCESS;
}
