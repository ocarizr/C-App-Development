#include "ui_internal.h"

#include <stdlib.h>
#include <string.h>

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static int measure_height(UiDocument *doc, int index)
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

        h = measure_height(doc, child);
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
    return min_int(height, 4096);
}

static void arrange_nodes(UiDocument *doc, int index, int x, int y, int width, int available)
{
    /* Geometry is measured in terminal cells. Child rectangles stay inside the
     * space offered by their parent; only the viewport applies screen scrolling. */
    UiNode *n = &doc->nodes[index];
    int children[UI_NODES] = {0}, count = 0, cursor = 0;
    if (width < 0)
    {
        width = 0;
    }

    n->x = x;
    n->y = y;
    n->w = min_int(n->width > 0 ? n->width : width, width);
    n->h = min_int(measure_height(doc, index), available);
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
            for (int j = i - column; j < min_int(i - column + columns, count); ++j)
            {
                int h = measure_height(doc, children[j]);
                row_height = row_height > h ? row_height : h;
            }

            int offset = column * (cell + n->gap);
            int child_width = min_int(cell, n->w - offset);
            arrange_nodes(doc, child, x + offset, y + cursor, child_width, available - cursor);
            if (column == columns - 1 || i == count - 1)
            {
                cursor += row_height + n->gap;
            }
        }
        else
        {
            arrange_nodes(doc, child, x, y + cursor, cell, available - cursor);
            cursor += measure_height(doc, child) + n->gap;
        }
    }
}

typedef struct
{
    uint32_t character;
    bool focused;
} Cell;

static uint32_t decode_scalar(const char **text)
{
    /* Invalid bytes and control scalars become printable placeholders so stored
     * text can never inject ANSI commands into the terminal output. */
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

static void draw_text(Cell *cells, int width, int height, int x, int y, int limit, const char *text,
                      bool focus, int skip)
{
    int column = 0;
    if (y < 0 || y >= height)
    {
        return;
    }

    while (*text != '\0' && *text != '\n' && column < skip + limit)
    {
        uint32_t character = decode_scalar(&text);
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
        (void)decode_scalar(&text);
        ++width;
    }

    return width;
}

static void write_scalar(FILE *out, uint32_t c)
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

    int viewport = height - min_int(bars, height - 1);
    if (doc->revision != mvvm_revision(doc->model))
    {
        for (int i = 0; i < doc->count; ++i)
        {
            doc->nodes[i].row_offset = 0;
        }

        doc->revision = mvvm_revision(doc->model);
    }

    arrange_nodes(doc, doc->root, 0, 0, width, 4096);
    if (doc->focus >= 0)
    {
        UiNode *focus = &doc->nodes[doc->focus];
        int focus_height = focus->type == C_TABS ? 1 : focus->h;
        int bottom = focus->y + min_int(focus_height, viewport - 1) - 1;
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
            draw_text(cells, width, height, 0, viewport + bar++, width, text, false, 0);
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
                draw_text(cells, width, viewport, x, y, n->w - (x - n->x), title, focused, 0);
                x += text_width(title) + 1;
            }
        }
        else if (n->type == C_INPUT || n->type == C_BUTTON)
        {
            char value[1400];
            if (n->type == C_INPUT)
            {
                draw_text(cells, width, viewport, n->x, y, n->w, n->text, false, 0);
                snprintf(value, sizeof(value), "[%s%s]", focused ? "> " : "", text);
                /* Keep the end of a long input visible while editing. */
                int skip = text_width(value) > n->w ? text_width(value) - n->w : 0;
                if (n->h >= 2)
                {
                    draw_text(cells, width, viewport, n->x, y + 1, n->w, value, focused, skip);
                }
            }
            else
            {
                snprintf(value, sizeof(value), "%s[ %s ]", focused ? "> " : "", n->text);
                draw_text(cells, width, viewport, n->x, y, n->w, value, focused, 0);
            }
        }
        else if (n->type == C_TABLE)
        {
            draw_text(cells, width, viewport, n->x, y, n->w, n->text, focused, 0);
            const char *row = text;
            for (int line = 0; line < n->row_offset && *row != '\0'; ++line)
            {
                const char *next = strchr(row, '\n');
                row = next == NULL ? row + strlen(row) : next + 1;
            }

            for (int line = 1; line < n->h && *row != '\0'; ++line)
            {
                draw_text(cells, width, viewport, n->x, y + line, n->w, row, false,
                          n->column_offset);
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
            draw_text(cells, width, viewport, n->x, y, n->w, text, false, 0);
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

            write_scalar(output, cell.character == 0 ? ' ' : cell.character);
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
