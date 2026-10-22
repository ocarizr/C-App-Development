#include "workforce_report.h"
#include <stdlib.h>
#include <string.h>

static int compare_rows(const void *left, const void *right)
{
    const ReportRow *a = left;
    const ReportRow *b = right;
    int department = strcmp(a->profile.department, b->profile.department);
    if (department != 0)
    {
        return department;
    }
    return (a->worker_id > b->worker_id) - (a->worker_id < b->worker_id);
}

WorkforceStatus report_build(const Team *team, int year, int month, const ReportFilter *filter,
                             PayrollReport *report)
{
    PayrollReport candidate = {0};
    WorkforceStatus status = WORKFORCE_OK;
    if (team == NULL || filter == NULL || report == NULL || year < 1 || year > 9999 || month < 1 ||
        month > 12 || memchr(filter->department, '\0', sizeof(filter->department)) == NULL ||
        (filter->has_level && (filter->level < WORKER_TRAINEE || filter->level > WORKER_SENIOR)))
    {
        return WORKFORCE_INVALID;
    }
    if (team->count > SIZE_MAX / sizeof(*candidate.rows) ||
        team->count > SIZE_MAX / sizeof(*candidate.departments))
    {
        return WORKFORCE_OVERFLOW;
    }
    candidate.year = year;
    candidate.month = month;
    candidate.filter = *filter;
    if (team->count > 0)
    {
        candidate.rows = calloc(team->count, sizeof(*candidate.rows));
        candidate.departments = calloc(team->count, sizeof(*candidate.departments));
        if (candidate.rows == NULL || candidate.departments == NULL)
        {
            report_dispose(&candidate);
            return WORKFORCE_NO_MEMORY;
        }
    }
    for (size_t i = 0; i < team->count; ++i)
    {
        const Worker *worker = &team->workers[i];
        ReportRow *row;
        if ((filter->department[0] != '\0' &&
             strcmp(filter->department, worker->profile.department) != 0) ||
            (filter->has_level && filter->level != worker->profile.level))
        {
            continue;
        }
        row = &candidate.rows[candidate.count];
        row->worker_id = worker->id;
        row->profile = worker->profile;
        status = worker_salary_at(worker, year, month, &row->salary_cents);
        if (status == WORKFORCE_OK)
        {
            status = worker_income(worker, year, month, &row->total_cents);
        }
        if (status != WORKFORCE_OK || candidate.total_cents > INT64_MAX - row->total_cents)
        {
            status = status == WORKFORCE_OK ? WORKFORCE_OVERFLOW : status;
            break;
        }
        row->contracts_cents = row->total_cents - row->salary_cents;
        candidate.salary_cents += row->salary_cents;
        candidate.contracts_cents += row->contracts_cents;
        candidate.total_cents += row->total_cents;
        ++candidate.count;
    }
    if (status != WORKFORCE_OK)
    {
        report_dispose(&candidate);
        return status;
    }
    if (candidate.count > 1)
    {
        qsort(candidate.rows, candidate.count, sizeof(*candidate.rows), compare_rows);
    }
    for (size_t i = 0; i < candidate.count; ++i)
    {
        const ReportRow *row = &candidate.rows[i];
        DepartmentTotal *department;
        if (candidate.department_count == 0 ||
            strcmp(candidate.departments[candidate.department_count - 1].department,
                   row->profile.department) != 0)
        {
            department = &candidate.departments[candidate.department_count++];
            memcpy(department->department, row->profile.department, sizeof(department->department));
        }
        else
        {
            department = &candidate.departments[candidate.department_count - 1];
        }
        ++department->worker_count;
        /* Each subtotal is bounded by the checked nonnegative grand total. */
        department->salary_cents += row->salary_cents;
        department->contracts_cents += row->contracts_cents;
        department->total_cents += row->total_cents;
    }
    *report = candidate;
    return WORKFORCE_OK;
}

void report_dispose(PayrollReport *report)
{
    if (report != NULL)
    {
        free(report->rows);
        free(report->departments);
        *report = (PayrollReport){0};
    }
}
