#include "money.h"
#include "test.h"
#include "workforce.h"

#include <string.h>

static WorkerProfile profile(void)
{
    WorkerProfile result = {"Ana Silva", "Software Developer", "Engineering",
                            30,          WORKER_SENIOR,        300000};
    return result;
}

static void test_money(void)
{
    int64_t cents = -1;
    const char *invalid[] = {"",
                             "-1",
                             "+1",
                             " 1",
                             "1 ",
                             "1,50",
                             "1e3",
                             "nan",
                             "inf",
                             "1.",
                             ".50",
                             "1.001",
                             "1.2x",
                             "92233720368547758.08",
                             "999999999999999999999999999999999999"};
    size_t i;
    CHECK(money_parse("0", &cents) && cents == 0);
    CHECK(money_parse("12", &cents) && cents == 1200);
    CHECK(money_parse("0.01", &cents) && cents == 1);
    CHECK(money_parse("12.3", &cents) && cents == 1230);
    CHECK(money_parse("0012.34", &cents) && cents == 1234);
    CHECK(money_parse("92233720368547758.07", &cents) && cents == INT64_MAX);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        cents = 17;
        CHECK(!money_parse(invalid[i], &cents));
        CHECK(cents == 17);
    }
    CHECK(!money_parse(NULL, &cents));
    CHECK(!money_parse("1", NULL));
}

static void test_profiles(void)
{
    Worker worker = {0};
    WorkerProfile data = profile();
    WorkerLevel level = WORKER_JUNIOR;
    int i;
    for (i = WORKER_TRAINEE; i <= WORKER_SENIOR; ++i)
    {
        CHECK(worker_level_parse(worker_level_name((WorkerLevel)i), &level) == WORKFORCE_OK);
        CHECK(level == (WorkerLevel)i);
    }
    CHECK(worker_level_parse("Invalid", &level) == WORKFORCE_INVALID);
    CHECK(worker_level_parse(NULL, &level) == WORKFORCE_INVALID);
    CHECK(worker_level_parse("Junior", NULL) == WORKFORCE_INVALID);
    CHECK(strcmp(worker_level_name((WorkerLevel)99), "Unknown") == 0);
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_OK);
    data.name[0] = 'X';
    CHECK(strcmp(worker.profile.name, "Ana Silva") == 0);
    CHECK(worker.contract_count == 0 && worker.contracts == NULL);
    worker_dispose(&worker);
    data = profile();
    CHECK(worker_init(NULL, 1, &data) == WORKFORCE_INVALID);
    CHECK(worker_init(&worker, 0, &data) == WORKFORCE_INVALID);
    CHECK(worker_init(&worker, 1, NULL) == WORKFORCE_INVALID);
    data.age = 0;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    data.age = 151;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    data = profile();
    data.salary_cents = -1;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    data = profile();
    data.level = (WorkerLevel)99;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    data = profile();
    data.department[0] = '\0';
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    data = profile();
    memset(data.name, 'x', sizeof(data.name));
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    data = profile();
    memcpy(data.profession, " \t", 3);
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_INVALID);
    CHECK(worker.id == 0 && worker.contracts == NULL);
}

