#include "test_files.h"
#include "workforce_export.h"

#include <string.h>

static void snapshot_adapters(void)
{
    const char *path = "workforce-snapshot-test.txt";
    WorkerProfile profile = {"Ana", "Developer", "Engineering", 30, WORKER_SENIOR, 300000};
    Team team = {0};
    uint32_t id;
    char text[2048];
    CHECK(team_add(&team, &profile, &id) == WORKFORCE_OK);
    Worker *worker = team_find(&team, id);
    CHECK(worker != NULL);
    CHECK(worker_add_contract(worker, (HourContract){2026, 10, 10, 2550, 0}) == WORKFORCE_OK);
    CHECK(worker_set_salary(worker, (SalaryChange){2026, 11, 400000}) == WORKFORCE_OK);
    FILE *file = test_file(path, "w+b");
    CHECK(team_write_text(file, &team) == WORKFORCE_OK);
    CHECK(worker_write_contracts(file, worker) == WORKFORCE_OK);
    CHECK(worker_write_history(file, worker) == WORKFORCE_OK);
    /* Borrowing the stream leaves it usable by the caller after every adapter. */
    CHECK(fputs("still open\n", file) >= 0);
    CHECK(fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0);
    size_t length = fread(text, 1, sizeof(text) - 1, file);
    text[length] = '\0';
    CHECK(!ferror(file) && fclose(file) == 0);
    CHECK(strstr(text, "1 | Ana | 30 | Developer | Engineering | Senior\n") != NULL);
    CHECK(strstr(text, "1 | 2026-10 | 10 | 25.50\n") != NULL);
    CHECK(strstr(text, "Salario inicial: 3000.00\n") != NULL);
    CHECK(strstr(text, "2026-11 | 4000.00\n") != NULL);
    CHECK(strstr(text, "still open\n") != NULL);
    file = test_file(path, "rb");
    CHECK(team_write_text(file, &team) == WORKFORCE_IO_ERROR);
    CHECK(worker_write_contracts(file, worker) == WORKFORCE_IO_ERROR);
    CHECK(worker_write_history(file, worker) == WORKFORCE_IO_ERROR);
    CHECK(team_write_text(file, NULL) == WORKFORCE_INVALID);
    CHECK(worker_write_contracts(file, NULL) == WORKFORCE_INVALID);
    CHECK(worker_write_history(file, NULL) == WORKFORCE_INVALID);
    CHECK(fclose(file) == 0);
    CHECK(team_write_text(NULL, &team) == WORKFORCE_INVALID);
    CHECK(worker_write_contracts(NULL, worker) == WORKFORCE_INVALID);
    CHECK(worker_write_history(NULL, worker) == WORKFORCE_INVALID);
    team_dispose(&team);
    test_delete(path);
}

int main(void)
{
    snapshot_adapters();
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
