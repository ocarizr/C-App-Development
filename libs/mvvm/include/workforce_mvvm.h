#ifndef WORKFORCE_MVVM_H
#define WORKFORCE_MVVM_H

#include <stdbool.h>
#include <stddef.h>

/* Opaque, owned property/command registry. It borrows command contexts.
 * Values are UTF-8 strings, at most 65535 bytes. No expression evaluation.
 * All APIs are single-threaded. Returned values expire on the next write. */
typedef struct Mvvm Mvvm;
typedef bool (*MvvmCommand)(void *context);
Mvvm *mvvm_create(void);
void mvvm_destroy(Mvvm *model);
bool mvvm_property(Mvvm *model, const char *name, const char *value, bool writable);
const char *mvvm_get(const Mvvm *model, const char *name);
bool mvvm_writable(const Mvvm *model, const char *name);
/* set is a view edit; publish also updates read-only properties from the VM. */
bool mvvm_set(Mvvm *model, const char *name, const char *value);
bool mvvm_publish(Mvvm *model, const char *name, const char *value);
bool mvvm_command(Mvvm *model, const char *name, MvvmCommand callback, void *context);
bool mvvm_has_command(const Mvvm *model, const char *name);
bool mvvm_execute(Mvvm *model, const char *name);
size_t mvvm_revision(const Mvvm *model);
#endif
