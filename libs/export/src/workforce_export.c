#include "workforce_export.h"

#include <ctype.h>
#include <inttypes.h>

static int valid_report(const PayrollReport *report)
{
    return report != NULL && report->year >= 1 && report->year <= 9999 && report->month >= 1 &&
           report->month <= 12 && (report->count == 0 || report->rows != NULL) &&
           (report->department_count == 0 || report->departments != NULL);
}

static void money(FILE *output, int64_t cents)
{
    fprintf(output, "%" PRId64 ".%02" PRId64, cents / 100, cents % 100);
}

static WorkforceStatus finish(FILE *output)
{
    return ferror(output) || fflush(output) != 0 ? WORKFORCE_IO_ERROR : WORKFORCE_OK;
}

WorkforceStatus report_write_text(FILE *output, const PayrollReport *report)
{
    if (output == NULL || !valid_report(report))
    {
        return WORKFORCE_INVALID;
    }
    fprintf(output, "Remuneracao de %04d-%02d:\n", report->year, report->month);
    fprintf(output, "Departamento: %s | Nivel: %s\n",
            report->filter.department[0] == '\0' ? "Todos" : report->filter.department,
            report->filter.has_level ? worker_level_name(report->filter.level) : "Todos");
    if (report->count == 0)
    {
        fputs("Nenhum funcionario encontrado para os filtros.\n", output);
    }
    for (size_t i = 0; i < report->count; ++i)
    {
        const ReportRow *row = &report->rows[i];
        fprintf(output, "%s: ", row->profile.name);
        money(output, row->total_cents);
        fprintf(output, " | ID %" PRIu32 " | %s | %s | Salario: ", row->worker_id,
                row->profile.department, worker_level_name(row->profile.level));
        money(output, row->salary_cents);
        fputs(" | Contratos: ", output);
        money(output, row->contracts_cents);
        fputc('\n', output);
    }
    for (size_t i = 0; i < report->department_count; ++i)
    {
        const DepartmentTotal *department = &report->departments[i];
        fprintf(output, "Subtotal %s (%zu funcionarios): ", department->department,
                department->worker_count);
        money(output, department->total_cents);
        fputc('\n', output);
    }
    fputs("Total da equipe: ", output);
    money(output, report->total_cents);
    fputc('\n', output);
    return finish(output);
}

static void csv_text(FILE *output, const char *text)
{
    const char *first = text;
    while (isspace((unsigned char)*first))
    {
        ++first;
    }
    fputc('"', output);
    /* Keep user text from being interpreted as a spreadsheet formula. */
    if (*first == '=' || *first == '+' || *first == '-' || *first == '@')
    {
        fputc('\'', output);
    }
    for (; *text != '\0'; ++text)
    {
        if (*text == '"')
        {
            fputc('"', output);
        }
        fputc(*text, output);
    }
    fputc('"', output);
}

static void csv_amounts(FILE *output, int64_t salary, int64_t contracts, int64_t total)
{
    money(output, salary);
    fputc(',', output);
    money(output, contracts);
    fputc(',', output);
    money(output, total);
    fputs("\r\n", output);
}

WorkforceStatus report_write_csv(FILE *output, const PayrollReport *report)
{
    if (output == NULL || !valid_report(report))
    {
        return WORKFORCE_INVALID;
    }
    fputs("type,year,month,worker_id,name,profession,department,level,worker_count,salary,"
          "contracts,total\r\n",
          output);
    for (size_t i = 0; i < report->count; ++i)
    {
        const ReportRow *row = &report->rows[i];
        fprintf(output, "WORKER,%d,%d,%" PRIu32 ",", report->year, report->month, row->worker_id);
        csv_text(output, row->profile.name);
        fputc(',', output);
        csv_text(output, row->profile.profession);
        fputc(',', output);
        csv_text(output, row->profile.department);
        fputc(',', output);
        csv_text(output, worker_level_name(row->profile.level));
        fputs(",1,", output);
        csv_amounts(output, row->salary_cents, row->contracts_cents, row->total_cents);
    }
    for (size_t i = 0; i < report->department_count; ++i)
    {
        const DepartmentTotal *department = &report->departments[i];
        fprintf(output, "DEPARTMENT,%d,%d,,,,", report->year, report->month);
        csv_text(output, department->department);
        fprintf(output, ",,%zu,", department->worker_count);
        csv_amounts(output, department->salary_cents, department->contracts_cents,
                    department->total_cents);
    }
    fprintf(output, "TOTAL,%d,%d,,,,,,%zu,", report->year, report->month, report->count);
    csv_amounts(output, report->salary_cents, report->contracts_cents, report->total_cents);
    return finish(output);
}

WorkforceStatus report_export_csv(const char *path, const PayrollReport *report)
{
    FILE *file = NULL;
    WorkforceStatus status;
    if (path == NULL || path[0] == '\0' || !valid_report(report))
    {
        return WORKFORCE_INVALID;
    }
#ifdef _MSC_VER
    (void)fopen_s(&file, path, "wbx");
#else
    file = fopen(path, "wbx");
#endif
    if (file == NULL)
    {
        return WORKFORCE_IO_ERROR;
    }
    status = report_write_csv(file, report);
    if (fclose(file) != 0)
    {
        status = WORKFORCE_IO_ERROR;
    }
    if (status != WORKFORCE_OK)
    {
        (void)remove(path);
    }
    return status;
}
