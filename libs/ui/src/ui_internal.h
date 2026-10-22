#ifndef WORKFORCE_UI_INTERNAL_H
#define WORKFORCE_UI_INTERNAL_H
#include "workforce_ui.h"

#include <stdint.h>

#define UI_NODES 512
#define UI_DEPTH 32

typedef enum
{
    C_APPLICATION,
    C_TABS,
    C_TAB,
    C_WINDOW,
    C_FRAGMENT,
    C_INCLUDE,
    C_VSTACK,
    C_HSTACK,
    C_GRID,
    C_LABEL,
    C_INPUT,
    C_BUTTON,
    C_TABLE,
    C_SEPARATOR,
    C_STATUS
} Component;

typedef struct
{
    Component type;
    int parent, child, next;
    char id[64], text[256], bind[64], command[64], source[256];
    int width, height, columns, gap, max_length;
    int x, y, w, h, row_offset, column_offset;
} UiNode;

/* Nodes form an index-based tree: -1 means no link. The bounded array keeps
 * pointers stable while referenced XML files append children during loading. */
struct UiDocument
{
    Mvvm *model;
    UiNode nodes[UI_NODES];
    int count, root, tabs, active_tab, focus, scroll;
    size_t revision;
    bool quit;
};

void ui_focus(UiDocument *doc, int direction);
bool ui_visible(const UiDocument *doc, int node);
#endif
