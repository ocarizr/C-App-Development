#ifndef WORKFORCE_CONFIG_H
#define WORKFORCE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define WORKFORCE_PATH_SIZE 1024
#define WORKFORCE_CONFIG_ERROR_SIZE 256

typedef struct
{
    char database_path[WORKFORCE_PATH_SIZE];
    int busy_timeout_ms;
} AppConfig;

void config_defaults(AppConfig *config);
/* Merge one YAML file over base. Relative paths are relative to that YAML file.
 * Unknown/duplicate keys and invalid values fail without modifying output.
 * Readable files are required; there is no silent fallback on invalid config. */
bool config_load_file(const char *path, const AppConfig *base, AppConfig *output, char *error,
                      size_t error_size);

#endif
