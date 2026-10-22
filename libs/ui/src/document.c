#include "ui_internal.h"
#include <ctype.h>
#include <expat.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    UiDocument *doc;
    XML_Parser xml;
    int stack[UI_DEPTH], depth, root;
    char *error;
    size_t capacity;
    bool failed;
} Parser;
static void fail(Parser *p, const char *message)
{
    if (!p->failed)
    {
        snprintf(p->error, p->capacity, "linha %lu: %s",
                 (unsigned long)XML_GetCurrentLineNumber64(p->xml), message);
        p->failed = true;
        XML_StopParser(p->xml, XML_FALSE);
    }
}
static bool copy_attribute(Parser *p, char *out, size_t size, const char *value)
{
    if (strlen(value) >= size)
    {
        fail(p, "atributo excede o limite");
        return false;
    }
    memcpy(out, value, strlen(value) + 1);
    return true;
}
static int number(Parser *p, const char *value, int min, int max)
{
    char *end;
    long result;
    if (*value == '\0' || strlen(value) > 5)
    {
        fail(p, "numero invalido");
        return min;
    }
    result = strtol(value, &end, 10);
    if (*end != '\0' || result < min || result > max)
    {
        fail(p, "numero fora dos limites");
        return min;
    }
    return (int)result;
}
static bool layout(Component t)
{
    return t == C_VSTACK || t == C_HSTACK || t == C_GRID;
}
static void XMLCALL start(void *context, const char *name, const char **attributes)
{
    static const char *const names[] = {
        "Application", "Tabs",  "Tab",   "Window", "Fragment", "Include",   "VStack",   "HStack",
        "Grid",        "Label", "Input", "Button", "Table",    "Separator", "StatusBar"};
    Parser *p = context;
    UiNode *node;
    int type = -1, index;
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); ++i)
    {
        if (strcmp(name, names[i]) == 0)
        {
            type = i;
        }
    }
    if (type < 0 || p->depth == UI_DEPTH || p->doc->count == UI_NODES)
    {
        fail(p, "componente desconhecido ou limite de arvore excedido");
        return;
    }
    index = p->doc->count++;
    node = &p->doc->nodes[index];
    node->type = (Component)type;
    node->parent = p->depth == 0 ? -1 : p->stack[p->depth - 1];
    node->child = node->next = -1;
    node->gap = layout(node->type) ? 1 : 0;
    node->columns = 2;
    node->max_length = 99;
    if (p->depth == 0)
    {
        p->root = index;
    }
    else
    {
        int *link = &p->doc->nodes[node->parent].child;
        while (*link != -1)
        {
            link = &p->doc->nodes[*link].next;
        }
        *link = index;
    }
    p->stack[p->depth++] = index;
    for (int i = 0; attributes[i] != NULL && !p->failed; i += 2)
    {
        const char *key = attributes[i], *value = attributes[i + 1];
        Component t = node->type;
        if (strcmp(key, "id") == 0)
        {
            copy_attribute(p, node->id, sizeof(node->id), value);
        }
        else if (strcmp(key, "text") == 0 &&
                 (t == C_LABEL || t == C_BUTTON || t == C_INPUT || t == C_TABLE || t == C_TAB ||
                  t == C_APPLICATION || t == C_WINDOW || t == C_STATUS))
        {
            copy_attribute(p, node->text, sizeof(node->text), value);
        }
        else if (strcmp(key, "bind") == 0 &&
                 (t == C_LABEL || t == C_INPUT || t == C_TABLE || t == C_STATUS))
        {
            copy_attribute(p, node->bind, sizeof(node->bind), value);
        }
        else if (strcmp(key, "command") == 0 && t == C_BUTTON)
        {
            copy_attribute(p, node->command, sizeof(node->command), value);
        }
        else if (strcmp(key, "source") == 0 && (t == C_INCLUDE || t == C_TAB))
        {
            copy_attribute(p, node->source, sizeof(node->source), value);
        }
        else if (strcmp(key, "width") == 0)
        {
            node->width = number(p, value, 1, 300);
        }
        else if (strcmp(key, "height") == 0)
        {
            node->height = number(p, value, 1, 300);
        }
        else if (strcmp(key, "gap") == 0 && layout(t))
        {
            node->gap = number(p, value, 0, 8);
        }
        else if (strcmp(key, "columns") == 0 && t == C_GRID)
        {
            node->columns = number(p, value, 1, 12);
        }
        else if (strcmp(key, "maxLength") == 0 && t == C_INPUT)
        {
            node->max_length = number(p, value, 1, 1023);
        }
        else
        {
            fail(p, "atributo desconhecido ou incompativel com o componente");
        }
    }
}
static void XMLCALL end(void *context, const char *name)
{
    Parser *p = context;
    (void)name;
    if (p->depth > 0)
    {
        --p->depth;
    }
}
static void XMLCALL characters(void *context, const char *value, int length)
{
    Parser *p = context;
    for (int i = 0; i < length; ++i)
    {
        if (!isspace((unsigned char)value[i]))
        {
            fail(p, "use o atributo text em vez de texto dentro do elemento");
            break;
        }
    }
}
static void XMLCALL doctype(void *context, const char *name, const char *sysid, const char *pubid,
                            int subset)
{
    (void)name;
    (void)sysid;
    (void)pubid;
    (void)subset;
    fail(context, "DTD e entidades personalizadas nao sao permitidos");
}
static bool relative_source(const char *path)
{
    const char *part = path;
    if (*path == '\0' || *path == '/' || strchr(path, '\\') != NULL || strchr(path, ':') != NULL)
    {
        return false;
    }
    for (const char *cursor = path;; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\0')
        {
            size_t length = (size_t)(cursor - part);
            if (length == 0 || (length == 1 && *part == '.') ||
                (length == 2 && part[0] == '.' && part[1] == '.'))
            {
                return false;
            }
            part = cursor + 1;
            if (*cursor == '\0')
            {
                return true;
            }
        }
    }
}
static int load_file(UiDocument *doc, const char *path, int depth, char *error, size_t capacity)
{
    Parser p = {0};
    FILE *file;
    char data[4096];
    size_t total = 0, count;
    int begin = doc->count, finish;
    if (depth > 8 || (file = fopen(path, "rb")) == NULL)
    {
        snprintf(error, capacity, "nao foi possivel ler %s (limite de includes: 8)", path);
        return -1;
    }
    p.doc = doc;
    p.error = error;
    p.capacity = capacity;
    p.root = -1;
    p.xml = XML_ParserCreate("UTF-8");
    if (p.xml == NULL)
    {
        fclose(file);
        snprintf(error, capacity, "memoria insuficiente para XML");
        return -1;
    }
    XML_SetUserData(p.xml, &p);
    XML_SetElementHandler(p.xml, start, end);
    XML_SetCharacterDataHandler(p.xml, characters);
    XML_SetStartDoctypeDeclHandler(p.xml, doctype);
    do
    {
        count = fread(data, 1, sizeof(data), file);
        total += count;
        if (total > 65536 || ferror(file))
        {
            fail(&p, "arquivo excede 64 KiB ou falha de leitura");
            break;
        }
        if (XML_Parse(p.xml, data, (int)count, count < sizeof(data)) == XML_STATUS_ERROR)
        {
            if (!p.failed)
            {
                fail(&p, XML_ErrorString(XML_GetErrorCode(p.xml)));
            }
            break;
        }
    } while (count == sizeof(data));
    fclose(file);
    XML_ParserFree(p.xml);
    if (p.failed || p.root == -1)
    {
        char detail[512];
        snprintf(detail, sizeof(detail), "%s", error);
        snprintf(error, capacity, "%s: %s", path, detail);
        return -1;
    }
    finish = doc->count;
    for (int i = begin; i < finish; ++i)
    {
        UiNode *node = &doc->nodes[i];
        if (node->type == C_INCLUDE || node->type == C_TAB)
        {
            char child_path[1024];
            const char *slash = strrchr(path, '/'), *backslash = strrchr(path, '\\');
            size_t prefix;
            int child;
            if (backslash != NULL && (slash == NULL || backslash > slash))
            {
                slash = backslash;
            }
            prefix = slash == NULL ? 0 : (size_t)(slash - path + 1);
            if (node->child != -1 || !relative_source(node->source) ||
                prefix + strlen(node->source) >= sizeof(child_path))
            {
                snprintf(error, capacity,
                         "Tab/Include exige source relativo e nenhum filho inline");
                return -1;
            }
            memcpy(child_path, path, prefix);
            memcpy(child_path + prefix, node->source, strlen(node->source) + 1);
            child = load_file(doc, child_path, depth + 1, error, capacity);
            if (child == -1)
            {
                return -1;
            }
            if (doc->nodes[child].type != (node->type == C_TAB ? C_WINDOW : C_FRAGMENT))
            {
                snprintf(error, capacity,
                         "source deve ter raiz Window (Tab) ou Fragment (Include)");
                return -1;
            }
            node->child = child;
            doc->nodes[child].parent = i;
        }
    }
    return p.root;
}
static bool validate(UiDocument *doc, char *error, size_t capacity)
{
    for (int i = 0; i < doc->count; ++i)
    {
        UiNode *n = &doc->nodes[i];
        bool leaf = n->type >= C_LABEL;
        if ((leaf && n->child != -1) ||
            (n->type == C_TAB &&
             (n->parent < 0 || doc->nodes[n->parent].type != C_TABS || n->id[0] == '\0')) ||
            (n->type == C_TABS && n->child == -1) ||
            (n->parent >= 0 && doc->nodes[n->parent].type == C_TABS && n->type != C_TAB) ||
            (n->type == C_APPLICATION && i != doc->root) ||
            (n->type == C_WINDOW && (n->parent < 0 || doc->nodes[n->parent].type != C_TAB)) ||
            (n->type == C_FRAGMENT && (n->parent < 0 || doc->nodes[n->parent].type != C_INCLUDE)))
        {
            snprintf(error, capacity, "hierarquia invalida no componente %d", i);
            return false;
        }
        if ((n->bind[0] != '\0' && mvvm_get(doc->model, n->bind) == NULL) ||
            (n->type == C_INPUT && !mvvm_writable(doc->model, n->bind)) ||
            (n->type == C_TABLE && n->bind[0] == '\0') ||
            (n->type == C_BUTTON && strcmp(n->command, "quit") != 0 &&
             !mvvm_has_command(doc->model, n->command)))
        {
            snprintf(error, capacity, "binding/comando invalido no componente %d (%s)", i, n->id);
            return false;
        }
        for (int j = 0; n->id[0] != '\0' && j < i; ++j)
        {
            if (strcmp(n->id, doc->nodes[j].id) == 0)
            {
                snprintf(error, capacity, "id duplicado: %s", n->id);
                return false;
            }
        }
        if (n->type == C_TABS)
        {
            if (doc->tabs != -1)
            {
                snprintf(error, capacity, "exatamente um Tabs e obrigatorio");
                return false;
            }
            doc->tabs = i;
            doc->active_tab = n->child;
        }
    }
    if (doc->tabs == -1)
    {
        snprintf(error, capacity, "Tabs ausente");
        return false;
    }
    return true;
}
UiDocument *ui_load(const char *path, Mvvm *model, char *error, size_t capacity)
{
    UiDocument *doc;
    if (path == NULL || model == NULL || error == NULL || capacity == 0)
    {
        return NULL;
    }
    *error = '\0';
    doc = calloc(1, sizeof(*doc));
    if (doc == NULL)
    {
        snprintf(error, capacity, "memoria insuficiente");
        return NULL;
    }
    doc->model = model;
    doc->tabs = doc->focus = doc->active_tab = -1;
    doc->root = load_file(doc, path, 0, error, capacity);
    if (doc->root == -1 || doc->nodes[doc->root].type != C_APPLICATION ||
        !validate(doc, error, capacity))
    {
        if (*error == '\0')
        {
            snprintf(error, capacity, "raiz deve ser Application");
        }
        free(doc);
        return NULL;
    }
    ui_focus(doc, 1);
    return doc;
}
void ui_destroy(UiDocument *document)
{
    free(document);
}
