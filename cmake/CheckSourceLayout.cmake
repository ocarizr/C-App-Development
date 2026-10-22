# A folder copy can resurrect sources that Git recorded as moved/deleted.
# Fail early instead of leaving two incompatible versions in the source tree.
if(NOT DEFINED WORKFORCE_SOURCE_ROOT)
    set(WORKFORCE_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
foreach(legacy_file IN ITEMS
    include/money.h include/workforce.h
    src/console.c src/money.c src/workforce.c
    tests/console_tests.c tests/domain_tests.c tests/test.h)
    if(EXISTS "${WORKFORCE_SOURCE_ROOT}/${legacy_file}")
        message(FATAL_ERROR
            "Obsolete source ${legacy_file} found. The current layout uses app/, libs/ and "
            "tests/support/. Remove leftover files from the previous layout before configuring.")
    endif()
endforeach()
