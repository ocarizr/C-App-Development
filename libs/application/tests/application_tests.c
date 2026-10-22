#include "test.h"
#include "workforce_application.h"

#include <string.h>

static void set(Mvvm *m, const char *name, const char *value)
{
    CHECK(mvvm_set(m, name, value));
}

int main(void)
{
    StoreOptions options = {":memory:", 0};
    WorkforceStore *store = NULL;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    bool created;
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_OK);
    WorkforceViewModel *vm = workforce_vm_create(store);
    CHECK(vm != NULL);
    Mvvm *m = workforce_vm_bindings(vm);
    CHECK(!mvvm_execute(m, "worker.create"));
    CHECK(strstr(mvvm_get(m, "status"), "invalidos") != NULL);
    set(m, "worker.name", "Ana");
    set(m, "worker.age", "30");
    set(m, "worker.profession", "Developer");
    set(m, "worker.department", "Engineering");
    set(m, "worker.level", "Senior");
    set(m, "worker.salary", "3000");
    CHECK(mvvm_execute(m, "worker.create"));
    CHECK(strcmp(mvvm_get(m, "worker.id"), "1") == 0);
    set(m, "worker.name", "Wrong form content");
    CHECK(mvvm_execute(m, "worker.load"));
    CHECK(strcmp(mvvm_get(m, "worker.name"), "Ana") == 0);
    set(m, "worker.name", "Ana Maria");
    set(m, "worker.salary", "9999");
    CHECK(mvvm_execute(m, "worker.save"));
    CHECK(strstr(mvvm_get(m, "workers"), "Ana Maria") != NULL);
    set(m, "contract.year", "2026");
    set(m, "contract.month", "10");
    set(m, "contract.hours", "10");
    set(m, "contract.rate", "25.50");
    CHECK(mvvm_execute(m, "contract.add"));
    CHECK(strcmp(mvvm_get(m, "contract.id"), "1") == 0);
    CHECK(mvvm_execute(m, "contract.load"));
    set(m, "contract.hours", "20");
    CHECK(mvvm_execute(m, "contract.save"));
    set(m, "salary.year", "2026");
    set(m, "salary.month", "11");
    set(m, "salary.amount", "4000");
    CHECK(mvvm_execute(m, "salary.save"));
    CHECK(strstr(mvvm_get(m, "history"), "2026-11 | 4000.00") != NULL);
    set(m, "report.year", "2026");
    set(m, "report.month", "10");
    set(m, "report.department", "Engineering");
    set(m, "report.level", "Senior");
    CHECK(mvvm_execute(m, "report.build"));
    CHECK(strstr(mvvm_get(m, "report"), "3510.00") != NULL);
    set(m, "report.month", "11");
    CHECK(mvvm_execute(m, "report.build"));
    CHECK(strstr(mvvm_get(m, "report"), "4000.00") != NULL);
    set(m, "report.level", "Invalid");
    CHECK(!mvvm_execute(m, "report.build"));
    set(m, "report.level", "Senior");
    set(m, "salary.amount", "4200");
    CHECK(mvvm_execute(m, "salary.save"));
    CHECK(mvvm_execute(m, "report.build"));
    CHECK(strstr(mvvm_get(m, "report"), "4200.00") != NULL);
    CHECK(mvvm_execute(m, "salary.delete"));
    CHECK(mvvm_execute(m, "report.build"));
    CHECK(strstr(mvvm_get(m, "report"), "3000.00") != NULL);
    set(m, "worker.id", "0");
    CHECK(!mvvm_execute(m, "worker.load"));
    set(m, "worker.id", "1");
    set(m, "contract.id", "999");
    CHECK(!mvvm_execute(m, "contract.save"));
    set(m, "contract.id", "1");
    set(m, "contract.rate", "92233720368547758.07");
    CHECK(!mvvm_execute(m, "contract.save"));
    CHECK(mvvm_execute(m, "contract.load"));
    CHECK(strcmp(mvvm_get(m, "contract.rate"), "25.50") == 0);
    CHECK(mvvm_execute(m, "contract.delete"));
    CHECK(mvvm_execute(m, "worker.delete"));
    CHECK(!mvvm_execute(m, "worker.load"));
    CHECK(strcmp(mvvm_get(m, "contracts"), "") == 0);
    CHECK(mvvm_execute(m, "worker.list"));
    workforce_vm_destroy(vm);
    store_close(store);
    CHECK(workforce_vm_create(NULL) == NULL);
    return 0;
}
