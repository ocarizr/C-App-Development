#ifndef WORKFORCE_TEST_H
#define WORKFORCE_TEST_H

#include <stdio.h>
#include <stdlib.h>

/* Unlike assert(), these checks also execute in Release builds. */
#define CHECK(expression)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            exit(EXIT_FAILURE);                                                                    \
        }                                                                                          \
    } while (0)

#endif
