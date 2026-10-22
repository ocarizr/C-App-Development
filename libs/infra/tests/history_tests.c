#include "test.h"
#include "workforce_store.h"

#include <errno.h>
#include <sqlite3.h>
#include <string.h>

static const char *path = "workforce-history-test.db";

static void delete_database(void)
{
    errno = 0;
    CHECK(remove(path) == 0 || errno == ENOENT);
}

static WorkerProfile profile(void)
{
    WorkerProfile data = {"Ana Silva", "Developer", "Engineering", 30, WORKER_JUNIOR, 300000};
    return data;
}

static WorkforceStore *open_store(void)
{
    WorkforceStore *store = NULL;
    StoreOptions options = {path, 0};
    bool created;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_OK);
    return store;
}

static void sql(sqlite3 *db, const char *text)
{
    CHECK(sqlite3_exec(db, text, NULL, NULL, NULL) == SQLITE_OK);
}

static int sql_integer(sqlite3 *db, const char *text)
{
    sqlite3_stmt *statement = NULL;
    int value;
    CHECK(sqlite3_prepare_v2(db, text, -1, &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    value = sqlite3_column_int(statement, 0);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    return value;
}

static void check_income(WorkforceStore *store, uint32_t id, int year, int month, int64_t expected)
{
    Worker worker = {0};
    int64_t actual = -1;
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK);
    CHECK(worker_income(&worker, year, month, &actual) == WORKFORCE_OK && actual == expected);
    worker_dispose(&worker);
}

static void test_persistent_edits(void)
{
    WorkforceStore *store;
    WorkforceStore *observer;
    WorkerProfile data = profile();
    uint32_t id;
    Worker worker = {0};
    Team team = {0};
    sqlite3 *raw = NULL;
    delete_database();
    store = open_store();
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_OK);
    CHECK(store_add_contract(store, id, (HourContract){2026, 10, 10, 100}) == WORKFORCE_OK);
    CHECK(store_set_salary(store, id, (SalaryChange){2027, 1, 450000}) == WORKFORCE_OK);
    CHECK(store_set_salary(store, id, (SalaryChange){2026, 10, 400000}) == WORKFORCE_OK);
    CHECK(store_set_salary(store, id, (SalaryChange){2026, 10, 420000}) == WORKFORCE_OK);
    memcpy(data.name, "Ana D'Avila", sizeof("Ana D'Avila"));
    data.age = 31;
    data.level = WORKER_SENIOR;
    CHECK(store_update_profile(store, id, &data) == WORKFORCE_OK);
    data.salary_cents = 1;
    CHECK(store_update_profile(store, id, &data) == WORKFORCE_INVALID);
    CHECK(store_set_salary(store, id, (SalaryChange){2026, 10, INT64_MAX}) == WORKFORCE_OVERFLOW);
    observer = open_store();
    check_income(observer, id, 2026, 9, 300000);
    check_income(observer, id, 2026, 10, 421000);
    check_income(observer, id, 2027, 1, 450000);
    store_close(observer);
    store_close(store);
    store = open_store();
    CHECK(store_list(store, &team) == WORKFORCE_OK && team.count == 1);
    CHECK(strcmp(team.workers[0].profile.name, "Ana D'Avila") == 0);
    CHECK(team.workers[0].salary_count == 2 && team.workers[0].contract_count == 1);
    CHECK(team.workers[0].profile.salary_cents == 300000 && team.workers[0].id == id);
    team_dispose(&team);
    CHECK(store_remove_salary(store, id, 2026, 10) == WORKFORCE_OK);
    CHECK(store_remove_salary(store, id, 2026, 10) == WORKFORCE_NOT_FOUND);
    check_income(store, id, 2026, 10, 301000);
    CHECK(store_update_profile(store, UINT32_MAX, &data) == WORKFORCE_NOT_FOUND);
    CHECK(store_set_salary(store, UINT32_MAX, (SalaryChange){2026, 1, 1}) == WORKFORCE_NOT_FOUND);
    CHECK(store_remove_salary(store, UINT32_MAX, 2026, 1) == WORKFORCE_NOT_FOUND);
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK);
    data = worker.profile;
    worker_dispose(&worker);
    data.age = 0;
    CHECK(store_update_profile(store, id, &data) == WORKFORCE_INVALID);
    CHECK(store_remove_salary(store, id, 0, 1) == WORKFORCE_INVALID);
    CHECK(store_set_salary(store, id, (SalaryChange){2026, 0, 1}) == WORKFORCE_INVALID);
    CHECK(store_update_profile(store, id, NULL) == WORKFORCE_INVALID);
    CHECK(store_update_profile(NULL, id, &data) == WORKFORCE_INVALID);
    CHECK(store_remove_worker(store, id) == WORKFORCE_OK);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sql_integer(raw, "SELECT count(*) FROM salary_changes") == 0);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    store_close(store);
}

