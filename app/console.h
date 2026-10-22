#ifndef WORKFORCE_CONSOLE_H
#define WORKFORCE_CONSOLE_H

#include <stdio.h>

/* Starts an in-memory session. EOF cancels the current operation and exits. */
int console_run(FILE *input, FILE *output);

#endif
