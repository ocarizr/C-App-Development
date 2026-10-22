#include "ui_internal.h"

#include <stdlib.h>
#include <string.h>

bool ui_visible(const UiDocument *doc, int index)
{
    for (int i = index; i != -1; i = doc->nodes[i].parent)
    {
        if (doc->nodes[i].type == C_TAB && i != doc->active_tab)
        {
            return false;
        }
    }

    return true;
}

static void collect(const UiDocument *doc, int index, int *items, int *count)
{
    /* Only the selected tab participates in focus order; shared bindings still
     * belong to the same model in hidden tabs. Loading bounds the tree to UI_NODES. */
    const UiNode *n = &doc->nodes[index];
    if (!ui_visible(doc, index))
    {
        return;
    }

    if (n->type == C_INPUT || n->type == C_BUTTON || n->type == C_TABLE || n->type == C_TABS)
    {
        items[(*count)++] = index;
    }

    for (int child = n->child; child != -1; child = doc->nodes[child].next)
    {
        collect(doc, child, items, count);
    }
}

void ui_focus(UiDocument *doc, int direction)
{
    int items[UI_NODES], count = 0, position = -1;
    collect(doc, doc->root, items, &count);
    for (int i = 0; i < count; ++i)
    {
        if (items[i] == doc->focus)
        {
            position = i;
        }
    }

    if (count > 0)
    {
        doc->focus = items[(position + direction + count) % count];
    }
}

bool ui_select_tab(UiDocument *doc, const char *id)
{
    if (doc == NULL || id == NULL)
    {
        return false;
    }

    for (int i = doc->nodes[doc->tabs].child; i != -1; i = doc->nodes[i].next)
    {
        if (strcmp(doc->nodes[i].id, id) == 0)
        {
            doc->active_tab = i;
            doc->focus = doc->tabs;
            doc->scroll = 0;
            return true;
        }
    }

    return false;
}

static void change_tab(UiDocument *doc, bool forward)
{
    int first = doc->nodes[doc->tabs].child, last = first, previous = -1;
    for (int i = first; i != -1; i = doc->nodes[i].next)
    {
        if (doc->nodes[i].next == doc->active_tab)
        {
            previous = i;
        }

        last = i;
    }

    int next = doc->nodes[doc->active_tab].next;
    doc->active_tab = forward ? (next == -1 ? first : next) : (previous == -1 ? last : previous);
    doc->focus = doc->tabs;
    doc->scroll = 0;
}

bool ui_key(UiDocument *doc, UiKey key)
{
    UiNode *n;
    if (doc == NULL)
    {
        return false;
    }

    n = doc->focus < 0 ? NULL : &doc->nodes[doc->focus];
    if (key == UI_NEXT || key == UI_PREVIOUS)
    {
        ui_focus(doc, key == UI_NEXT ? 1 : -1);
    }
    else if (key == UI_TAB_NEXT || key == UI_TAB_PREVIOUS ||
             (n != NULL && n->type == C_TABS && (key == UI_LEFT || key == UI_RIGHT)))
    {
        change_tab(doc, key == UI_TAB_NEXT || key == UI_RIGHT);
    }
    else if (n != NULL && key == UI_ACTIVATE)
    {
        if (n->type == C_BUTTON)
        {
            if (strcmp(n->command, "quit") == 0)
            {
                doc->quit = true;
            }
            else
            {
                /* Validation errors are displayed through the VM status binding. */
                (void)mvvm_execute(doc->model, n->command);
            }
        }
        else if (n->type == C_TABS)
        {
            change_tab(doc, true);
        }
        else
        {
            ui_focus(doc, 1);
        }
    }
    else if (n != NULL && n->type == C_INPUT && (key == UI_BACKSPACE || key == UI_CLEAR))
    {
        const char *old = mvvm_get(doc->model, n->bind);
        char text[1024];
        size_t length = strlen(old);
        if (length >= sizeof(text))
        {
            return false;
        }

        memcpy(text, old, length + 1);
        if (key == UI_CLEAR)
        {
            length = 0;
        }
        else if (length > 0)
        {
            --length;
            while (length > 0 && ((unsigned char)text[length] & 0xc0) == 0x80)
            {
                --length;
            }
        }

        text[length] = '\0';
        return mvvm_set(doc->model, n->bind, text);
    }
    else if (n != NULL && n->type == C_TABLE)
    {
        const char *text = mvvm_get(doc->model, n->bind);
        int lines = 1;
        for (const char *p = text; *p != '\0'; ++p)
        {
            lines += *p == '\n';
        }

        if (key == UI_PAGE_DOWN && n->row_offset < lines - 1)
        {
            n->row_offset += n->h > 2 ? n->h - 2 : 1;
            if (n->row_offset >= lines)
            {
                n->row_offset = lines - 1;
            }
        }
        else if (key == UI_PAGE_UP)
        {
            n->row_offset -= n->h > 2 ? n->h - 2 : 1;
            if (n->row_offset < 0)
            {
                n->row_offset = 0;
            }
        }
        else if (key == UI_RIGHT && n->column_offset < 1024)
        {
            n->column_offset += 8;
        }
        else if (key == UI_LEFT && n->column_offset > 0)
        {
            n->column_offset -= 8;
        }
    }

    return true;
}

