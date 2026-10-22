#ifndef WORKFORCE_TEST_FILES_H
#define WORKFORCE_TEST_FILES_H

#include "test.h"
#include <errno.h>

static FILE *test_file(const char *path, const char *mode)
{
    FILE *file = NULL;
#ifdef _MSC_VER
    CHECK(fopen_s(&file, path, mode) == 0);
#else
    file = fopen(path, mode);
#endif
    CHECK(file != NULL);
    return file;
}

static void test_delete(const char *path)
{
    errno = 0;
    CHECK(remove(path) == 0 || errno == ENOENT);
}

#endif
