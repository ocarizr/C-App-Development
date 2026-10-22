#include "workforce_store.h"

#include <limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct WorkforceStore
{
    sqlite3 *db;
    char error[WORKFORCE_STORE_ERROR_SIZE];
};

static WorkforceStatus db_error(WorkforceStore *store)
{
    (void)snprintf(store->error, sizeof(store->error), "%s", sqlite3_errmsg(store->db));
    return sqlite3_errcode(store->db) == SQLITE_NOMEM ? WORKFORCE_NO_MEMORY : WORKFORCE_IO_ERROR;
}

static WorkforceStatus exec_sql(WorkforceStore *store, const char *sql)
{
    return sqlite3_exec(store->db, sql, NULL, NULL, NULL) == SQLITE_OK ? WORKFORCE_OK
                                                                       : db_error(store);
}

static WorkforceStatus prepare(WorkforceStore *store, const char *sql, sqlite3_stmt **statement)
{
    return sqlite3_prepare_v2(store->db, sql, -1, statement, NULL) == SQLITE_OK ? WORKFORCE_OK
                                                                                : db_error(store);
}

static WorkforceStatus finish_write(WorkforceStore *store, WorkforceStatus status)
{
    if (status == WORKFORCE_OK)
    {
        status = exec_sql(store, "COMMIT");
    }
    if (status != WORKFORCE_OK)
    {
        /* Preserve the original diagnostic if rollback succeeds or fails. */
        (void)sqlite3_exec(store->db, "ROLLBACK", NULL, NULL, NULL);
    }
    return status;
}

static WorkforceStatus step_write(WorkforceStore *store, sqlite3_stmt *statement)
{
    WorkforceStatus status = WORKFORCE_OK;
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        status = db_error(store);
    }
    if (sqlite3_finalize(statement) != SQLITE_OK && status == WORKFORCE_OK)
    {
        status = db_error(store);
    }
    return status;
}

static WorkforceStatus pragma_integer(WorkforceStore *store, const char *sql, int *value)
{
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status = prepare(store, sql, &statement);
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        *value = sqlite3_column_int(statement, 0);
    }
    else
    {
        status = db_error(store);
    }
    (void)sqlite3_finalize(statement);
    return status;
}

static WorkforceStatus initialize_schema(WorkforceStore *store)
{
    int version = 0;
    int application = 0;
    int tables = 0;
    WorkforceStatus status = exec_sql(store, "BEGIN IMMEDIATE");
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    status = pragma_integer(store, "PRAGMA user_version", &version);
    if (status == WORKFORCE_OK)
    {
        status = pragma_integer(store, "PRAGMA application_id", &application);
    }
    if (status == WORKFORCE_OK && version == 0 && application == 0)
    {
        status = pragma_integer(
            store, "SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'", &tables);
        if (status == WORKFORCE_OK && tables == 0)
        {
            status = exec_sql(
                store,
                "CREATE TABLE workers("
                "id INTEGER PRIMARY KEY AUTOINCREMENT CHECK(id BETWEEN 1 AND 4294967295),"
                "name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 99),"
                "profession TEXT NOT NULL CHECK(length(CAST(profession AS BLOB)) BETWEEN 1 AND 99),"
                "department TEXT NOT NULL CHECK(length(CAST(department AS BLOB)) BETWEEN 1 AND 99),"
                "age INTEGER NOT NULL CHECK(age BETWEEN 1 AND 150),"
                "level INTEGER NOT NULL CHECK(level BETWEEN 0 AND 3),"
                "salary_cents INTEGER NOT NULL CHECK(salary_cents >= 0)) STRICT;"
                "CREATE TABLE contracts("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "worker_id INTEGER NOT NULL REFERENCES workers(id) ON DELETE CASCADE,"
                "year INTEGER NOT NULL CHECK(year BETWEEN 1 AND 9999),"
                "month INTEGER NOT NULL CHECK(month BETWEEN 1 AND 12),"
                "hours INTEGER NOT NULL CHECK(hours BETWEEN 1 AND 2147483647),"
                "hourly_rate_cents INTEGER NOT NULL CHECK(hourly_rate_cents >= 0)) STRICT;"
                "CREATE INDEX contracts_worker_period ON contracts(worker_id,year,month);"
                "PRAGMA application_id=1464226371; PRAGMA user_version=1;");
        }
        else if (status == WORKFORCE_OK)
        {
            status = WORKFORCE_IO_ERROR;
        }
    }
    else if (status == WORKFORCE_OK && (version != 1 || application != 1464226371))
    {
        status = WORKFORCE_IO_ERROR;
    }
    if (status == WORKFORCE_IO_ERROR && sqlite3_errcode(store->db) == SQLITE_OK)
    {
        (void)snprintf(store->error, sizeof(store->error),
                       "Banco ou versao de esquema incompativel.");
    }
    if (status == WORKFORCE_OK)
    {
        sqlite3_stmt *statement = NULL;
        status =
            prepare(store,
                    "SELECT w.id,w.name,w.profession,w.department,w.age,w.level,w.salary_cents,"
                    "c.id,c.worker_id,c.year,c.month,c.hours,c.hourly_rate_cents "
                    "FROM workers w LEFT JOIN contracts c ON c.worker_id=w.id LIMIT 0",
                    &statement);
        (void)sqlite3_finalize(statement);
    }
    return finish_write(store, status);
}

