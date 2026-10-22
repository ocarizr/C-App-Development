#include "money.h"

#include <stddef.h>

bool money_parse(const char *text, int64_t *cents)
{
    int64_t whole = 0;
    int64_t fraction = 0;
    if (text == NULL || cents == NULL || *text < '0' || *text > '9')
    {
        return false;
    }

    while (*text >= '0' && *text <= '9')
    {
        int digit = *text++ - '0';
        if (whole > (INT64_MAX - digit) / 10)
        {
            return false;
        }

        whole = whole * 10 + digit;
    }

    if (*text == '.')
    {
        ++text;
        if (*text < '0' || *text > '9')
        {
            return false;
        }

        fraction = (*text++ - '0') * 10;
        if (*text >= '0' && *text <= '9')
        {
            fraction += *text++ - '0';
        }
    }

    if (*text != '\0' || whole > (INT64_MAX - fraction) / 100)
    {
        return false;
    }

    *cents = whole * 100 + fraction;
    return true;
}
