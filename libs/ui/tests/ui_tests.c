#include "test.h"
#include "workforce_ui.h"
#include <string.h>

static bool count(void *context)
{
    ++*(int *)context;
    return true;
}
static void snapshot(UiDocument *doc, char *text, size_t size, int width, int height)
{
    FILE *file = tmpfile();
    CHECK(file != NULL);
    CHECK(ui_render(doc, file, width, height, true));
    rewind(file);
    size_t length = fread(text, 1, size - 1, file);
    text[length] = '\0';
    CHECK(!ferror(file));
    CHECK(fclose(file) == 0);
}
static void invalid(Mvvm *model, const char *xml)
{
    char error[512];
    FILE *file = fopen("ui-invalid.xml", "wb");
    CHECK(file != NULL);
    CHECK(fputs(xml, file) >= 0);
    CHECK(fclose(file) == 0);
    CHECK(ui_load("ui-invalid.xml", model, error, sizeof(error)) == NULL);
    CHECK(error[0] != '\0');
    CHECK(remove("ui-invalid.xml") == 0);
}
static void invalid_window(Mvvm *model, const char *xml)
{
    FILE *file = fopen("ui-window-invalid.xml", "wb");
    CHECK(file != NULL);
    CHECK(fputs(xml, file) >= 0);
    CHECK(fclose(file) == 0);
    invalid(
        model,
        "<Application><Tabs><Tab id='one' source='ui-window-invalid.xml'/></Tabs></Application>");
    CHECK(remove("ui-window-invalid.xml") == 0);
}
int main(int argc, char **argv)
{
    char path[1024], error[512], image[16000], before[16000];
    Mvvm *model = mvvm_create();
    int calls = 0;
    CHECK(argc == 2 && model != NULL);
    CHECK(mvvm_property(model, "name", "Ana", true));
    CHECK(mvvm_property(model, "status", "STATUS ALWAYS VISIBLE", false));
    CHECK(mvvm_property(model, "rows", "row0\nrow1\nrow2\nrow3\nrow4\nrow5\nrow6\n", false));
    CHECK(mvvm_command(model, "count", count, &calls));
    snprintf(path, sizeof(path), "%s/application.xml", argv[1]);
    UiDocument *doc = ui_load(path, model, error, sizeof(error));
    CHECK(doc != NULL);
    snapshot(doc, image, sizeof(image), 80, 24);
    const char *left = strstr(image, "LEFT"), *right = strstr(image, "RIGHT & XML");
    CHECK(left != NULL && right != NULL && right > left && right - left < 80);
    CHECK(strchr(left, '\n') > right);
    CHECK(strstr(image, "STATUS ALWAYS VISIBLE") != NULL);
    CHECK(strchr(image, '\033') == NULL);
    CHECK(ui_key(doc, UI_NEXT));
    CHECK(ui_text(doc, " Maria"));
    CHECK(strcmp(mvvm_get(model, "name"), "Ana Maria") == 0);
    CHECK(!ui_text(doc, "too long"));
    CHECK(!ui_text(doc, "\033"));
    CHECK(ui_key(doc, UI_CLEAR));
    CHECK(ui_text(doc, "Jos\xc3\xa9"));
    CHECK(ui_key(doc, UI_BACKSPACE));
    CHECK(strcmp(mvvm_get(model, "name"), "Jos") == 0);
    CHECK(ui_key(doc, UI_NEXT));
    CHECK(ui_key(doc, UI_ACTIVATE) && calls == 1);
    CHECK(ui_key(doc, UI_NEXT));
    CHECK(ui_key(doc, UI_NEXT));
    snapshot(doc, before, sizeof(before), 80, 24);
    CHECK(ui_key(doc, UI_PAGE_DOWN));
    snapshot(doc, image, sizeof(image), 80, 24);
    CHECK(strcmp(before, image) != 0);
    CHECK(strstr(image, "row0") == NULL);
    CHECK(ui_key(doc, UI_PAGE_UP));
    CHECK(ui_select_tab(doc, "second"));
    CHECK(!ui_select_tab(doc, "missing"));
    CHECK(ui_key(doc, UI_TAB_NEXT));
    snapshot(doc, image, sizeof(image), 80, 24);
    CHECK(strstr(image, "[ First ]") != NULL);
    CHECK(ui_key(doc, UI_TAB_PREVIOUS));
    snapshot(doc, image, sizeof(image), 40, 8);
    CHECK(strstr(image, "STATUS ALWAYS VISIBLE") != NULL);
    CHECK(mvvm_publish(model, "status", "\033[2J unsafe"));
    snapshot(doc, image, sizeof(image), 40, 8);
    CHECK(strchr(image, '\033') == NULL);
    CHECK(strstr(image, "?[2J unsafe") != NULL);
    CHECK(!ui_render(doc, stdout, 0, 0, true));
    FILE *script = tmpfile(), *out = tmpfile();
    CHECK(script != NULL && out != NULL);
    fputs("set name=Bea\ninvoke count\ntab first\nrender\nget name\nquit\n", script);
    rewind(script);
    CHECK(ui_script(doc, script, out) == 0 && calls == 2);
    fclose(script);
    fclose(out);
    script = tmpfile();
    out = tmpfile();
    CHECK(script != NULL && out != NULL);
    fputs("set status=forged\n", script);
    rewind(script);
    CHECK(ui_script(doc, script, out) != 0);
    fclose(script);
    fclose(out);
    ui_destroy(doc);
    invalid(model, "<Application><Unknown/></Application>");
    invalid(model, "<Application/><Application/>");
    invalid(model, "<Application><Tabs></Application>");
    invalid(model, "<!DOCTYPE Application [<!ENTITY x 'boom'>]><Application/>");
    invalid(model, "<Application wrong='x'/>");
    invalid(model, "<Application><Input bind='status'/></Application>");
    invalid(model, "<Application><Label bind='missing'/></Application>");
    invalid(model, "<Application><Button command='missing'/></Application>");
    invalid(model, "<Application><Grid columns='0'/></Application>");
    invalid(model, "<Application><Label>inline</Label></Application>");
    invalid(model, "<Application><Label id='x'/><Label id='x'/></Application>");
    invalid(model, "<Application><Include source='../secret.xml'/></Application>");
    invalid(model, "<Application><Include source='/secret.xml'/></Application>");
    invalid(model, "<Application><Include source='missing.xml'/></Application>");
    invalid(model, "<Application><Include source='ui-invalid.xml'/></Application>");
    invalid(model, "<Application><Label><Label/></Label></Application>");
    invalid(model, "<Application><Tabs><Label/></Tabs></Application>");
    invalid(model, "<Application><Tabs/></Application>");
    invalid_window(model, "<Window><Label id='same'/><Label id='same'/></Window>");
    invalid_window(model, "<Window><Label><Button command='count'/></Label></Window>");
    invalid_window(model, "<Window><Input bind='status'/></Window>");
    invalid_window(model, "<Window><Table bind='missing'/></Window>");
    invalid_window(model, "<Window><Button command='missing'/></Window>");
    invalid_window(model, "<Fragment/>");
    char *large = malloc(70000);
    CHECK(large != NULL);
    size_t used = sizeof("<Application>") - 1;
    memcpy(large, "<Application>", used);
    for (int i = 0; i < 513; ++i)
    {
        memcpy(large + used, "<Label/>", sizeof("<Label/>") - 1);
        used += sizeof("<Label/>") - 1;
    }
    memcpy(large + used, "</Application>", sizeof("</Application>"));
    invalid(model, large);
    used = sizeof("<Application>") - 1;
    memcpy(large, "<Application>", used);
    for (int i = 0; i < 33; ++i)
    {
        memcpy(large + used, "<VStack>", sizeof("<VStack>") - 1);
        used += sizeof("<VStack>") - 1;
    }
    large[used] = '\0';
    invalid(model, large);
    memset(large, ' ', 69999);
    large[69999] = '\0';
    invalid(model, large);
    free(large);
    mvvm_destroy(model);
    return 0;
}
