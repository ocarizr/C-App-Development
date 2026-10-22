#include "test.h"
#include "workforce_report.h"
#include <string.h>

static WorkerProfile profile(const char *name, const char *department, WorkerLevel level)
{
    WorkerProfile data = {"", "Developer", "", 30, WORKER_JUNIOR, 300000};
    memcpy(data.name, name, strlen(name) + 1);
    memcpy(data.department, department, strlen(department) + 1);
    data.level = level;
    return data;
}

static void test_report(void)
{
    Team team = {0};
    PayrollReport report = {0};
    ReportFilter filter = {0};
    WorkerProfile data = profile("Ana", "Engineering", WORKER_SENIOR);
    uint32_t ana, bruno, clara;
    CHECK(team_add(&team, &data, &ana) == WORKFORCE_OK);
    data = profile("Bruno", "Engineering", WORKER_JUNIOR);
    CHECK(team_add(&team, &data, &bruno) == WORKFORCE_OK);
    data = profile("Clara", "Design", WORKER_SENIOR);
    CHECK(team_add(&team, &data, &clara) == WORKFORCE_OK);
    CHECK(worker_set_salary(team_find(&team, ana), (SalaryChange){2026, 10, 400000}) ==
          WORKFORCE_OK);
    CHECK(worker_add_contract(team_find(&team, ana), (HourContract){2026, 10, 10, 1000, 0}) ==
          WORKFORCE_OK);
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK);
    CHECK(report.count == 3 && report.department_count == 2 && report.total_cents == 1010000);
    CHECK(strcmp(report.departments[0].department, "Design") == 0);
    CHECK(report.rows[0].worker_id == clara && report.rows[1].worker_id == ana);
    CHECK(report.departments[1].worker_count == 2 && report.departments[1].total_cents == 710000);
    CHECK(report.salary_cents == 1000000 && report.contracts_cents == 10000);
    report_dispose(&report);
    CHECK(report_build(&team, 2026, 9, &filter, &report) == WORKFORCE_OK &&
          report.total_cents == 900000);
    report_dispose(&report);
    memcpy(filter.department, "Engineering", sizeof("Engineering"));
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK && report.count == 2);
    report_dispose(&report);
    filter.department[0] = '\0';
    filter.has_level = true;
    filter.level = WORKER_SENIOR;
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK && report.count == 2);
    report_dispose(&report);
    memcpy(filter.department, "engineering", sizeof("engineering"));
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK && report.count == 0);
    report_dispose(&report);
    memcpy(filter.department, "Engineering", sizeof("Engineering"));
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK);
    CHECK(report.count == 1 && report.rows[0].worker_id == ana && report.total_cents == 410000);
    team_dispose(&team);
    CHECK(strcmp(report.rows[0].profile.name, "Ana") == 0); /* Independent snapshot. */
    report_dispose(&report);
    report_dispose(&report);
}

static void test_empty_and_overflow(void)
{
    Team team = {0};
    PayrollReport report = {0};
    ReportFilter filter = {0};
    WorkerProfile data = profile("Ana", "Engineering", WORKER_JUNIOR);
    uint32_t id;
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK);
    CHECK(report.count == 0 && report.total_cents == 0);
    report_dispose(&report);
    data.salary_cents = INT64_MAX;
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OK);
    data.salary_cents = 1;
    memcpy(data.department, "Design", sizeof("Design"));
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OK);
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OVERFLOW);
    CHECK(report.rows == NULL && report.count == 0);
    memcpy(filter.department, "Design", sizeof("Design"));
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK &&
          report.total_cents == 1);
    report_dispose(&report);
    memcpy(filter.department, "Missing", sizeof("Missing"));
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK && report.count == 0);
    report_dispose(&report);
    CHECK(report_build(&team, 2026, 0, &filter, &report) == WORKFORCE_INVALID);
    filter.has_level = true;
    /* Intentional invalid caller input: verify rejection without changing output. */
    filter.level = (WorkerLevel)99; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_INVALID);
    filter = (ReportFilter){0};
    memset(filter.department, 'x', sizeof(filter.department));
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_INVALID);
    CHECK(report_build(NULL, 2026, 10, &filter, &report) == WORKFORCE_INVALID);
    team_dispose(&team);
}

static void test_contract_identity(void)
{
    Worker worker = {0};
    WorkerProfile data = profile("Ana", "Engineering", WORKER_SENIOR);
    HourContract replacement = {2026, 11, 20, 2000, 0};
    int64_t income;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, (HourContract){2026, 10, 1, 1000, 7}) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, (HourContract){2026, 10, 1, 1000, 99}) == WORKFORCE_OK);
    CHECK(worker_remove_contract(&worker, 7) == WORKFORCE_OK);
    CHECK(worker_update_contract(&worker, 99, replacement) == WORKFORCE_OK);
    CHECK(worker_find_contract(&worker, 99)->month == 11);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 300000);
    CHECK(worker_income(&worker, 2026, 11, &income) == WORKFORCE_OK && income == 340000);
    replacement.hourly_rate_cents = INT64_MAX;
    CHECK(worker_update_contract(&worker, 99, replacement) == WORKFORCE_OVERFLOW);
    CHECK(worker_find_contract(&worker, 99)->hourly_rate_cents == 2000);
    replacement = (HourContract){2026, 11, 1, INT64_MAX, 0};
    CHECK(worker_update_contract(&worker, 99, replacement) ==
          WORKFORCE_OVERFLOW); /* Addition, not multiplication. */
    replacement = (HourContract){2026, 11, 1, 1, 100};
    CHECK(worker_update_contract(&worker, 99, replacement) == WORKFORCE_INVALID);
    replacement.id = 0;
    replacement.hours = 0;
    CHECK(worker_update_contract(&worker, 99, replacement) == WORKFORCE_INVALID);
    CHECK(worker_update_contract(&worker, 123, (HourContract){2026, 1, 1, 1, 0}) ==
          WORKFORCE_NOT_FOUND);
    CHECK(worker_remove_contract(&worker, 7) == WORKFORCE_NOT_FOUND);
    CHECK(worker_add_contract(&worker, (HourContract){2026, 1, 1, 1, 0}) == WORKFORCE_OK);
    CHECK(worker.contracts[1].id == 100);
    CHECK(worker_add_contract(&worker, (HourContract){2026, 1, 1, 1, 99}) == WORKFORCE_INVALID);
    worker_dispose(&worker);
}

int main(void)
{
    test_contract_identity();
    test_report();
    test_empty_and_overflow();
    puts("Report domain tests passed.");
    return EXIT_SUCCESS;
}
