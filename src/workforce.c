#include "workforce.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int valid_name(const char *name)
{
    size_t i;
    int has_text = 0;
    for (i = 0; i < WORKFORCE_NAME_SIZE; ++i) {
        if (name[i] == '\0') {
            return has_text;
        }
        if (!isspace((unsigned char)name[i])) {
            has_text = 1;
        }
    }
    return 0;
}

static int valid_period(int year, int month)
{
    return year >= 1 && year <= 9999 && month >= 1 && month <= 12;
}

const char *worker_level_name(WorkerLevel level)
{
    switch (level) {
    case WORKER_TRAINEE: return "Trainee";
    case WORKER_JUNIOR: return "Junior";
    case WORKER_MID_LEVEL: return "MidLevel";
    case WORKER_SENIOR: return "Senior";
    default: return "Unknown";
    }
}

WorkforceStatus worker_level_parse(const char *text, WorkerLevel *level)
{
    int i;
    if (text == NULL || level == NULL) {
        return WORKFORCE_INVALID;
    }
    for (i = WORKER_TRAINEE; i <= WORKER_SENIOR; ++i) {
        if (strcmp(text, worker_level_name((WorkerLevel)i)) == 0) {
            *level = (WorkerLevel)i;
            return WORKFORCE_OK;
        }
    }
    return WORKFORCE_INVALID;
}

WorkforceStatus worker_init(Worker *worker, uint32_t id, const WorkerProfile *profile)
{
    Worker initialized = {0};
    if (worker == NULL || profile == NULL || id == 0 ||
        !valid_name(profile->name) || !valid_name(profile->profession) ||
        !valid_name(profile->department) || profile->age < 1 || profile->age > 150 ||
        profile->level < WORKER_TRAINEE || profile->level > WORKER_SENIOR ||
        profile->salary_cents < 0) {
        return WORKFORCE_INVALID;
    }
    initialized.id = id;
    initialized.profile = *profile;
    *worker = initialized;
    return WORKFORCE_OK;
}

void worker_dispose(Worker *worker)
{
    if (worker != NULL) {
        free(worker->contracts);
        *worker = (Worker){0};
    }
}

WorkforceStatus worker_add_contract(Worker *worker, HourContract contract)
{
    int64_t income;
    int64_t value;
    WorkforceStatus status;
    if (worker == NULL || worker->id == 0 || !valid_period(contract.year, contract.month) ||
        contract.hours <= 0 || contract.hourly_rate_cents < 0) {
        return WORKFORCE_INVALID;
    }
    if (contract.hourly_rate_cents > INT64_MAX / contract.hours) {
        return WORKFORCE_OVERFLOW;
    }
    value = contract.hourly_rate_cents * contract.hours;
    status = worker_income(worker, contract.year, contract.month, &income);
    if (status != WORKFORCE_OK) {
        return status;
    }
    if (income > INT64_MAX - value) {
        return WORKFORCE_OVERFLOW;
    }
    if (worker->contract_count == worker->contract_capacity) {
        size_t capacity = worker->contract_capacity == 0 ? 4 : worker->contract_capacity * 2;
        HourContract *resized;
        if (capacity < worker->contract_capacity || capacity > SIZE_MAX / sizeof(*resized)) {
            return WORKFORCE_OVERFLOW;
        }
        resized = realloc(worker->contracts, capacity * sizeof(*resized));
        if (resized == NULL) {
            return WORKFORCE_NO_MEMORY;
        }
        worker->contracts = resized;
        worker->contract_capacity = capacity;
    }
    worker->contracts[worker->contract_count++] = contract;
    return WORKFORCE_OK;
}

WorkforceStatus worker_remove_contract(Worker *worker, size_t index)
{
    if (worker == NULL || worker->id == 0) {
        return WORKFORCE_INVALID;
    }
    if (index >= worker->contract_count) {
        return WORKFORCE_NOT_FOUND;
    }
    memmove(&worker->contracts[index], &worker->contracts[index + 1],
            (worker->contract_count - index - 1) * sizeof(*worker->contracts));
    --worker->contract_count;
    return WORKFORCE_OK;
}

WorkforceStatus worker_income(const Worker *worker, int year, int month, int64_t *cents)
{
    int64_t total;
    size_t i;
    if (worker == NULL || worker->id == 0 || cents == NULL || !valid_period(year, month)) {
        return WORKFORCE_INVALID;
    }
    total = worker->profile.salary_cents;
    for (i = 0; i < worker->contract_count; ++i) {
        const HourContract *contract = &worker->contracts[i];
        if (contract->year == year && contract->month == month) {
            int64_t value;
            if (contract->hourly_rate_cents > INT64_MAX / contract->hours) {
                return WORKFORCE_OVERFLOW;
            }
            value = contract->hourly_rate_cents * contract->hours;
            if (total > INT64_MAX - value) {
                return WORKFORCE_OVERFLOW;
            }
            total += value;
        }
    }
    *cents = total;
    return WORKFORCE_OK;
}

WorkforceStatus team_add(Team *team, const WorkerProfile *profile, uint32_t *id)
{
    Worker worker;
    WorkforceStatus status;
    if (team == NULL || id == NULL) {
        return WORKFORCE_INVALID;
    }
    if (team->last_id == UINT32_MAX) {
        return WORKFORCE_OVERFLOW;
    }
    status = worker_init(&worker, team->last_id + 1, profile);
    if (status != WORKFORCE_OK) {
        return status;
    }
    if (team->count == team->capacity) {
        size_t capacity = team->capacity == 0 ? 4 : team->capacity * 2;
        Worker *resized;
        if (capacity < team->capacity || capacity > SIZE_MAX / sizeof(*resized)) {
            return WORKFORCE_OVERFLOW;
        }
        resized = realloc(team->workers, capacity * sizeof(*resized));
        if (resized == NULL) {
            return WORKFORCE_NO_MEMORY;
        }
        team->workers = resized;
        team->capacity = capacity;
    }
    team->workers[team->count++] = worker;
    *id = ++team->last_id;
    return WORKFORCE_OK;
}

Worker *team_find(Team *team, uint32_t id)
{
    size_t i;
    if (team != NULL) {
        for (i = 0; i < team->count; ++i) {
            if (team->workers[i].id == id) {
                return &team->workers[i];
            }
        }
    }
    return NULL;
}

WorkforceStatus team_remove(Team *team, uint32_t id)
{
    Worker *worker;
    size_t index;
    if (team == NULL) {
        return WORKFORCE_INVALID;
    }
    worker = team_find(team, id);
    if (worker == NULL) {
        return WORKFORCE_NOT_FOUND;
    }
    index = (size_t)(worker - team->workers);
    worker_dispose(worker);
    memmove(&team->workers[index], &team->workers[index + 1],
            (team->count - index - 1) * sizeof(*team->workers));
    --team->count;
    team->workers[team->count] = (Worker){0};
    return WORKFORCE_OK;
}

void team_dispose(Team *team)
{
    if (team != NULL) {
        size_t i;
        for (i = 0; i < team->count; ++i) {
            worker_dispose(&team->workers[i]);
        }
        free(team->workers);
        *team = (Team){0};
    }
}