static void test_contracts(void)
{
    Worker worker = {0};
    WorkerProfile data = profile();
    HourContract october = {2026, 10, 10, 2550};
    HourContract november = {2026, 11, 3, 1000};
    HourContract other_year = {2025, 10, 1, 100};
    HourContract invalid = october;
    int64_t income = -1;
    int i;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_OK);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 300000);
    CHECK(worker_add_contract(&worker, october) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, november) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, other_year) == WORKFORCE_OK);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 325500);
    CHECK(worker_income(&worker, 2026, 11, &income) == WORKFORCE_OK && income == 303000);
    CHECK(worker_income(&worker, 2025, 10, &income) == WORKFORCE_OK && income == 300100);
    invalid.month = 13;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    invalid.month = 0;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    invalid = october;
    invalid.year = 0;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    invalid.year = 10000;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    invalid = october;
    invalid.hours = 0;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    invalid.hours = -1;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    invalid = october;
    invalid.hourly_rate_cents = -1;
    CHECK(worker_add_contract(&worker, invalid) == WORKFORCE_INVALID);
    CHECK(worker.contract_count == 3);
    CHECK(worker_remove_contract(&worker, 1) == WORKFORCE_OK);
    CHECK(worker.contract_count == 2 && worker.contracts[1].year == 2025);
    CHECK(worker_remove_contract(&worker, 0) == WORKFORCE_OK);
    CHECK(worker_remove_contract(&worker, 0) == WORKFORCE_OK);
    CHECK(worker_remove_contract(&worker, 0) == WORKFORCE_NOT_FOUND);
    for (i = 0; i < 100; ++i)
    {
        CHECK(worker_add_contract(&worker, october) == WORKFORCE_OK);
    }
    CHECK(worker.contract_count == 100);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 2850000);
    income = 17;
    CHECK(worker_income(&worker, 2026, 0, &income) == WORKFORCE_INVALID && income == 17);
    CHECK(worker_income(&worker, 2026, 10, NULL) == WORKFORCE_INVALID);
    worker_dispose(&worker);
    CHECK(worker.contracts == NULL && worker.contract_count == 0 && worker.id == 0);
    worker_dispose(&worker);
    CHECK(worker_add_contract(&worker, october) == WORKFORCE_INVALID);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_INVALID);
    CHECK(worker_remove_contract(NULL, 0) == WORKFORCE_INVALID);
    worker_dispose(NULL);
}

static void test_overflow(void)
{
    Worker worker;
    WorkerProfile data = profile();
    HourContract contract = {2026, 10, 2, INT64_MAX};
    int64_t income;
    data.salary_cents = 0;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OVERFLOW);
    CHECK(worker.contract_count == 0);
    contract.hours = 1;
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OK);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == INT64_MAX);
    contract.hourly_rate_cents = 1;
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OVERFLOW);
    CHECK(worker.contract_count == 1);
    contract.month = 11;
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OK);
    worker_dispose(&worker);
    data.salary_cents = INT64_MAX;
    CHECK(worker_init(&worker, 1, &data) == WORKFORCE_OK);
    CHECK(worker_add_contract(&worker, contract) == WORKFORCE_OVERFLOW);
    worker_dispose(&worker);
}

static void test_team(void)
{
    Team team = {0};
    WorkerProfile data = profile();
    HourContract contract = {2026, 10, 5, 100};
    uint32_t id = 0;
    uint32_t expected;
    CHECK(team_find(&team, 1) == NULL);
    CHECK(team_remove(&team, 1) == WORKFORCE_NOT_FOUND);
    CHECK(team_add(NULL, &data, &id) == WORKFORCE_INVALID);
    CHECK(team_add(&team, &data, NULL) == WORKFORCE_INVALID);
    CHECK(team_add(&team, NULL, &id) == WORKFORCE_INVALID);
    CHECK(team.count == 0 && team.last_id == 0);
    for (expected = 1; expected <= 100; ++expected)
    {
        Worker *worker;
        CHECK(team_add(&team, &data, &id) == WORKFORCE_OK && id == expected);
        worker = team_find(&team, id);
        CHECK(worker != NULL);
        CHECK(worker_add_contract(worker, contract) == WORKFORCE_OK);
    }
    CHECK(team.count == 100);
    CHECK(team_remove(&team, 50) == WORKFORCE_OK);
    CHECK(team_find(&team, 50) == NULL && team.count == 99);
    CHECK(team_find(&team, 51)->contracts[0].hours == 5);
    CHECK(team_remove(&team, 1) == WORKFORCE_OK);
    CHECK(team_remove(&team, 100) == WORKFORCE_OK);
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OK && id == 101);
    team.last_id = UINT32_MAX;
    id = 17;
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OVERFLOW && id == 17);
    team_dispose(&team);
    CHECK(team.workers == NULL && team.count == 0 && team.capacity == 0 && team.last_id == 0);
    team_dispose(&team);
    team_dispose(NULL);
}

int main(void)
{
    test_money();
    test_profiles();
    test_contracts();
    test_overflow();
    test_team();
    puts("Domain tests passed.");
    return EXIT_SUCCESS;
}
