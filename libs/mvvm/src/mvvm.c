#include "workforce_mvvm.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    char name[64];
    char *value;
    bool writable;
} Property;
typedef struct
{
    char name[64];
    MvvmCommand callback;
    void *context;
} Command;
struct Mvvm
{
    Property properties[64];
    Command commands[32];
    size_t count, command_count, revision;
};
static char *copy_value(const char *value)
{
    char *copy;
    size_t length;
    if (value == NULL || (length = strlen(value)) > 65535)
    {
        return NULL;
    }
    copy = malloc(length + 1);
    if (copy != NULL)
    {
        memcpy(copy, value, length + 1);
    }
    return copy;
}
static bool valid_name(const char *name)
{
    return name != NULL && name[0] != '\0' && strlen(name) < 64;
}
static Property *property(Mvvm *model, const char *name)
{
    if (model != NULL && name != NULL)
    {
        for (size_t i = 0; i < model->count; ++i)
        {
            if (strcmp(model->properties[i].name, name) == 0)
            {
                return &model->properties[i];
            }
        }
    }
    return NULL;
}
Mvvm *mvvm_create(void)
{
    return calloc(1, sizeof(Mvvm));
}
void mvvm_destroy(Mvvm *model)
{
    if (model != NULL)
    {
        for (size_t i = 0; i < model->count; ++i)
        {
            free(model->properties[i].value);
        }
        free(model);
    }
}
const char *mvvm_get(const Mvvm *model, const char *name)
{
    if (model != NULL && name != NULL)
    {
        for (size_t i = 0; i < model->count; ++i)
        {
            if (strcmp(model->properties[i].name, name) == 0)
            {
                return model->properties[i].value;
            }
        }
    }
    return NULL;
}
bool mvvm_writable(const Mvvm *model, const char *name)
{
    if (model != NULL && name != NULL)
    {
        for (size_t i = 0; i < model->count; ++i)
        {
            if (strcmp(model->properties[i].name, name) == 0)
            {
                return model->properties[i].writable;
            }
        }
    }
    return false;
}
bool mvvm_property(Mvvm *model, const char *name, const char *value, bool writable)
{
    char *copy;
    Property *entry;
    if (model == NULL || !valid_name(name) || model->count == 64 || mvvm_get(model, name) != NULL ||
        (copy = copy_value(value)) == NULL)
    {
        return false;
    }
    entry = &model->properties[model->count++];
    memcpy(entry->name, name, strlen(name) + 1);
    entry->value = copy;
    entry->writable = writable;
    ++model->revision;
    return true;
}
bool mvvm_publish(Mvvm *model, const char *name, const char *value)
{
    Property *entry = property(model, name);
    char *copy;
    if (entry == NULL || (copy = copy_value(value)) == NULL)
    {
        return false;
    }
    free(entry->value);
    entry->value = copy;
    ++model->revision;
    return true;
}
bool mvvm_set(Mvvm *model, const char *name, const char *value)
{
    return mvvm_writable(model, name) && mvvm_publish(model, name, value);
}
bool mvvm_has_command(const Mvvm *model, const char *name)
{
    if (model != NULL && name != NULL)
    {
        for (size_t i = 0; i < model->command_count; ++i)
        {
            if (strcmp(model->commands[i].name, name) == 0)
            {
                return true;
            }
        }
    }
    return false;
}
bool mvvm_command(Mvvm *model, const char *name, MvvmCommand callback, void *context)
{
    Command *entry;
    if (model == NULL || !valid_name(name) || callback == NULL || model->command_count == 32 ||
        mvvm_has_command(model, name))
    {
        return false;
    }
    entry = &model->commands[model->command_count++];
    memcpy(entry->name, name, strlen(name) + 1);
    entry->callback = callback;
    entry->context = context;
    return true;
}
bool mvvm_execute(Mvvm *model, const char *name)
{
    if (model != NULL && name != NULL)
    {
        for (size_t i = 0; i < model->command_count; ++i)
        {
            if (strcmp(model->commands[i].name, name) == 0)
            {
                return model->commands[i].callback(model->commands[i].context);
            }
        }
    }
    return false;
}
size_t mvvm_revision(const Mvvm *model)
{
    return model == NULL ? 0 : model->revision;
}
