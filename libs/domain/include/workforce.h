#ifndef WORKFORCE_H
#define WORKFORCE_H

#include <stddef.h>
#include <stdint.h>

#define WORKFORCE_NAME_SIZE 100

typedef enum
{
    WORKFORCE_OK,
    WORKFORCE_INVALID,
    WORKFORCE_NOT_FOUND,
    WORKFORCE_NO_MEMORY,
    WORKFORCE_OVERFLOW,
    WORKFORCE_IO_ERROR
} WorkforceStatus;

typedef enum
{
    WORKER_TRAINEE,
    WORKER_JUNIOR,
    WORKER_MID_LEVEL,
    WORKER_SENIOR
} WorkerLevel;

typedef struct
{
    char name[WORKFORCE_NAME_SIZE];
    char profession[WORKFORCE_NAME_SIZE];
    char department[WORKFORCE_NAME_SIZE];
    int age;
    WorkerLevel level;
    int64_t salary_cents;
} WorkerProfile;

typedef struct
{
    int year;
    int month;
    int hours;
    int64_t hourly_rate_cents;
} HourContract;

/* Owns contracts. Do not shallow-copy an initialized Worker. */
typedef struct
{
    uint32_t id;
    WorkerProfile profile;
    HourContract *contracts;
    size_t contract_count;
    size_t contract_capacity;
} Worker;

/* Owns workers and their contracts. Initialize with {0}, dispose exactly once.
 * Pointers returned by team_find are invalidated by adding/removing workers. */
typedef struct
{
    Worker *workers;
    size_t count;
    size_t capacity;
    uint32_t last_id;
} Team;

const char *worker_level_name(WorkerLevel level);
WorkforceStatus worker_level_parse(const char *text, WorkerLevel *level);
/* init is for fresh storage only. On failure the output remains unchanged. */
WorkforceStatus worker_init(Worker *worker, uint32_t id, const WorkerProfile *profile);
void worker_dispose(Worker *worker);
WorkforceStatus worker_add_contract(Worker *worker, HourContract contract);
WorkforceStatus worker_remove_contract(Worker *worker, size_t index);
/* Monthly base salary plus contracts matching year/month, with checked arithmetic. */
WorkforceStatus worker_income(const Worker *worker, int year, int month, int64_t *cents);
WorkforceStatus team_add(Team *team, const WorkerProfile *profile, uint32_t *id);
Worker *team_find(Team *team, uint32_t id);
WorkforceStatus team_remove(Team *team, uint32_t id);
void team_dispose(Team *team);

#endif
