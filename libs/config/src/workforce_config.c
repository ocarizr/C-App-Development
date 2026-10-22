#include "workforce_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

static bool fail(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0)
    {
        (void)snprintf(error, size, "%s", message);
    }
    return false;
}

void config_defaults(AppConfig *config)
{
    if (config != NULL)
    {
        *config = (AppConfig){"workforce.db", 5000};
    }
}

static const char *scalar(yaml_document_t *document, int index)
{
    yaml_node_t *node = yaml_document_get_node(document, index);
    if (node == NULL || node->type != YAML_SCALAR_NODE ||
        strlen((const char *)node->data.scalar.value) != node->data.scalar.length)
    {
        return NULL;
    }
    return (const char *)node->data.scalar.value;
}

static bool resolve_path(const char *filename, const char *value, char *output, size_t size)
{
    const char *slash = strrchr(filename, '/');
    const char *backslash = strrchr(filename, '\\');
    size_t prefix = 0;
    size_t length = strlen(value);
    if (length == 0 || strcmp(value, ":memory:") == 0 || strncmp(value, "file:", 5) == 0)
    {
        return false;
    }
    if (value[0] != '/' && value[0] != '\\' && !(length >= 2 && value[1] == ':'))
    {
        if (backslash != NULL && (slash == NULL || backslash > slash))
        {
            slash = backslash;
        }
        if (slash != NULL)
        {
            prefix = (size_t)(slash - filename) + 1;
        }
    }
    if (prefix >= size || length >= size - prefix)
    {
        return false;
    }
    memcpy(output, filename, prefix);
    memcpy(output + prefix, value, length + 1);
    return true;
}

static bool database_mapping(yaml_document_t *document, yaml_node_t *mapping, const char *filename,
                             AppConfig *config)
{
    unsigned int seen = 0;
    yaml_node_pair_t *pair;
    if (mapping == NULL || mapping->type != YAML_MAPPING_NODE)
    {
        return false;
    }
    for (pair = mapping->data.mapping.pairs.start; pair < mapping->data.mapping.pairs.top; ++pair)
    {
        const char *key = scalar(document, pair->key);
        const char *value = scalar(document, pair->value);
        unsigned int bit;
        if (key == NULL || value == NULL)
        {
            return false;
        }
        if (strcmp(key, "path") == 0)
        {
            bit = 1;
            if (!resolve_path(filename, value, config->database_path,
                              sizeof(config->database_path)))
            {
                return false;
            }
        }
        else if (strcmp(key, "busy_timeout_ms") == 0)
        {
            char *end;
            long number;
            bit = 2;
            errno = 0;
            number = strtol(value, &end, 10);
            if (errno != 0 || end == value || *end != '\0' || number < 0 || number > 60000)
            {
                return false;
            }
            config->busy_timeout_ms = (int)number;
        }
        else
        {
            return false;
        }
        if ((seen & bit) != 0)
        {
            return false;
        }
        seen |= bit;
    }
    return true;
}

bool config_load_file(const char *path, const AppConfig *base, AppConfig *output, char *error,
                      size_t error_size)
{
    FILE *file = NULL;
    yaml_parser_t parser;
    yaml_document_t document;
    yaml_document_t extra;
    yaml_node_t *root;
    yaml_node_pair_t *pair;
    AppConfig candidate;
    bool valid = true;
    bool seen = false;
    if (path == NULL || base == NULL || output == NULL)
    {
        return fail(error, error_size, "Argumentos de configuracao invalidos.");
    }
#ifdef _MSC_VER
    (void)fopen_s(&file, path, "rb");
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL)
    {
        return fail(error, error_size, "Nao foi possivel abrir o arquivo de configuracao.");
    }
    if (!yaml_parser_initialize(&parser))
    {
        (void)fclose(file);
        return fail(error, error_size, "Memoria insuficiente para ler YAML.");
    }
    yaml_parser_set_input_file(&parser, file);
    if (!yaml_parser_load(&parser, &document))
    {
        yaml_parser_delete(&parser);
        (void)fclose(file);
        return fail(error, error_size, "YAML invalido.");
    }
    candidate = *base;
    root = yaml_document_get_root_node(&document);
    if (root == NULL || root->type != YAML_MAPPING_NODE)
    {
        valid = false;
    }
    else
    {
        for (pair = root->data.mapping.pairs.start; pair < root->data.mapping.pairs.top; ++pair)
        {
            const char *key = scalar(&document, pair->key);
            if (key == NULL || strcmp(key, "database") != 0 || seen ||
                !database_mapping(&document, yaml_document_get_node(&document, pair->value), path,
                                  &candidate))
            {
                valid = false;
                break;
            }
            seen = true;
        }
    }
    yaml_document_delete(&document);
    if (valid)
    {
        if (!yaml_parser_load(&parser, &extra))
        {
            valid = false;
        }
        else
        {
            valid = yaml_document_get_root_node(&extra) == NULL;
            yaml_document_delete(&extra);
        }
    }
    yaml_parser_delete(&parser);
    if (fclose(file) != 0)
    {
        valid = false;
    }
    if (!valid || candidate.database_path[0] == '\0' || candidate.busy_timeout_ms < 0 ||
        candidate.busy_timeout_ms > 60000)
    {
        return fail(error, error_size,
                    "Configuracao invalida: verifique chaves, valores e documentos YAML.");
    }
    *output = candidate;
    if (error != NULL && error_size > 0)
    {
        error[0] = '\0';
    }
    return true;
}
