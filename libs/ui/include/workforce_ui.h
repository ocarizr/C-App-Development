#ifndef WORKFORCE_UI_H
#define WORKFORCE_UI_H
#include "workforce_mvvm.h"
#include <stdbool.h>
#include <stdio.h>

/* A document owns the expanded XML component tree and borrows its model.
 * Load validates the complete tree before exposing it. UI never owns business data. */
typedef struct UiDocument UiDocument;
typedef enum
{
    UI_NEXT,
    UI_PREVIOUS,
    UI_ACTIVATE,
    UI_BACKSPACE,
    UI_CLEAR,
    UI_TAB_NEXT,
    UI_TAB_PREVIOUS,
    UI_PAGE_DOWN,
    UI_PAGE_UP,
    UI_LEFT,
    UI_RIGHT
} UiKey;
UiDocument *ui_load(const char *path, Mvvm *model, char *error, size_t capacity);
void ui_destroy(UiDocument *document);
bool ui_key(UiDocument *document, UiKey key);
bool ui_text(UiDocument *document, const char *utf8);
bool ui_select_tab(UiDocument *document, const char *id);
/* Render a bounded viewport. plain=true omits ANSI escapes for snapshots. */
bool ui_render(UiDocument *document, FILE *output, int width, int height, bool plain);
/* Requires a TTY. Restores terminal mode and alternate screen on normal exit/errors. */
int ui_run(UiDocument *document);
/* Deterministic adapter: set NAME=VALUE, invoke COMMAND, tab ID, render, quit.
 * Failures stop the script with nonzero exit status. EOF is a clean exit. */
int ui_script(UiDocument *document, FILE *input, FILE *output);
#endif
