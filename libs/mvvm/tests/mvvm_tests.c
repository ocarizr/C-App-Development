#include "test.h"
#include "workforce_mvvm.h"

#include <string.h>

static bool command(void *context)
{
    ++*(int *)context;
    return true;
}

int main(void)
{
    int calls = 0;
    Mvvm *model = mvvm_create();
    CHECK(model != NULL);
    CHECK(mvvm_get(NULL, "name") == NULL);
    CHECK(!mvvm_writable(NULL, "name"));
    CHECK(!mvvm_publish(NULL, "name", "value"));
    CHECK(mvvm_get(model, NULL) == NULL);
    CHECK(!mvvm_writable(model, NULL));
    CHECK(mvvm_property(model, "name", "Ana", true));
    CHECK(!mvvm_property(model, "name", "Duplicate", true));
    CHECK(mvvm_property(model, "status", "ready", false));
    CHECK(!mvvm_set(model, "status", "forged"));
    CHECK(mvvm_publish(model, "status", "done"));
    CHECK(mvvm_set(model, "name", "Maria"));
    CHECK(strcmp(mvvm_get(model, "name"), "Maria") == 0);
    CHECK(mvvm_revision(model) == 4);
    /* A borrowed value may be published back into the same property. */
    CHECK(mvvm_publish(model, "name", mvvm_get(model, "name")));
    CHECK(strcmp(mvvm_get(model, "name"), "Maria") == 0);
    CHECK(mvvm_command(model, "save", command, &calls));
    CHECK(!mvvm_command(model, "save", command, &calls));
    CHECK(mvvm_execute(model, "save") && calls == 1);
    CHECK(!mvvm_execute(model, "missing"));
    CHECK(!mvvm_set(model, "missing", "bad"));
    CHECK(!mvvm_property(model, "bad", NULL, true));
    char *large = malloc(65537);
    CHECK(large != NULL);
    memset(large, 'x', 65536);
    large[65536] = '\0';
    size_t revision = mvvm_revision(model);
    CHECK(!mvvm_set(model, "name", large));
    CHECK(strcmp(mvvm_get(model, "name"), "Maria") == 0);
    CHECK(mvvm_revision(model) == revision);
    free(large);
    mvvm_destroy(model);
    return 0;
}
