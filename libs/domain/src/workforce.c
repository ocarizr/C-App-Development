#include "workforce.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int valid_name(const char *name)
{
    size_t i;
    int has_text = 0;
    for (i = 0; i < WORKFORCE_NAME_SIZE; ++i)
    {
        if (name[i] == '\0')
        {
            return has_text;
        }

        if (!isspace((unsigned char)name[i]))
        {
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
    switch (level)
    {
    case WORKER_TRAINEE:
        return "Trainee";
    case WORKER_JUNIOR:
        return "Junior";
    case WORKER_MID_LEVEL:
        return "MidLevel";
    case WORKER_SENIOR:
        return "Senior";
    default:
        return "Unknown";
    }
}

WorkforceStatus worker_level_parse(const char *text, WorkerLevel *level)
{
    int i;
    if (text == NULL || level == NULL)
    {
        return WORKFORCE_INVALID;
    }

    for (i = WORKER_TRAINEE; i <= WORKER_SENIOR; ++i)
    {
        if (strcmp(text, worker_level_name((WorkerLevel)i)) == 0)
        {
            *level = (WorkerLevel)i;
            return WORKFORCE_OK;
        }
    }

    return WORKFORCE_INVALID;
}

WorkforceStatus worker_init(Worker *worker, uint32_t id, const WorkerProfile *profile)
{
    Worker initialized = {0};
    if (worker == NULL || profile == NULL || id == 0 || !valid_name(profile->name) ||
        !valid_name(profile->profession) || !valid_name(profile->department) || profile->age < 1 ||
        profile->age > 150 || profile->level < WORKER_TRAINEE || profile->level > WORKER_SENIOR ||
        profile->salary_cents < 0)
    {
        return WORKFORCE_INVALID;
    }

    initialized.id = id;
    initialized.profile = *profile;
    *worker = initialized;
    return WORKFORCE_OK;
}

void worker_dispose(Worker *worker)
{
    if (worker != NULL)
    {
        free(worker->contracts);
        free(worker->salary_changes);
        *worker = (Worker){0};
    }
}

WorkforceStatus worker_update_profile(Worker *worker, const WorkerProfile *profile)
{
    Worker validated = {0};
    WorkforceStatus status;
    if (worker == NULL || worker->id == 0 || profile == NULL ||
        profile->salary_cents != worker->profile.salary_cents)
    {
        return WORKFORCE_INVALID;
    }

    status = worker_init(&validated, worker->id, profile);
    if (status == WORKFORCE_OK)
    {
        worker->profile = validated.profile;
    }

    return status;
}

WorkforceStatus worker_salary_at(const Worker *worker, int year, int month, int64_t *cents)
{
    if (worker == NULL || worker->id == 0 || cents == NULL || !valid_period(year, month))
    {
        return WORKFORCE_INVALID;
    }

    *cents = worker->profile.salary_cents;
    for (size_t i = 0; i < worker->salary_count; ++i)
    {
        const SalaryChange *change = &worker->salary_changes[i];
        if (change->year * 12 + change->month > year * 12 + month)
        {
            break;
        }

        *cents = change->salary_cents;
    }

    return WORKFORCE_OK;
}

static WorkforceStatus replace_history(Worker *worker, SalaryChange *changes, size_t count)
{
    /* The candidate borrows contracts; only this function consumes changes.
     * Ownership transfers to worker only after every affected month validates. */
    Worker candidate = *worker;
    candidate.salary_changes = changes;
    candidate.salary_count = count;
    for (size_t i = 0; i < worker->contract_count; ++i)
    {
        int64_t income;
        WorkforceStatus status = worker_income(&candidate, worker->contracts[i].year,
                                               worker->contracts[i].month, &income);
        if (status != WORKFORCE_OK)
        {
            free(changes);
            return status;
        }
    }

    free(worker->salary_changes);
    worker->salary_changes = changes;
    worker->salary_count = count;
    return WORKFORCE_OK;
}

WorkforceStatus worker_set_salary(Worker *worker, SalaryChange change)
{
    size_t index = 0;
    size_t count;
    size_t offset;
    SalaryChange *changes;
    if (worker == NULL || worker->id == 0 || !valid_period(change.year, change.month) ||
        change.salary_cents < 0)
    {
        return WORKFORCE_INVALID;
    }

    while (index < worker->salary_count &&
           worker->salary_changes[index].year * 12 + worker->salary_changes[index].month <
               change.year * 12 + change.month)
    {
        ++index;
    }

    offset = index < worker->salary_count && worker->salary_changes[index].year == change.year &&
                     worker->salary_changes[index].month == change.month
                 ? 0
                 : 1;
    if (worker->salary_count >= SIZE_MAX / sizeof(*changes))
    {
        return WORKFORCE_OVERFLOW;
    }

    count = worker->salary_count + offset;
    changes = malloc(count * sizeof(*changes));
    if (changes == NULL)
    {
        return WORKFORCE_NO_MEMORY;
    }

    for (size_t i = 0; i < index; ++i)
    {
        changes[i] = worker->salary_changes[i];
    }

    changes[index] = change;
    for (size_t i = index + 1; i < count; ++i)
    {
        changes[i] = worker->salary_changes[i - offset];
    }

    return replace_history(worker, changes, count);
}

WorkforceStatus worker_remove_salary(Worker *worker, int year, int month)
{
    size_t index = 0;
    size_t count;
    SalaryChange *changes = NULL;
    if (worker == NULL || worker->id == 0 || !valid_period(year, month))
    {
        return WORKFORCE_INVALID;
    }

    while (index < worker->salary_count && (worker->salary_changes[index].year != year ||
                                            worker->salary_changes[index].month != month))
    {
        ++index;
    }

    if (index == worker->salary_count)
    {
        return WORKFORCE_NOT_FOUND;
    }

    count = worker->salary_count - 1;
    if (count > 0)
    {
        changes = malloc(count * sizeof(*changes));
        if (changes == NULL)
        {
            return WORKFORCE_NO_MEMORY;
        }

        for (size_t i = 0; i < count; ++i)
        {
            changes[i] = worker->salary_changes[i < index ? i : i + 1];
        }
    }

    return replace_history(worker, changes, count);
}

WorkforceStatus worker_add_contract(Worker *worker, HourContract contract)
{
    int64_t income;
    int64_t value;
    WorkforceStatus status;
    if (worker == NULL || worker->id == 0 || !valid_period(contract.year, contract.month) ||
        contract.hours <= 0 || contract.hourly_rate_cents < 0 || contract.id < 0 ||
        (contract.id > 0 && worker_find_contract(worker, contract.id) != NULL))
    {
        return WORKFORCE_INVALID;
    }

    if (contract.hourly_rate_cents > INT64_MAX / contract.hours)
    {
        return WORKFORCE_OVERFLOW;
    }

    value = contract.hourly_rate_cents * contract.hours;
    status = worker_income(worker, contract.year, contract.month, &income);
    if (status != WORKFORCE_OK)
    {
        return status;
    }

    if (income > INT64_MAX - value)
    {
        return WORKFORCE_OVERFLOW;
    }

    if (contract.id == 0)
    {
        if (worker->last_contract_id == INT64_MAX)
        {
            return WORKFORCE_OVERFLOW;
        }

        contract.id = worker->last_contract_id + 1;
    }

    if (worker->contract_count == worker->contract_capacity)
    {
        size_t capacity = worker->contract_capacity == 0 ? 4 : worker->contract_capacity * 2;
        HourContract *resized;
        if (capacity < worker->contract_capacity || capacity > SIZE_MAX / sizeof(*resized))
        {
            return WORKFORCE_OVERFLOW;
        }

        /* Keep the original allocation and capacity intact when growth fails. */
        resized = realloc(worker->contracts, capacity * sizeof(*resized));
        if (resized == NULL)
        {
            return WORKFORCE_NO_MEMORY;
        }

        worker->contracts = resized;
        worker->contract_capacity = capacity;
    }

    worker->contracts[worker->contract_count++] = contract;
    if (contract.id > worker->last_contract_id)
    {
        worker->last_contract_id = contract.id;
    }

    return WORKFORCE_OK;
}

const HourContract *worker_find_contract(const Worker *worker, int64_t contract_id)
{
    if (worker != NULL && contract_id > 0)
    {
        for (size_t i = 0; i < worker->contract_count; ++i)
        {
            if (worker->contracts[i].id == contract_id)
            {
                return &worker->contracts[i];
            }
        }
    }

    return NULL;
}

WorkforceStatus worker_update_contract(Worker *worker, int64_t contract_id,
                                       HourContract replacement)
{
    const HourContract *original;
    int64_t income;
    int64_t value;
    WorkforceStatus status;
    if (worker == NULL || worker->id == 0 || contract_id <= 0 ||
        !valid_period(replacement.year, replacement.month) || replacement.hours <= 0 ||
        replacement.hourly_rate_cents < 0 || (replacement.id != 0 && replacement.id != contract_id))
    {
        return WORKFORCE_INVALID;
    }

    original = worker_find_contract(worker, contract_id);
    if (original == NULL)
    {
        return WORKFORCE_NOT_FOUND;
    }

    if (replacement.hourly_rate_cents > INT64_MAX / replacement.hours)
    {
        return WORKFORCE_OVERFLOW;
    }

    status = worker_income(worker, replacement.year, replacement.month, &income);
    if (status != WORKFORCE_OK)
    {
        return status;
    }

    if (original->year == replacement.year && original->month == replacement.month)
    {
        income -= original->hours * original->hourly_rate_cents;
    }

    value = replacement.hours * replacement.hourly_rate_cents;
    if (income > INT64_MAX - value)
    {
        return WORKFORCE_OVERFLOW;
    }

    replacement.id = contract_id;
    worker->contracts[(size_t)(original - worker->contracts)] = replacement;
    return WORKFORCE_OK;
}

WorkforceStatus worker_remove_contract(Worker *worker, int64_t contract_id)
{
    const HourContract *contract;
    size_t index;
    if (worker == NULL || worker->id == 0 || contract_id <= 0)
    {
        return WORKFORCE_INVALID;
    }

    contract = worker_find_contract(worker, contract_id);
    if (contract == NULL)
    {
        return WORKFORCE_NOT_FOUND;
    }

    index = (size_t)(contract - worker->contracts);
    memmove(&worker->contracts[index], &worker->contracts[index + 1],
            (worker->contract_count - index - 1) * sizeof(*worker->contracts));
    --worker->contract_count;
    return WORKFORCE_OK;
}

WorkforceStatus worker_income(const Worker *worker, int year, int month, int64_t *cents)
{
    int64_t total;
    size_t i;
    if (worker == NULL || worker->id == 0 || cents == NULL || !valid_period(year, month))
    {
        return WORKFORCE_INVALID;
    }

    (void)worker_salary_at(worker, year, month, &total);
    for (i = 0; i < worker->contract_count; ++i)
    {
        const HourContract *contract = &worker->contracts[i];
        if (contract->year == year && contract->month == month)
        {
            int64_t value;
            if (contract->hourly_rate_cents > INT64_MAX / contract->hours)
            {
                return WORKFORCE_OVERFLOW;
            }

            value = contract->hourly_rate_cents * contract->hours;
            if (total > INT64_MAX - value)
            {
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
    if (team == NULL || id == NULL)
    {
        return WORKFORCE_INVALID;
    }

    if (team->last_id == UINT32_MAX)
    {
        return WORKFORCE_OVERFLOW;
    }

    status = worker_init(&worker, team->last_id + 1, profile);
    if (status != WORKFORCE_OK)
    {
        return status;
    }

    if (team->count == team->capacity)
    {
        size_t capacity = team->capacity == 0 ? 4 : team->capacity * 2;
        Worker *resized;
        if (capacity < team->capacity || capacity > SIZE_MAX / sizeof(*resized))
        {
            return WORKFORCE_OVERFLOW;
        }

        resized = realloc(team->workers, capacity * sizeof(*resized));
        if (resized == NULL)
        {
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
    if (team != NULL)
    {
        for (i = 0; i < team->count; ++i)
        {
            if (team->workers[i].id == id)
            {
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
    if (team == NULL)
    {
        return WORKFORCE_INVALID;
    }

    worker = team_find(team, id);
    if (worker == NULL)
    {
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
    if (team != NULL)
    {
        size_t i;
        for (i = 0; i < team->count; ++i)
        {
            worker_dispose(&team->workers[i]);
        }

        free(team->workers);
        *team = (Team){0};
    }
}
