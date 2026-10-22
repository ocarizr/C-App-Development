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
    if (length + extra > (size_t)n->max_length || length + extra >= sizeof(text))
    {
        return false;
    }
    memcpy(text, old, length);
    memcpy(text + length, utf8, extra + 1);
    return mvvm_set(doc->model, n->bind, text);
}
static int minimum(int a, int b)
{
    return a < b ? a : b;
}
static int natural(UiDocument *doc, int index)
{
    UiNode *n = &doc->nodes[index];
    int height = 0, count = 0, row = 0;
    if (!ui_visible(doc, index))
    {
        return 0;
    }
    if (n->type == C_STATUS)
    {
        return 0;
    }
    if (n->height > 0)
    {
        return n->height;
    }
    if (n->type >= C_LABEL)
    {
        return n->type == C_INPUT ? 2 : n->type == C_TABLE ? 10 : 1;
    }
    for (int child = n->child; child != -1; child = doc->nodes[child].next)
    {
        int h;
        if (!ui_visible(doc, child))
        {
            continue;
        }
        h = natural(doc, child);
        if (n->type == C_HSTACK)
        {
            height = height > h ? height : h;
        }
        else if (n->type == C_GRID)
        {
            if (count % n->columns == 0 && count != 0)
            {
                height += row + n->gap;
                row = 0;
            }
            row = row > h ? row : h;
        }
        else
        {
            height += h + (count > 0 ? n->gap : 0);
        }
        ++count;
    }
    height += row + (n->type == C_TABS || n->type == C_WINDOW || n->type == C_APPLICATION ? 2 : 0);
    return minimum(height, 4096);
}
static void arrange(UiDocument *doc, int index, int x, int y, int width, int available)
{
    UiNode *n = &doc->nodes[index];
    int children[UI_NODES] = {0}, count = 0, cursor = 0;
    if (width < 0)
    {
        width = 0;
    }
    n->x = x;
    n->y = y;
    n->w = minimum(n->width > 0 ? n->width : width, width);
    n->h = minimum(natural(doc, index), available);
    if (n->h < 0)
    {
        n->h = 0;
    }
    available = n->h;
    if (n->type == C_TABS || n->type == C_WINDOW || n->type == C_APPLICATION)
    {
        y += 2;
        available -= 2;
    }
    for (int i = n->child; i != -1; i = doc->nodes[i].next)
    {
        if (ui_visible(doc, i))
        {
            children[count++] = i;
        }
    }
    for (int i = 0; i < count; ++i)
    {
        int child = children[i], cell = n->w;
        if (n->type == C_HSTACK || n->type == C_GRID)
        {
            int columns = n->type == C_GRID ? n->columns : count;
            cell = (n->w - n->gap * (columns - 1)) / columns;
            if (cell < 1)
            {
                cell = 1;
            }
            int column = i % columns;
            int row_height = 0;
            for (int j = i - column; j < minimum(i - column + columns, count); ++j)
            {
                int h = natural(doc, children[j]);
                row_height = row_height > h ? row_height : h;
            }
            int offset = column * (cell + n->gap);
            int child_width = minimum(cell, n->w - offset);
            arrange(doc, child, x + offset, y + cursor, child_width, available - cursor);
            if (column == columns - 1 || i == count - 1)
            {
                cursor += row_height + n->gap;
            }
        }
        else
        {
            arrange(doc, child, x, y + cursor, cell, available - cursor);
            cursor += natural(doc, child) + n->gap;
        }
    }
}
typedef struct
{
    uint32_t character;
    bool focused;
} Cell;
/* Width-one Unicode scalar values; control characters never reach the terminal. */
static uint32_t decode(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    uint32_t value = *p++;
    int extra = 0;
    if (value >= 0xc2 && value <= 0xdf)
    {
        value &= 0x1f;
        extra = 1;
    }
    else if (value >= 0xe0 && value <= 0xef)
    {
        value &= 0x0f;
        extra = 2;
    }
    else if (value >= 0xf0 && value <= 0xf4)
    {
        value &= 7;
        extra = 3;
    }
    else if (value >= 0x80)
    {
        value = '?';
    }
    for (int i = 0; i < extra; ++i)
    {
        if ((*p & 0xc0) != 0x80)
        {
            value = '?';
            break;
        }
        value = (value << 6) | (*p++ & 0x3f);
    }
    *text = (const char *)p;
    if (value < 32 || (value >= 127 && value < 160) || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff))
    {
        return '?';
    }
    return value;
}
static void put(Cell *cells, int width, int height, int x, int y, int limit, const char *text,
                bool focus, int skip)
{
    int column = 0;
    if (y < 0 || y >= height)
    {
        return;
    }
    while (*text != '\0' && *text != '\n' && column < skip + limit)
    {
        uint32_t character = decode(&text);
        if (column >= skip && x + column - skip >= 0 && x + column - skip < width)
        {
            Cell *cell = &cells[y * width + x + column - skip];
            cell->character = character;
            cell->focused = focus;
        }
        ++column;
    }
}
static int text_width(const char *text)
{
    int width = 0;
    while (*text != '\0')
    {
        (void)decode(&text);
        ++width;
    }
    return width;
}
static void emit(FILE *out, uint32_t c)
{
    if (c < 128)
    {
        fputc((int)c, out);
    }
    else if (c < 2048)
    {
        fputc((int)(0xc0 | (c >> 6)), out);
        fputc((int)(0x80 | (c & 63)), out);
    }
    else if (c < 65536)
    {
        fputc((int)(0xe0 | (c >> 12)), out);
        fputc((int)(0x80 | ((c >> 6) & 63)), out);
        fputc((int)(0x80 | (c & 63)), out);
    }
    else
    {
        fputc((int)(0xf0 | (c >> 18)), out);
        fputc((int)(0x80 | ((c >> 12) & 63)), out);
        fputc((int)(0x80 | ((c >> 6) & 63)), out);
        fputc((int)(0x80 | (c & 63)), out);
    }
}
bool ui_render(UiDocument *doc, FILE *output, int width, int height, bool plain)
{
    Cell *cells;
    int bars = 0, bar = 0;
    if (doc == NULL || output == NULL || width < 10 || width > 300 || height < 4 || height > 100)
    {
        return false;
    }
    cells = calloc((size_t)width * (size_t)height, sizeof(*cells));
    if (cells == NULL)
    {
        return false;
    }
    for (int i = 0; i < doc->count; ++i)
    {
        if (doc->nodes[i].type == C_STATUS && ui_visible(doc, i))
        {
            ++bars;
        }
    }
    int viewport = height - minimum(bars, height - 1);
    if (doc->revision != mvvm_revision(doc->model))
    {
        for (int i = 0; i < doc->count; ++i)
        {
            doc->nodes[i].row_offset = 0;
        }
        doc->revision = mvvm_revision(doc->model);
    }
    arrange(doc, doc->root, 0, 0, width, 4096);
    if (doc->focus >= 0)
    {
        UiNode *focus = &doc->nodes[doc->focus];
        int focus_height = focus->type == C_TABS ? 1 : focus->h;
        int bottom = focus->y + minimum(focus_height, viewport - 1) - 1;
        if (focus->y < doc->scroll)
        {
            doc->scroll = focus->y;
        }
        if (bottom >= doc->scroll + viewport)
        {
            doc->scroll = bottom - viewport + 1;
        }
    }
    for (int i = 0; i < doc->count; ++i)
    {
        UiNode *n = &doc->nodes[i];
        bool focused = i == doc->focus;
        int y = n->y - doc->scroll;
        const char *text = n->bind[0] == '\0' ? n->text : mvvm_get(doc->model, n->bind);
        if (!ui_visible(doc, i))
        {
            continue;
        }
        if (n->type == C_STATUS)
        {
            put(cells, width, height, 0, viewport + bar++, width, text, false, 0);
            continue;
        }
        if (n->h <= 0)
        {
            continue;
        }
        if (n->type == C_TABS)
        {
            int x = n->x;
            for (int t = n->child; t != -1; t = doc->nodes[t].next)
            {
                char title[300];
                snprintf(title, sizeof(title), "%c %s %c", t == doc->active_tab ? '[' : ' ',
                         doc->nodes[t].text, t == doc->active_tab ? ']' : ' ');
                put(cells, width, viewport, x, y, n->w - (x - n->x), title, focused, 0);
                x += text_width(title) + 1;
            }
        }
        else if (n->type == C_INPUT || n->type == C_BUTTON)
        {
            char value[1400];
            if (n->type == C_INPUT)
            {
                put(cells, width, viewport, n->x, y, n->w, n->text, false, 0);
                snprintf(value, sizeof(value), "[%s%s]", focused ? "> " : "", text);
                /* Keep the end of a long input visible while editing. */
                int skip = text_width(value) > n->w ? text_width(value) - n->w : 0;
                if (n->h >= 2)
                {
                    put(cells, width, viewport, n->x, y + 1, n->w, value, focused, skip);
                }
            }
            else
            {
                snprintf(value, sizeof(value), "%s[ %s ]", focused ? "> " : "", n->text);
                put(cells, width, viewport, n->x, y, n->w, value, focused, 0);
            }
        }
        else if (n->type == C_TABLE)
        {
            put(cells, width, viewport, n->x, y, n->w, n->text, focused, 0);
            const char *row = text;
            for (int line = 0; line < n->row_offset && *row != '\0'; ++line)
            {
                const char *next = strchr(row, '\n');
                row = next == NULL ? row + strlen(row) : next + 1;
            }
            for (int line = 1; line < n->h && *row != '\0'; ++line)
            {
                put(cells, width, viewport, n->x, y + line, n->w, row, false, n->column_offset);
                const char *next = strchr(row, '\n');
                row = next == NULL ? row + strlen(row) : next + 1;
            }
        }
        else if (n->type == C_SEPARATOR)
        {
            for (int x = n->x; y >= 0 && y < viewport && x < n->x + n->w && x < width; ++x)
            {
                cells[y * width + x].character = '-';
            }
        }
        else if (n->type == C_LABEL || n->type == C_WINDOW || n->type == C_APPLICATION)
        {
            put(cells, width, viewport, n->x, y, n->w, text, false, 0);
        }
    }
    if (!plain)
    {
        fputs("\033[H\033[0m", output);
    }
    bool style = false;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Cell cell = cells[y * width + x];
            if (!plain && cell.focused != style)
            {
                fputs(cell.focused ? "\033[30;106m" : "\033[0m", output);
                style = cell.focused;
            }
            emit(output, cell.character == 0 ? ' ' : cell.character);
        }
        if (y + 1 < height)
        {
            fputs(plain ? "\n" : "\r\n", output);
        }
    }
    if (!plain)
    {
        fputs("\033[0m", output);
    }
    free(cells);
    return fflush(output) == 0 && !ferror(output);
}
