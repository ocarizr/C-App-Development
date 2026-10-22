#ifndef WORKFORCE_APPLICATION_H
#define WORKFORCE_APPLICATION_H
#include "workforce_mvvm.h"
#include "workforce_store.h"
/* Owns bindings and commands, borrows store until destruction. No terminal/XML dependency.
 * Reads acquire fresh repository snapshots; command failures publish a status.
 * A failed view refresh after a successful write is reported explicitly. */
typedef struct WorkforceViewModel WorkforceViewModel;
WorkforceViewModel *workforce_vm_create(WorkforceStore *store);
Mvvm *workforce_vm_bindings(WorkforceViewModel *vm);
void workforce_vm_destroy(WorkforceViewModel *vm);
#endif
