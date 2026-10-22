#ifndef WORKFORCE_MONEY_H
#define WORKFORCE_MONEY_H

#include <stdbool.h>
#include <stdint.h>

/* Nonnegative decimal amount, optional dot and one or two decimal digits.
 * No sign, whitespace, exponent or thousands separator. Output unchanged on error. */
bool money_parse(const char *text, int64_t *cents);

#endif
