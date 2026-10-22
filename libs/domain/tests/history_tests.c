#include "test.h"
#include "workforce.h"

#include <string.h>

static WorkerProfile profile(void)
{
    WorkerProfile data = {"Ana Silva", "Developer", "Engineering", 30, WORKER_JUNIOR, 300000};
    return data;
}

static void check_salary(const Worker *worker, int year, int month, int64_t expected)
{
    int64_t actual = -1;
    CHECK(worker_salary_at(worker, year, month, &actual) == WORKFORCE_OK && actual == expected);
}

static void test_timeline_and_profile(void)
{
    Worker worker = {0};
    WorkerProfile data = profile();
    HourContract contract = {2026, 10, 10, 100, 0};
    int64_t income;
    CHECK(worker_init(&worker, 42, &data) == WORKFORCE_OK);
    CHECK(worker_set_salary(&worker, (SalaryChange){2027, 1, 450000}) == WORKFORCE_OK);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 10, 400000}) == WORKFORCE_OK);
    CHECK(worker.salary_count == 2 && worker.salary_changes[0].year == 2026);
    check_salary(&worker, 2026, 9, 300000);
    check_salary(&worker, 2026, 10, 400000);
    check_salary(&worker, 2026, 12, 400000);
    check_salary(&worker, 2027, 1, 450000);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 10, 420000}) == WORKFORCE_OK);
    CHECK(worker.salary_count == 2);
    check_salary(&worker, 2026, 10, 420000);
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OK);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 421000);
    memcpy(data.name, "Ana Maria", sizeof("Ana Maria"));
    data.age = 31;
    data.level = WORKER_SENIOR;
    CHECK(worker_update_profile(&worker, &data) == WORKFORCE_OK);
    CHECK(strcmp(worker.profile.name, "Ana Maria") == 0 && worker.id == 42);
    CHECK(worker.profile.age == 31 && worker.profile.level == WORKER_SENIOR);
    CHECK(worker.contract_count == 1 && worker.salary_count == 2);
    data.salary_cents = 1;
    CHECK(worker_update_profile(&worker, &data) == WORKFORCE_INVALID);
    CHECK(worker.profile.salary_cents == 300000);
    data = worker.profile;
    data.name[0] = '\0';
    CHECK(worker_update_profile(&worker, &data) == WORKFORCE_INVALID);
    CHECK(strcmp(worker.profile.name, "Ana Maria") == 0);
    CHECK(worker_remove_salary(&worker, 2026, 10) == WORKFORCE_OK);
    check_salary(&worker, 2026, 10, 300000);
    check_salary(&worker, 2027, 1, 450000);
    CHECK(worker_remove_salary(&worker, 2026, 10) == WORKFORCE_NOT_FOUND);
    CHECK(worker_remove_salary(&worker, 2027, 1) == WORKFORCE_OK);
    CHECK(worker.salary_count == 0 && worker.salary_changes == NULL);
    check_salary(&worker, 9999, 12, 300000);
    worker_dispose(&worker);
    worker_dispose(&worker);
}

static void test_overflow_and_validation(void)
{
    Worker worker = {0};
    WorkerProfile data = profile();
    HourContract contract = {2026, 10, 1, 10, 0};
    int64_t value = 17;
    data.salary_cents = INT64_MAX;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_OK);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 10, 0}) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OK);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 10, INT64_MAX}) == WORKFORCE_OVERFLOW);
    CHECK(worker_remove_salary(&worker, 2026, 10) == WORKFORCE_OVERFLOW);
    check_salary(&worker, 2026, 10, 0);
    CHECK(worker.salary_count == 1);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 11, INT64_MAX}) == WORKFORCE_OK);
    contract.month = 11;
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OVERFLOW);
    CHECK(worker_set_salary(&worker, (SalaryChange){0, 1, 0}) == WORKFORCE_INVALID);
    CHECK(worker_set_salary(&worker, (SalaryChange){10000, 1, 0}) == WORKFORCE_INVALID);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 13, 0}) == WORKFORCE_INVALID);
    CHECK(worker_set_salary(&worker, (SalaryChange){2026, 1, -1}) == WORKFORCE_INVALID);
    CHECK(worker_remove_salary(&worker, 2026, 0) == WORKFORCE_INVALID);
    CHECK(worker_salary_at(&worker, 0, 1, &value) == WORKFORCE_INVALID && value == 17);
    CHECK(worker_salary_at(&worker, 2026, 1, NULL) == WORKFORCE_INVALID);
    CHECK(worker_update_profile(&worker, NULL) == WORKFORCE_INVALID);
    CHECK(worker_set_salary(NULL, (SalaryChange){2026, 1, 1}) == WORKFORCE_INVALID);
    CHECK(worker_remove_salary(NULL, 2026, 1) == WORKFORCE_INVALID);
    worker_dispose(&worker);
}

static void test_history_growth(void)
{
    Team team = {0};
    WorkerProfile data = profile();
    uint32_t id;
    int year;
    Worker *worker;
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OK);
    worker = team_find(&team, id);
    for (year = 2099; year >= 2000; --year)
    {
        CHECK(worker_set_salary(worker, (SalaryChange){year, 1, year}) == WORKFORCE_OK);
    }

    CHECK(worker->salary_count == 100);
    check_salary(worker, 2050, 12, 2050);
    CHECK(team_remove(&team, id) == WORKFORCE_OK);
    team_dispose(&team);
}

int main(void)
{
    test_timeline_and_profile();
    test_overflow_and_validation();
    test_history_growth();
    puts("Salary history domain tests passed.");
    return EXIT_SUCCESS;
}
