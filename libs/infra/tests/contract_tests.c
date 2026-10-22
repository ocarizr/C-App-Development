#include "test.h"
#include "workforce_store.h"

#include <errno.h>
#include <sqlite3.h>

static const char *path = "workforce-contract-edit-test.db";

static WorkforceStore *open_store(void)
{
    WorkforceStore *store = NULL;
    StoreOptions options = {path, 0};
    bool created;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_OK);
    return store;
}

static void verify(WorkforceStore *store, uint32_t id, int64_t contract_id)
{
    Worker worker = {0};
    int64_t income;
    CHECK(store_find(store, id, &worker) == WORKFORCE_OK);
    CHECK(worker.contract_count == 1 && worker.contracts[0].id == contract_id);
    CHECK(worker.contracts[0].year == 2027 && worker.contracts[0].month == 1);
    CHECK(worker.contracts[0].hours == 20 && worker.contracts[0].hourly_rate_cents == 100);
    CHECK(worker_income(&worker, 2026, 10, &income) == WORKFORCE_OK && income == 300000);
    CHECK(worker_income(&worker, 2027, 1, &income) == WORKFORCE_OK && income == 402000);
    worker_dispose(&worker);
}

int main(void)
{
    WorkforceStore *store;
    WorkerProfile data = {"Ana", "Developer", "Engineering", 30, WORKER_SENIOR, 300000};
    uint32_t ana, bruno;
    int64_t first, other_owner, editable, next, after_delete;
    HourContract contract = {2026, 10, 1, 100, 0};
    HourContract replacement = {2027, 1, 20, 100, 0};
    sqlite3 *raw = NULL;
    errno = 0;
    CHECK(remove(path) == 0 || errno == ENOENT);
    store = open_store();
    CHECK(store_add_worker(store, &data, &ana) == WORKFORCE_OK);
    CHECK(store_add_worker(store, &data, &bruno) == WORKFORCE_OK);
    CHECK(store_set_salary(store, ana, (SalaryChange){2027, 1, 400000}) == WORKFORCE_OK);
    CHECK(store_add_contract(store, ana, contract, &first) == WORKFORCE_OK);
    CHECK(store_add_contract(store, bruno, contract, &other_owner) == WORKFORCE_OK);
    CHECK(store_add_contract(store, ana, contract, &editable) == WORKFORCE_OK);
    CHECK(first < other_owner && other_owner < editable);
    CHECK(store_remove_contract(store, ana, first) == WORKFORCE_OK);
    CHECK(store_update_contract(store, ana, editable, replacement) == WORKFORCE_OK);
    CHECK(store_update_contract(store, bruno, editable, replacement) == WORKFORCE_NOT_FOUND);
    CHECK(store_remove_contract(store, bruno, editable) == WORKFORCE_NOT_FOUND);
    CHECK(store_update_contract(store, ana, first, replacement) == WORKFORCE_NOT_FOUND);
    CHECK(store_remove_contract(store, ana, first) == WORKFORCE_NOT_FOUND);
    replacement.hours = 2;
    replacement.hourly_rate_cents = INT64_MAX;
    CHECK(store_update_contract(store, ana, editable, replacement) == WORKFORCE_OVERFLOW);
    replacement = (HourContract){2027, 1, 20, 100, other_owner};
    CHECK(store_update_contract(store, ana, editable, replacement) == WORKFORCE_INVALID);
    replacement.id = 0;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(store_update_contract(store, ana, editable, replacement) == WORKFORCE_IO_ERROR);
    CHECK(sqlite3_exec(raw,
                       "ROLLBACK; CREATE TRIGGER reject_contract_edit BEFORE UPDATE ON contracts "
                       "BEGIN SELECT RAISE(ABORT,'simulated failure'); END",
                       NULL, NULL, NULL) == SQLITE_OK);
    replacement.hours = 10;
    CHECK(store_update_contract(store, ana, editable, replacement) == WORKFORCE_IO_ERROR);
    CHECK(sqlite3_exec(raw, "DROP TRIGGER reject_contract_edit", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    verify(store, ana, editable);
    store_close(store);
    store = open_store();
    verify(store, ana, editable);
    CHECK(store_add_contract(store, ana, contract, &next) == WORKFORCE_OK && next > editable);
    CHECK(store_remove_contract(store, ana, next) == WORKFORCE_OK);
    store_close(store);
    store = open_store();
    CHECK(store_add_contract(store, ana, contract, &after_delete) == WORKFORCE_OK &&
          after_delete > next);
    CHECK(store_update_contract(NULL, ana, editable, replacement) == WORKFORCE_INVALID);
    CHECK(store_remove_contract(store, ana, 0) == WORKFORCE_INVALID);
    CHECK(store_add_contract(store, ana, contract, NULL) == WORKFORCE_INVALID);
    store_close(store);
    CHECK(remove(path) == 0);
    puts("Persistent contract ID/edit tests passed.");
    return EXIT_SUCCESS;
}
