#ifndef WORKFORCE_CONSOLE_H
#define WORKFORCE_CONSOLE_H

#include "workforce_store.h"
#include <stdio.h>

/* Borrows a connected store. EOF cancels the current operation and exits. */
int console_run(FILE *input, FILE *output, WorkforceStore *store);

#endif