bool ui_text(UiDocument *doc, const char *utf8)
{
    char text[1024];
    UiNode *n;
    const char *old;
    size_t length, extra;
    if (doc == NULL || utf8 == NULL || doc->focus < 0)
    {
        return false;
    }

    n = &doc->nodes[doc->focus];
    if (n->type != C_INPUT)
    {
        return false;
    }

    for (const unsigned char *p = (const unsigned char *)utf8; *p != 0; ++p)
    {
        if (*p < 32 || *p == 127)
        {
            return false;
        }
    }

    old = mvvm_get(doc->model, n->bind);
    length = strlen(old);
    extra = strlen(utf8);
    /* Compare against remaining capacity before addition, including the terminator. */
    if (length >= sizeof(text) || extra >= sizeof(text) - length ||
        length > (size_t)n->max_length || extra > (size_t)n->max_length - length)
    {
        return false;
    }

    memcpy(text, old, length);
    memcpy(text + length, utf8, extra + 1);
    return mvvm_set(doc->model, n->bind, text);
}

int ui_script(UiDocument *doc, FILE *input, FILE *output)
{
    char line[2048];
    if (doc == NULL || input == NULL || output == NULL)
    {
        return EXIT_FAILURE;
    }

    while (!doc->quit && fgets(line, sizeof(line), input) != NULL)
    {
        size_t length = strlen(line);
        if (length == sizeof(line) - 1 && line[length - 1] != '\n')
        {
            fputs("Linha de script longa demais.\n", output);
            return EXIT_FAILURE;
        }

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        if (strncmp(line, "set ", 4) == 0)
        {
            char *separator = strchr(line + 4, '=');
            if (separator == NULL)
            {
                return EXIT_FAILURE;
            }

            *separator = '\0';
            if (!mvvm_set(doc->model, line + 4, separator + 1))
            {
                return EXIT_FAILURE;
            }
        }
        else if (strncmp(line, "invoke ", 7) == 0 || strncmp(line, "reject ", 7) == 0)
        {
            bool expected = line[0] == 'i';
            if (!mvvm_has_command(doc->model, line + 7) ||
                mvvm_execute(doc->model, line + 7) != expected)
            {
                const char *status = mvvm_get(doc->model, "status");
                fprintf(output, "Comando falhou: %s (%s)\n", line + 7,
                        status == NULL ? "" : status);
                return EXIT_FAILURE;
            }
        }
        else if (strncmp(line, "get ", 4) == 0)
        {
            const char *value = mvvm_get(doc->model, line + 4);
            if (value == NULL)
            {
                return EXIT_FAILURE;
            }

            fprintf(output, "%s\n", value);
        }
        else if (strncmp(line, "tab ", 4) == 0)
        {
            if (!ui_select_tab(doc, line + 4))
            {
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(line, "render") == 0)
        {
            if (!ui_render(doc, output, 120, 40, true))
            {
                return EXIT_FAILURE;
            }

            fputc('\n', output);
        }
        else if (strcmp(line, "quit") == 0)
        {
            break;
        }
        else
        {
            fprintf(output, "Instrucao desconhecida: %s\n", line);
            return EXIT_FAILURE;
        }
    }

    return ferror(input) || fflush(output) != 0 || ferror(output) ? EXIT_FAILURE : EXIT_SUCCESS;
}