WorkforceStatus store_open(const StoreOptions *options, WorkforceStore **output, bool *created,
                           char error[WORKFORCE_STORE_ERROR_SIZE])
{
    WorkforceStore *store;
    WorkforceStatus status;
    bool is_new = false;
    int result;
    if (options == NULL || output == NULL || created == NULL || error == NULL ||
        options->path == NULL || options->path[0] == '\0' || options->busy_timeout_ms < 0 ||
        options->busy_timeout_ms > 60000)
    {
        return WORKFORCE_INVALID;
    }
    store = calloc(1, sizeof(*store));
    if (store == NULL)
    {
        (void)snprintf(error, WORKFORCE_STORE_ERROR_SIZE, "Memoria insuficiente.");
        return WORKFORCE_NO_MEMORY;
    }
    result = sqlite3_open_v2(options->path, &store->db, SQLITE_OPEN_READWRITE, NULL);
    if (result == SQLITE_CANTOPEN)
    {
        (void)sqlite3_close(store->db);
        store->db = NULL;
        result = sqlite3_open_v2(options->path, &store->db,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        is_new = result == SQLITE_OK;
    }
    if (result != SQLITE_OK)
    {
        status = db_error(store);
    }
    else
    {
        result = sqlite3_busy_timeout(store->db, options->busy_timeout_ms);
        status = result == SQLITE_OK ? exec_sql(store, "PRAGMA foreign_keys=ON") : db_error(store);
        if (status == WORKFORCE_OK)
        {
            status = initialize_schema(store);
        }
    }
    if (status != WORKFORCE_OK)
    {
        (void)snprintf(error, WORKFORCE_STORE_ERROR_SIZE, "%s", store->error);
        store_close(store);
        return status;
    }
    error[0] = '\0';
    *output = store;
    *created = is_new;
    return WORKFORCE_OK;
}

void store_close(WorkforceStore *store)
{
    if (store != NULL)
    {
        (void)sqlite3_close(store->db);
        free(store);
    }
}

const char *store_error(const WorkforceStore *store)
{
    return store == NULL ? "Conexao invalida." : store->error;
}

static bool column_text(sqlite3_stmt *statement, int index, char output[WORKFORCE_NAME_SIZE])
{
    const unsigned char *text = sqlite3_column_text(statement, index);
    int bytes = sqlite3_column_bytes(statement, index);
    if (text == NULL || bytes < 1 || bytes >= WORKFORCE_NAME_SIZE ||
        strlen((const char *)text) != (size_t)bytes)
    {
        return false;
    }
    memcpy(output, text, (size_t)bytes + 1);
    return true;
}

static WorkforceStatus load_contracts(WorkforceStore *store, Worker *worker)
{
    sqlite3_stmt *statement = NULL;
    int result = SQLITE_DONE;
    WorkforceStatus status = prepare(
        store,
        "SELECT year,month,hours,hourly_rate_cents FROM contracts WHERE worker_id=? ORDER BY id",
        &statement);
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    if (sqlite3_bind_int64(statement, 1, worker->id) != SQLITE_OK)
    {
        status = db_error(store);
    }
    while (status == WORKFORCE_OK && (result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        HourContract contract = {sqlite3_column_int(statement, 0), sqlite3_column_int(statement, 1),
                                 sqlite3_column_int(statement, 2),
                                 sqlite3_column_int64(statement, 3)};
        status = worker_add_contract(worker, contract);
        if (status == WORKFORCE_INVALID || status == WORKFORCE_OVERFLOW)
        {
            (void)snprintf(store->error, sizeof(store->error), "Contrato persistido invalido.");
            status = WORKFORCE_IO_ERROR;
        }
    }
    if (status == WORKFORCE_OK && result != SQLITE_DONE)
    {
        status = db_error(store);
    }
    (void)sqlite3_finalize(statement);
    return status;
}

static WorkforceStatus read_worker(WorkforceStore *store, sqlite3_stmt *statement, Worker *worker)
{
    WorkerProfile profile = {0};
    sqlite3_int64 id = sqlite3_column_int64(statement, 0);
    WorkforceStatus status;
    if (id < 1 || id > UINT32_MAX || !column_text(statement, 1, profile.name) ||
        !column_text(statement, 2, profile.profession) ||
        !column_text(statement, 3, profile.department))
    {
        (void)snprintf(store->error, sizeof(store->error), "Funcionario persistido invalido.");
        return WORKFORCE_IO_ERROR;
    }
    profile.age = sqlite3_column_int(statement, 4);
    profile.level = (WorkerLevel)sqlite3_column_int(statement, 5);
    profile.salary_cents = sqlite3_column_int64(statement, 6);
    status = worker_init(worker, (uint32_t)id, &profile);
    if (status != WORKFORCE_OK)
    {
        (void)snprintf(store->error, sizeof(store->error), "Perfil persistido invalido.");
        return WORKFORCE_IO_ERROR;
    }
    return load_contracts(store, worker);
}

static WorkforceStatus find_worker(WorkforceStore *store, uint32_t id, Worker *worker)
{
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status = prepare(
        store,
        "SELECT id,name,profession,department,age,level,salary_cents FROM workers WHERE id=?",
        &statement);
    int result;
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    if (sqlite3_bind_int64(statement, 1, id) != SQLITE_OK)
    {
        status = db_error(store);
    }
    else
    {
        result = sqlite3_step(statement);
        if (result == SQLITE_ROW)
        {
            status = read_worker(store, statement, worker);
        }
        else
        {
            status = result == SQLITE_DONE ? WORKFORCE_NOT_FOUND : db_error(store);
        }
    }
    (void)sqlite3_finalize(statement);
    return status;
}

WorkforceStatus store_find(WorkforceStore *store, uint32_t id, Worker *output)
{
    Worker worker = {0};
    WorkforceStatus status;
    if (store == NULL || id == 0 || output == NULL)
    {
        return WORKFORCE_INVALID;
    }
    status = exec_sql(store, "BEGIN");
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    status = finish_write(store, find_worker(store, id, &worker));
    if (status == WORKFORCE_OK)
    {
        *output = worker;
    }
    else
    {
        worker_dispose(&worker);
    }
    return status;
}

WorkforceStatus store_list(WorkforceStore *store, Team *output)
{
    Team team = {0};
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status;
    int result = SQLITE_DONE;
    if (store == NULL || output == NULL)
    {
        return WORKFORCE_INVALID;
    }
    status = exec_sql(store, "BEGIN");
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    status = prepare(
        store,
        "SELECT id,name,profession,department,age,level,salary_cents FROM workers ORDER BY id",
        &statement);
    while (status == WORKFORCE_OK && (result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        Worker worker = {0};
        uint32_t temporary_id;
        status = read_worker(store, statement, &worker);
        if (status == WORKFORCE_OK)
        {
            status = team_add(&team, &worker.profile, &temporary_id);
        }
        if (status == WORKFORCE_OK)
        {
            team.workers[team.count - 1] = worker;
        }
        else
        {
            worker_dispose(&worker);
        }
    }
    if (status == WORKFORCE_OK && result != SQLITE_DONE)
    {
        status = db_error(store);
    }
    (void)sqlite3_finalize(statement);
    status = finish_write(store, status);
    if (status == WORKFORCE_OK)
    {
        if (team.count > 0)
        {
            team.last_id = team.workers[team.count - 1].id;
        }
        *output = team;
    }
    else
    {
        team_dispose(&team);
    }
    return status;
}

WorkforceStatus store_add_worker(WorkforceStore *store, const WorkerProfile *profile, uint32_t *id)
{
    Worker worker = {0};
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status;
    sqlite3_int64 assigned = 0;
    int exhausted = 0;
    if (store == NULL || id == NULL)
    {
        return WORKFORCE_INVALID;
    }
    status = worker_init(&worker, 1, profile);
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    status = exec_sql(store, "BEGIN IMMEDIATE");
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    status = pragma_integer(
        store,
        "SELECT COALESCE((SELECT seq >= 4294967295 FROM sqlite_sequence WHERE name='workers'),0)",
        &exhausted);
    if (status == WORKFORCE_OK && exhausted)
    {
        status = WORKFORCE_OVERFLOW;
    }
    if (status != WORKFORCE_OK)
    {
        return finish_write(store, status);
    }
    status = prepare(store,
                     "INSERT INTO workers(name,profession,department,age,level,salary_cents) "
                     "VALUES(?,?,?,?,?,?)",
                     &statement);
    if (status == WORKFORCE_OK)
    {
        if (sqlite3_bind_text(statement, 1, profile->name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 2, profile->profession, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_bind_text(statement, 3, profile->department, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_bind_int(statement, 4, profile->age) != SQLITE_OK ||
            sqlite3_bind_int(statement, 5, profile->level) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 6, profile->salary_cents) != SQLITE_OK)
        {
            status = db_error(store);
            (void)sqlite3_finalize(statement);
        }
        else
        {
            status = step_write(store, statement);
            assigned = sqlite3_last_insert_rowid(store->db);
        }
    }
    if (status == WORKFORCE_OK && (assigned < 1 || assigned > UINT32_MAX))
    {
        status = WORKFORCE_OVERFLOW;
    }
    status = finish_write(store, status);
    if (status == WORKFORCE_OK)
    {
        *id = (uint32_t)assigned;
    }
    return status;
}

WorkforceStatus store_remove_worker(WorkforceStore *store, uint32_t id)
{
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status;
    if (store == NULL || id == 0)
    {
        return WORKFORCE_INVALID;
    }
    status = prepare(store, "DELETE FROM workers WHERE id=?", &statement);
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    if (sqlite3_bind_int64(statement, 1, id) != SQLITE_OK)
    {
        status = db_error(store);
        (void)sqlite3_finalize(statement);
        return status;
    }
    status = step_write(store, statement);
    return status == WORKFORCE_OK && sqlite3_changes(store->db) == 0 ? WORKFORCE_NOT_FOUND : status;
}

WorkforceStatus store_add_contract(WorkforceStore *store, uint32_t id, HourContract contract)
{
    Worker worker = {0};
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status;
    if (store == NULL || id == 0)
    {
        return WORKFORCE_INVALID;
    }
    status = exec_sql(store, "BEGIN IMMEDIATE");
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    status = find_worker(store, id, &worker);
    if (status == WORKFORCE_OK)
    {
        status = worker_add_contract(&worker, contract);
    }
    worker_dispose(&worker);
    if (status == WORKFORCE_OK)
    {
        status = prepare(
            store,
            "INSERT INTO contracts(worker_id,year,month,hours,hourly_rate_cents) VALUES(?,?,?,?,?)",
            &statement);
    }
    if (status == WORKFORCE_OK)
    {
        if (sqlite3_bind_int64(statement, 1, id) != SQLITE_OK ||
            sqlite3_bind_int(statement, 2, contract.year) != SQLITE_OK ||
            sqlite3_bind_int(statement, 3, contract.month) != SQLITE_OK ||
            sqlite3_bind_int(statement, 4, contract.hours) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 5, contract.hourly_rate_cents) != SQLITE_OK)
        {
            status = db_error(store);
            (void)sqlite3_finalize(statement);
        }
        else
        {
            status = step_write(store, statement);
        }
    }
    return finish_write(store, status);
}

WorkforceStatus store_remove_contract(WorkforceStore *store, uint32_t id, size_t index)
{
    sqlite3_stmt *statement = NULL;
    WorkforceStatus status;
    if (store == NULL || id == 0 || index > INT64_MAX)
    {
        return WORKFORCE_INVALID;
    }
    status = prepare(store,
                     "DELETE FROM contracts WHERE id=(SELECT id FROM contracts WHERE worker_id=? "
                     "ORDER BY id LIMIT 1 OFFSET ?)",
                     &statement);
    if (status != WORKFORCE_OK)
    {
        return status;
    }
    if (sqlite3_bind_int64(statement, 1, id) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)index) != SQLITE_OK)
    {
        status = db_error(store);
        (void)sqlite3_finalize(statement);
        return status;
    }
    status = step_write(store, statement);
    return status == WORKFORCE_OK && sqlite3_changes(store->db) == 0 ? WORKFORCE_NOT_FOUND : status;
}