static void test_rollback_and_lower_salary(void)
{
    WorkforceStore *store;
    WorkerProfile data = profile();
    Worker worker = {0};
    uint32_t id;
    sqlite3 *raw = NULL;
    delete_database();
    store = open_store();
    data.salary_cents = INT64_MAX;
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_OK);
    CHECK(store_set_salary(store, id, (SalaryChange){2026, 10, 0}) == WORKFORCE_OK);
    CHECK(store_add_contract(store, id, (HourContract){2026, 10, 1, 10}) == WORKFORCE_OK);
    CHECK(store_remove_salary(store, id, 2026, 10) == WORKFORCE_OVERFLOW);
    store_close(store);
    store = open_store();
    check_income(store, id, 2026, 10, 10);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    sql(raw, "BEGIN IMMEDIATE");
    CHECK(store_set_salary(store, id, (SalaryChange){2027, 1, 1}) == WORKFORCE_IO_ERROR);
    sql(raw, "ROLLBACK");
    sql(raw, "CREATE TRIGGER reject_salary BEFORE INSERT ON salary_changes "
             "BEGIN SELECT RAISE(ABORT,'simulated failure'); END");
    CHECK(store_set_salary(store, id, (SalaryChange){2026, 10, 1}) == WORKFORCE_IO_ERROR);
    check_income(store, id, 2026, 10, 10);
    sql(raw, "DROP TRIGGER reject_salary; "
             "CREATE TRIGGER reject_salary_delete BEFORE DELETE ON salary_changes "
             "BEGIN SELECT RAISE(ABORT,'simulated failure'); END");
    /* A valid domain deletion must still roll back if the database rejects it. */
    CHECK(store_remove_contract(store, id, 0) == WORKFORCE_OK);
    CHECK(store_remove_salary(store, id, 2026, 10) == WORKFORCE_IO_ERROR);
    check_income(store, id, 2026, 10, 0);
    sql(raw, "DROP TRIGGER reject_salary_delete; "
             "CREATE TRIGGER reject_profile BEFORE UPDATE ON workers "
             "BEGIN SELECT RAISE(ABORT,'simulated failure'); END");
    memcpy(data.name, "Changed", sizeof("Changed"));
    CHECK(store_update_profile(store, id, &data) == WORKFORCE_IO_ERROR);
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK);
    CHECK(strcmp(worker.profile.name, "Ana Silva") == 0 && worker.salary_count == 1);
    worker_dispose(&worker);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    store_close(store);
}

static void create_version_one(sqlite3 *raw)
{
    sql(raw, "CREATE TABLE workers(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,"
             "profession TEXT NOT NULL,department TEXT NOT NULL,age INTEGER NOT NULL,"
             "level INTEGER NOT NULL,salary_cents INTEGER NOT NULL) STRICT;"
             "CREATE TABLE contracts(id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "worker_id INTEGER REFERENCES workers(id) ON DELETE CASCADE,"
             "year INTEGER NOT NULL,month INTEGER NOT NULL,hours INTEGER NOT NULL,"
             "hourly_rate_cents INTEGER NOT NULL) STRICT;"
             "INSERT INTO workers VALUES(42,'Legacy Worker','Developer','Engineering',30,1,500000);"
             "INSERT INTO contracts VALUES(11,42,2026,10,2,10000);"
             "PRAGMA application_id=1464226371; PRAGMA user_version=1;");
}

static void test_migration(void)
{
    sqlite3 *raw = NULL;
    WorkforceStore *store;
    WorkerProfile data = profile();
    uint32_t id;
    bool created;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    StoreOptions options = {path, 0};
    delete_database();
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    create_version_one(raw);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    store = open_store();
    check_income(store, 42, 2026, 10, 520000);
    CHECK(store_set_salary(store, 42, (SalaryChange){2026, 10, 600000}) == WORKFORCE_OK);
    check_income(store, 42, 2026, 10, 620000);
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_OK && id == 43);
    store_close(store);
    store = open_store();
    check_income(store, 42, 2026, 9, 500000);
    check_income(store, 42, 2026, 10, 620000);
    store_close(store);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sql_integer(raw, "PRAGMA user_version") == 2);
    CHECK(sql_integer(raw, "SELECT count(*) FROM salary_changes") == 1);
    CHECK(sqlite3_close(raw) == SQLITE_OK);

    delete_database();
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    create_version_one(raw);
    sql(raw, "CREATE TABLE salary_changes(unrelated)");
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    store = NULL;
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_IO_ERROR && store == NULL);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sql_integer(raw, "PRAGMA user_version") == 1);
    CHECK(sql_integer(raw, "SELECT count(*) FROM workers WHERE id=42") == 1);
    CHECK(sql_integer(raw, "SELECT count(*) FROM contracts WHERE id=11") == 1);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
}

int main(void)
{
    test_persistent_edits();
    test_rollback_and_lower_salary();
    test_migration();
    delete_database();
    puts("Salary history infrastructure tests passed.");
    return EXIT_SUCCESS;
}
