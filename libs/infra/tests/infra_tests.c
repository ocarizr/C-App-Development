#include "test_files.h"
#include "workforce_store.h"

#include <sqlite3.h>
#include <string.h>

static const char *database_path = "workforce-infra-test.db";

static WorkerProfile profile(void)
{
    WorkerProfile value = {"Ana O'Brien", "Software Developer", "Engineering", 30, WORKER_SENIOR,
                           300000};
    return value;
}

static WorkforceStore *open_store(bool expected_new)
{
    StoreOptions options = {database_path, 0};
    WorkforceStore *store = NULL;
    bool created = false;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_OK);
    CHECK(created == expected_new && error[0] == '\0');
    return store;
}

static void test_persistence(void)
{
    WorkforceStore *store;
    WorkforceStore *observer;
    WorkerProfile data = profile();
    HourContract contract = {2026, 10, 10, 2550, 0};
    Worker worker = {0};
    Team team = {0};
    uint32_t id = 0;
    int64_t contract_id;
    uint32_t next_id;
    int64_t income = 0;
    test_delete(database_path);
    store = open_store(true);
    CHECK(store_list(store, &team) == WORKFORCE_OK && team.count == 0);
    team_dispose(&team);
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_OK && id == 1);
    CHECK(store_add_contract(store, id, contract, &contract_id) == WORKFORCE_OK);
    contract.month = 11;
    CHECK(store_add_contract(store, id, contract, &contract_id) == WORKFORCE_OK);
    observer = open_store(false);
    CHECK(store_find(observer, id, &worker) == WORKFORCE_OK);
    CHECK(strcmp(worker.profile.name, "Ana O'Brien") == 0 && worker.contract_count == 2);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 325500);
    worker_dispose(&worker);
    store_close(observer);
    store_close(store);

    store = open_store(false);
    CHECK(store_list(store, &team) == WORKFORCE_OK && team.count == 1);
    CHECK(team.workers[0].id == id && team.workers[0].contract_count == 2);
    team_dispose(&team);
    CHECK(store_remove_contract(store, id, 1) == WORKFORCE_OK);
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK);
    CHECK(worker.contract_count == 1 && worker.contracts[0].month == 11);
    worker_dispose(&worker);
    CHECK(store_remove_contract(store, id, 99) == WORKFORCE_NOT_FOUND);
    CHECK(store_remove_worker(store, id) == WORKFORCE_OK);
    CHECK(store_find(store, id, &worker) == WORKFORCE_NOT_FOUND);
    CHECK(store_remove_worker(store, id) == WORKFORCE_NOT_FOUND);
    store_close(store);
    store = open_store(false);
    CHECK(store_add_worker(store, &data, &next_id) == WORKFORCE_OK && next_id > id);
    CHECK(store_find(store, next_id, &worker) == WORKFORCE_OK && worker.contract_count == 0);
    worker_dispose(&worker);
    store_close(store);
}

static void test_failures(void)
{
    WorkforceStore *store = open_store(false);
    WorkerProfile data = profile();
    HourContract contract = {2026, 10, 2, INT64_MAX, 0};
    uint32_t id;
    int64_t contract_id;
    Worker worker = {0};
    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_OK);
    CHECK(store_add_contract(store, id, contract, &contract_id) == WORKFORCE_OVERFLOW);
    contract.hourly_rate_cents = 0;
    contract.month = 0;
    CHECK(store_add_contract(store, id, contract, &contract_id) == WORKFORCE_INVALID);
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK && worker.contract_count == 0);
    worker_dispose(&worker);
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_IO_ERROR);
    CHECK(store_error(store)[0] != '\0');
    CHECK(sqlite3_exec(raw, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
                       "CREATE TRIGGER fail_contract BEFORE INSERT ON contracts "
                       "BEGIN SELECT RAISE(ABORT, 'simulated write failure'); END",
                       NULL, NULL, NULL) == SQLITE_OK);
    contract.month = 10;
    CHECK(store_add_contract(store, id, contract, &contract_id) == WORKFORCE_IO_ERROR);
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK && worker.contract_count == 0);
    worker_dispose(&worker);
    CHECK(sqlite3_exec(raw, "DROP TRIGGER fail_contract", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(store_add_contract(store, id, contract, &contract_id) == WORKFORCE_OK);
    CHECK(store_remove_worker(store, id) == WORKFORCE_OK);
    CHECK(sqlite3_prepare_v2(raw, "SELECT count(*) FROM contracts", -1, &statement, NULL) ==
          SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 0);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE sqlite_sequence SET seq=4294967295 WHERE name='workers'", NULL,
                       NULL, NULL) == SQLITE_OK);
    id = 17;
    CHECK(store_add_worker(store, &data, &id) == WORKFORCE_OVERFLOW && id == 17);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(store_find(store, UINT32_MAX, &worker) == WORKFORCE_NOT_FOUND);
    CHECK(store_add_worker(store, NULL, &id) == WORKFORCE_INVALID);
    CHECK(store_list(store, NULL) == WORKFORCE_INVALID);
    CHECK(store_find(store, 0, &worker) == WORKFORCE_INVALID);
    CHECK(store_add_contract(NULL, 1, contract, &contract_id) == WORKFORCE_INVALID);
    CHECK(store_remove_contract(NULL, 1, 0) == WORKFORCE_INVALID);
    store_close(store);
}

static void test_invalid_database(void)
{
    WorkforceStore *store = NULL;
    StoreOptions options = {"workforce-infra-invalid.db", 0};
    bool created;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    sqlite3 *raw = NULL;
    FILE *file;
    test_delete(options.path);
    file = test_file(options.path, "wb");
    CHECK(fputs("This is not a SQLite database", file) >= 0);
    CHECK(fclose(file) == 0);
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_IO_ERROR && store == NULL);
    test_delete(options.path);
    CHECK(sqlite3_open(options.path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "CREATE TABLE unrelated(value);", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_IO_ERROR);
    test_delete(options.path);
    CHECK(sqlite3_open(options.path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA user_version=999", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_IO_ERROR);
    test_delete(options.path);
    CHECK(sqlite3_open(options.path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "PRAGMA user_version=1; PRAGMA application_id=1464226371;", NULL, NULL,
                       NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_IO_ERROR);
    test_delete(options.path);
    options.path = "workforce-nonexistent-directory/data.db";
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_IO_ERROR);
    store_close(NULL);
}

int main(void)
{
    test_persistence();
    test_failures();
    test_invalid_database();
    test_delete(database_path);
    puts("SQLite infrastructure library tests passed.");
    return EXIT_SUCCESS;
}
