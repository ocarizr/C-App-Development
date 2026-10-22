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
    }
}

static int register_worker(Console *console, Team *team)
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
    status = team_add(team, &profile, &id);
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

static void list_workers(Console *console, const Team *team)
{
    size_t i;
    if (team->count == 0)
    {
        fputs("Nenhum funcionario cadastrado.\n", console->output);
    }
    for (i = 0; i < team->count; ++i)
    {
        const Worker *worker = &team->workers[i];
        size_t j;
        fprintf(console->output,
                "ID %" PRIu32 " | %s | %d anos | %s %s | %s | Salario: ", worker->id,
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
}

static int manage_worker(Console *console, Team *team, int option)
{
    int64_t id;
    Worker *worker;
    if (!read_number(console, "ID do funcionario: ", 1, UINT32_MAX, &id))
    {
        return 0;
    }
    worker = team_find(team, (uint32_t)id);
    if (worker == NULL)
    {
        print_status(console, WORKFORCE_NOT_FOUND);
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
        print_status(console, worker_add_contract(worker, contract));
    }
    else if (option == 4)
    {
        int64_t number;
        if (!read_number(console, "Numero do contrato (consulte a listagem): ", 1, INT_MAX,
                         &number))
        {
            return 0;
        }
        print_status(console, worker_remove_contract(worker, (size_t)(number - 1)));
    }
    else
    {
        print_status(console, team_remove(team, (uint32_t)id));
    }
    return 1;
}

static int payroll(Console *console, const Team *team)
{
    int64_t year, month, total = 0;
    size_t i;
    if (!read_number(console, "Ano: ", 1, 9999, &year) ||
        !read_number(console, "Mes: ", 1, 12, &month))
    {
        return 0;
    }
    /* Check the full team before printing a partial payroll. */
    for (i = 0; i < team->count; ++i)
    {
        int64_t income;
        WorkforceStatus status = worker_income(&team->workers[i], (int)year, (int)month, &income);
        if (status != WORKFORCE_OK || total > INT64_MAX - income)
        {
            print_status(console, status == WORKFORCE_OK ? WORKFORCE_OVERFLOW : status);
            return 1;
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
    return 1;
}

int console_run(FILE *input, FILE *output)
{
    Team team = {0};
    Console console = {input, output};
    int running = 1;
    if (input == NULL || output == NULL)
    {
        return EXIT_FAILURE;
    }
    fputs("Workforce - gerenciamento de equipe\nDados mantidos apenas durante esta sessao.\n",
          output);
    while (running)
    {
        int64_t option;
        fputs("\n1 Cadastrar funcionario\n2 Listar equipe e contratos\n3 Adicionar contrato\n"
              "4 Remover contrato\n5 Consultar remuneracao mensal\n6 Remover funcionario\n0 Sair\n",
              output);
        if (!read_number(&console, "Opcao: ", 0, 6, &option))
        {
            break;
        }
        switch (option)
        {
        case 0:
            running = 0;
            break;
        case 1:
            running = register_worker(&console, &team);
            break;
        case 2:
            list_workers(&console, &team);
            break;
        case 3:
        case 4:
        case 6:
            running = manage_worker(&console, &team, (int)option);
            break;
        case 5:
            running = payroll(&console, &team);
            break;
        }
    }
    team_dispose(&team);
    fputs("Sessao encerrada.\n", output);
    return ferror(input) || ferror(output) ? EXIT_FAILURE : EXIT_SUCCESS;
}
