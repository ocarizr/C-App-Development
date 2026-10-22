#ifndef WORKFORCE_STORE_H
#define WORKFORCE_STORE_H

#include "workforce.h"
#include <stdbool.h>

#define WORKFORCE_STORE_ERROR_SIZE 256

typedef struct WorkforceStore WorkforceStore;
typedef struct
{
    const char *path;
    int busy_timeout_ms;
} StoreOptions;

/* Store owns a connection. Options are copied/consumed during open, not retained.
 * Parent directories must exist. No console I/O or configuration-file dependency. */
WorkforceStatus store_open(const StoreOptions *options, WorkforceStore **store, bool *created,
                           char error[WORKFORCE_STORE_ERROR_SIZE]);
void store_close(WorkforceStore *store);
const char *store_error(const WorkforceStore *store);
/* Outputs must be zero-initialized/fresh storage. On failure they are unchanged.
 * Each read is a fresh, consistent database snapshot. Call the domain dispose functions. */
WorkforceStatus store_list(WorkforceStore *store, Team *team);
WorkforceStatus store_find(WorkforceStore *store, uint32_t id, Worker *worker);
WorkforceStatus store_add_worker(WorkforceStore *store, const WorkerProfile *profile, uint32_t *id);
WorkforceStatus store_remove_worker(WorkforceStore *store, uint32_t id);
WorkforceStatus store_add_contract(WorkforceStore *store, uint32_t worker_id,
                                   HourContract contract);
/* Index is zero-based in contract ID order, matching the console's current listing. */
WorkforceStatus store_remove_contract(WorkforceStore *store, uint32_t worker_id, size_t index);

#endif
