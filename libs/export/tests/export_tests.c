#include "test_files.h"
#include "workforce_export.h"

#include <string.h>

int main(void)
{
    const char *path = "workforce-export-test.csv";
    Team team = {0};
    PayrollReport report = {0};
    ReportFilter filter = {0};
    WorkerProfile data = {
        "Ana, \"Silva\"\nJunior", "Developer", "Engineering", 30, WORKER_JUNIOR, 300000};
    uint32_t id;
    FILE *file;
    char text[4096];
    size_t length;
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OK);
    memcpy(data.name, "=SUM(1,2)", sizeof("=SUM(1,2)"));
    CHECK(team_add(&team, &data, &id) == WORKFORCE_OK);
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK);
    test_delete(path);
    CHECK(report_export_csv(path, &report) == WORKFORCE_OK);
    CHECK(report_export_csv(path, &report) == WORKFORCE_IO_ERROR); /* No overwrite. */
    file = test_file(path, "rb");
    length = fread(text, 1, sizeof(text) - 1, file);
    text[length] = '\0';
    CHECK(!ferror(file) && fclose(file) == 0);
    CHECK(strstr(text, "\"Ana, \"\"Silva\"\"\nJunior\"") != NULL);
    CHECK(strstr(text, "\"'=SUM(1,2)\"") != NULL);
    CHECK(strstr(text, "DEPARTMENT,2026,10,,,,\"Engineering\",,2,6000.00,0.00,6000.00\r\n") !=
          NULL);
    CHECK(strstr(text, "TOTAL,2026,10,,,,,,2,6000.00,0.00,6000.00\r\n") != NULL);
    file = test_file(path, "rb");
    CHECK(report_write_csv(file, &report) == WORKFORCE_IO_ERROR);
    CHECK(fclose(file) == 0);
    test_delete(path);
    CHECK(report_export_csv("nonexistent-report-directory/out.csv", &report) == WORKFORCE_IO_ERROR);
    CHECK(report_write_csv(NULL, &report) == WORKFORCE_INVALID);
    CHECK(report_write_text(NULL, &report) == WORKFORCE_INVALID);
    CHECK(report_export_csv(path, NULL) == WORKFORCE_INVALID);
    file = test_file(path, "wb");
    CHECK(report_write_text(file, &report) == WORKFORCE_OK);
    CHECK(fclose(file) == 0);
    test_delete(path);
    report_dispose(&report);
    team_dispose(&team);
    CHECK(report_build(&team, 2026, 10, &filter, &report) == WORKFORCE_OK);
    CHECK(report_export_csv(path, &report) == WORKFORCE_OK);
    report_dispose(&report);
    test_delete(path);
    puts("Report presentation and CSV tests passed.");
    return EXIT_SUCCESS;
}
