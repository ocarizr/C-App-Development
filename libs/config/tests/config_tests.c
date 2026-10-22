#include "test_files.h"
#include "workforce_config.h"

#include <string.h>

static void write_config(const char *path, const char *text)
{
    FILE *file = test_file(path, "wb");
    CHECK(fputs(text, file) >= 0);
    CHECK(fclose(file) == 0);
}

int main(void)
{
    const char *path = "fixtures/config-test.yaml";
    AppConfig base;
    AppConfig output;
    char error[WORKFORCE_CONFIG_ERROR_SIZE];
    const char *invalid[] = {"",
                             "[abc]",
                             "database: [broken",
                             "database: []",
                             "unknown: value",
                             "database: {unknown: 1}",
                             "database: {path: ''}",
                             "database: {path: ':memory:'}",
                             "database: {path: 'file:memory?mode=memory'}",
                             "database: {path: []}",
                             "database: {path: one, path: two}",
                             "database: {}\ndatabase: {}",
                             "database: {busy_timeout_ms: -1}",
                             "database: {busy_timeout_ms: 60001}",
                             "database: {busy_timeout_ms: nan}",
                             "database: {busy_timeout_ms: 1.5}",
                             "database: {busy_timeout_ms: 99999999999999999999999999999999}",
                             "database: {}\n---\ndatabase: {}"};
    size_t i;
    config_defaults(&base);
    CHECK(strcmp(base.database_path, "workforce.db") == 0 && base.busy_timeout_ms == 5000);
    write_config(path, "# Real YAML: quoting, spaces, inline mapping\n"
                       "database: {path: 'team data.db', busy_timeout_ms: 123}\n");
    CHECK(config_load_file(path, &base, &output, error, sizeof(error)));
    CHECK(strcmp(output.database_path, "fixtures/team data.db") == 0);
    CHECK(output.busy_timeout_ms == 123 && error[0] == '\0');
    write_config(path, "database:\n  busy_timeout_ms: 0\n");
    CHECK(config_load_file(path, &output, &output, error, sizeof(error)));
    CHECK(strcmp(output.database_path, "fixtures/team data.db") == 0);
    CHECK(output.busy_timeout_ms == 0);
    write_config(path, "database: {path: /tmp/workforce.db}");
    CHECK(config_load_file(path, &base, &output, error, sizeof(error)));
    CHECK(strcmp(output.database_path, "/tmp/workforce.db") == 0);
    write_config(path, "database: {path: 'C:/data/workforce.db'}");
    CHECK(config_load_file(path, &base, &output, error, sizeof(error)));
    CHECK(strcmp(output.database_path, "C:/data/workforce.db") == 0);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        write_config(path, invalid[i]);
        output = base;
        CHECK(!config_load_file(path, &base, &output, error, sizeof(error)));
        CHECK(strcmp(output.database_path, base.database_path) == 0);
        CHECK(output.busy_timeout_ms == base.busy_timeout_ms && error[0] != '\0');
    }
    test_delete(path);
    CHECK(!config_load_file(path, &base, &output, error, sizeof(error)));
    CHECK(!config_load_file(NULL, &base, &output, error, sizeof(error)));
    CHECK(!config_load_file(path, NULL, &output, error, sizeof(error)));
    CHECK(!config_load_file(path, &base, NULL, error, sizeof(error)));
    config_defaults(NULL);
    puts("Configuration library tests passed.");
    return EXIT_SUCCESS;
}
