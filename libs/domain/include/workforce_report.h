#ifndef WORKFORCE_REPORT_H
#define WORKFORCE_REPORT_H

#include "workforce.h"

#include <stdbool.h>

typedef struct
{
    char department[WORKFORCE_NAME_SIZE]; /* Empty matches all; otherwise exact, case-sensitive. */
    bool has_level;
    WorkerLevel level;
} ReportFilter;

typedef struct
{
    uint32_t worker_id;
    WorkerProfile profile; /* Current profile; effective salary is salary_cents below. */
    int64_t salary_cents;
    int64_t contracts_cents;
    int64_t total_cents;
} ReportRow;

typedef struct
{
    char department[WORKFORCE_NAME_SIZE];
    size_t worker_count;
    int64_t salary_cents;
    int64_t contracts_cents;
    int64_t total_cents;
} DepartmentTotal;

/* Owns its arrays; independent of the source Team lifetime. Do not shallow-copy.
 * Rows sort by department, then worker ID. Totals include only matching workers. */
typedef struct
{
    int year;
    int month;
    ReportFilter filter;
    ReportRow *rows;
    size_t count;
    DepartmentTotal *departments;
    size_t department_count;
    int64_t salary_cents;
    int64_t contracts_cents;
    int64_t total_cents;
} PayrollReport;

/* Output is fresh/zero-initialized storage and remains unchanged on failure. */
WorkforceStatus report_build(const Team *team, int year, int month, const ReportFilter *filter,
                             PayrollReport *report);
void report_dispose(PayrollReport *report);

#endif
