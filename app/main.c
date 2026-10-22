#include "workforce_application.h"
#include "workforce_config.h"
#include "workforce_ui.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static bool assets(const char *executable, char *path, size_t capacity)
{
    char located[4096];
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, located, sizeof(located));
    if (length == 0 || length >= sizeof(located))
    {
        return false;
    }

    executable = located;
#else
    /* argv[0] may be only a command name when launched through PATH. */
    if (strchr(executable, '/') == NULL)
    {
        const char *environment = getenv("PATH");
        const char *entry = environment == NULL ? "" : environment;
        bool found = false;
        do
        {
            const char *end = strchr(entry, ':');
            size_t length = end == NULL ? strlen(entry) : (size_t)(end - entry);
            if (length + strlen(executable) + 2 < sizeof(located))
            {
                /* PATH entries are slices, not terminated strings. Build the
                 * complete candidate in one bounded, terminating operation. */
                int written = snprintf(located, sizeof(located), "%.*s%s%s", (int)length, entry,
                                       length > 0 ? "/" : "", executable);
                if (written < 0 || (size_t)written >= sizeof(located))
                {
                    return false;
                }

                FILE *file = fopen(located, "rb");
                if (file != NULL)
                {
                    fclose(file);
                    executable = located;
                    found = true;
                    break;
                }
            }

            if (end == NULL)
            {
                break;
            }

            entry = end + 1;
        } while (*entry != '\0');
        if (!found)
        {
            return false;
        }
    }
#endif
    const char *slash = strrchr(executable, '/'), *backslash = strrchr(executable, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash))
    {
        slash = backslash;
    }

    size_t prefix = slash == NULL ? 0 : (size_t)(slash - executable + 1);
    if (prefix + sizeof("ui/application.xml") > capacity)
    {
        return false;
    }

    memcpy(path, executable, prefix);
    memcpy(path + prefix, "ui/application.xml", sizeof("ui/application.xml"));
    return true;
}

int main(int argc, char **argv)
{
    AppConfig config;
    WorkforceStore *store = NULL;
    WorkforceViewModel *vm;
    UiDocument *ui;
    StoreOptions options;
    bool created = false, configured = false;
    char error[1024], default_ui[4096];
    const char *ui_path = NULL, *script_path = NULL;
    int result;
    config_defaults(&config);
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        puts("Uso: workforce [--config arquivo.yaml]... [--ui application.xml] [--script arquivo]\n"
             "Configuracao padrao: app.yaml no diretorio atual. UI: ao lado do executavel.\n"
             "Tab: foco; Enter: acao; F1/F2: abas; Esc: sair. Scripts: docs/ui-authoring.md.");
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i += 2)
    {
        if (i + 1 >= argc)
        {
            fputs("Argumentos invalidos. Use workforce --help.\n", stderr);
            return EXIT_FAILURE;
        }

        if (strcmp(argv[i], "--config") == 0)
        {
            configured = true;
            if (!config_load_file(argv[i + 1], &config, &config, error, sizeof(error)))
            {
                fprintf(stderr, "Erro de configuracao (%s): %s\n", argv[i + 1], error);
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "--ui") == 0 && ui_path == NULL)
        {
            ui_path = argv[i + 1];
        }
        else if (strcmp(argv[i], "--script") == 0 && script_path == NULL)
        {
            script_path = argv[i + 1];
        }
        else
        {
            fputs("Argumentos invalidos. Use workforce --help.\n", stderr);
            return EXIT_FAILURE;
        }
    }

    if (!configured && !config_load_file("app.yaml", &config, &config, error, sizeof(error)))
    {
        fprintf(stderr, "Erro de configuracao (app.yaml): %s\n", error);
        return EXIT_FAILURE;
    }

    if (ui_path == NULL)
    {
        if (!assets(argv[0], default_ui, sizeof(default_ui)))
        {
            fputs("Nao foi possivel localizar a UI. Use --ui application.xml.\n", stderr);
            return EXIT_FAILURE;
        }

        ui_path = default_ui;
    }

    options = (StoreOptions){config.database_path, config.busy_timeout_ms};
    if (store_open(&options, &store, &created, error) != WORKFORCE_OK)
    {
        fprintf(stderr, "Nao foi possivel abrir o banco de dados: %s\n", error);
        return EXIT_FAILURE;
    }

    vm = workforce_vm_create(store);
    if (vm == NULL)
    {
        fputs("Falha ao inicializar o ViewModel.\n", stderr);
        store_close(store);
        return EXIT_FAILURE;
    }

    const char *notice = created ? "O banco de dados nao existia; um novo foi criado."
                                 : "Conexao com o banco de dados realizada com sucesso.";
    printf("%s %s\n", notice, config.database_path);
    (void)mvvm_publish(workforce_vm_bindings(vm), "status", notice);
    ui = ui_load(ui_path, workforce_vm_bindings(vm), error, sizeof(error));
    if (ui == NULL)
    {
        fprintf(stderr, "Erro de UI (%s): %s\n", ui_path, error);
        workforce_vm_destroy(vm);
        store_close(store);
        return EXIT_FAILURE;
    }

    if (script_path != NULL)
    {
        FILE *script = fopen(script_path, "rb");
        if (script == NULL)
        {
            fprintf(stderr, "Nao foi possivel ler script: %s\n", script_path);
            result = EXIT_FAILURE;
        }
        else
        {
            result = ui_script(ui, script, stdout);
            fclose(script);
        }
    }
    else
    {
        result = ui_run(ui);
    }

    ui_destroy(ui);
    workforce_vm_destroy(vm);
    store_close(store);
    return result;
}
