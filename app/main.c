#include "console.h"
#include "workforce_config.h"

#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    AppConfig config;
    WorkforceStore *store = NULL;
    StoreOptions options;
    bool created = false;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    int result;
    int i;
    config_defaults(&config);
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        puts("Uso: workforce [--config arquivo.yaml]...\nSem argumentos: le app.yaml no diretorio "
             "atual.");
        return EXIT_SUCCESS;
    }
    for (i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "--config") != 0 || i + 1 >= argc)
        {
            fputs("Argumentos invalidos. Use workforce --help.\n", stderr);
            return EXIT_FAILURE;
        }
        if (!config_load_file(argv[i + 1], &config, &config, error, sizeof(error)))
        {
            fprintf(stderr, "Erro de configuracao (%s): %s\n", argv[i + 1], error);
            return EXIT_FAILURE;
        }
    }
    if (argc == 1 && !config_load_file("app.yaml", &config, &config, error, sizeof(error)))
    {
        fprintf(stderr, "Erro de configuracao (app.yaml): %s\n", error);
        return EXIT_FAILURE;
    }
    options = (StoreOptions){config.database_path, config.busy_timeout_ms};
    if (store_open(&options, &store, &created, error) != WORKFORCE_OK)
    {
        fprintf(stderr, "Nao foi possivel abrir o banco de dados: %s\n", error);
        return EXIT_FAILURE;
    }
    if (created)
    {
        printf("O banco de dados nao existia; um novo foi criado: %s\n", config.database_path);
    }
    else
    {
        printf("Conexao com o banco de dados realizada com sucesso: %s\n", config.database_path);
    }
    result = console_run(stdin, stdout, store);
    store_close(store);
    return result;
}
