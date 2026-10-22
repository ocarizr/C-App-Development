include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Pinned source archives; no precompiled SQLite/YAML DLLs are used.
FetchContent_Declare(sqlite_source
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
    SOURCE_SUBDIR workforce-no-upstream-build
    URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9)
FetchContent_Declare(yaml_source
    URL https://github.com/yaml/libyaml/archive/refs/tags/0.2.5.tar.gz
    SOURCE_SUBDIR workforce-no-upstream-build
    URL_HASH SHA256=fa240dbf262be053f3898006d502d514936c818e422afdcf33921c63bed9bf2e)
FetchContent_MakeAvailable(sqlite_source yaml_source)

add_library(workforce_sqlite STATIC ${sqlite_source_SOURCE_DIR}/sqlite3.c)
target_include_directories(workforce_sqlite SYSTEM PUBLIC ${sqlite_source_SOURCE_DIR})
target_compile_definitions(workforce_sqlite PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0 SQLITE_DEFAULT_FOREIGN_KEYS=1)
find_package(Threads REQUIRED)
target_link_libraries(workforce_sqlite PRIVATE Threads::Threads)
if(UNIX)
    target_link_libraries(workforce_sqlite PRIVATE m)
endif()

# Build the dependency ourselves to avoid changing its global BUILD_TESTING options.
add_library(workforce_yaml STATIC
    ${yaml_source_SOURCE_DIR}/src/api.c
    ${yaml_source_SOURCE_DIR}/src/dumper.c
    ${yaml_source_SOURCE_DIR}/src/emitter.c
    ${yaml_source_SOURCE_DIR}/src/loader.c
    ${yaml_source_SOURCE_DIR}/src/parser.c
    ${yaml_source_SOURCE_DIR}/src/reader.c
    ${yaml_source_SOURCE_DIR}/src/scanner.c
    ${yaml_source_SOURCE_DIR}/src/writer.c)
target_include_directories(workforce_yaml SYSTEM PUBLIC ${yaml_source_SOURCE_DIR}/include)
target_compile_definitions(workforce_yaml PUBLIC YAML_DECLARE_STATIC PRIVATE
    YAML_VERSION_MAJOR=0 YAML_VERSION_MINOR=2 YAML_VERSION_PATCH=5 YAML_VERSION_STRING="0.2.5")
if(MSVC)
    target_compile_definitions(workforce_yaml PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(UNIX)
    # libyaml 0.2.5 calls the POSIX strdup API. Strict C11 hides its declaration
    # on glibc unless POSIX.1-2008 is requested before including system headers.
    target_compile_definitions(workforce_yaml PRIVATE _POSIX_C_SOURCE=200809L)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Older GCC otherwise accepts an implicit int return, causing pointer
        # truncation and crashes when configuration parsing runs on 64-bit Linux.
        target_compile_options(workforce_yaml PRIVATE -Werror=implicit-function-declaration)
    endif()
endif()
# Analyze our adapters, not downloaded upstream implementation files.
set_target_properties(workforce_sqlite workforce_yaml PROPERTIES C_CLANG_TIDY "")
