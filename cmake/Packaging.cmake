# Only runtime resources enter the distributable; tests, libraries and user data
# are never collected from a build directory or an existing installation.
install(TARGETS workforce RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/app.yaml" "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION . COMPONENT Runtime)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/app/ui/" DESTINATION ui
    COMPONENT Runtime FILES_MATCHING PATTERN "*.xml")
install(FILES "${PROJECT_SOURCE_DIR}/docs/design.md"
    "${PROJECT_SOURCE_DIR}/docs/persistence.md" "${PROJECT_SOURCE_DIR}/docs/ui-authoring.md"
    DESTINATION docs COMPONENT Runtime)

set(WORKFORCE_PACKAGE_VERSION "${PROJECT_VERSION}" CACHE STRING "Version written on release archives")
if(NOT WORKFORCE_PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+(\\.[0-9]+)?$")
    message(FATAL_ERROR "WORKFORCE_PACKAGE_VERSION must be numeric: 1.1 or 1.1.0")
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" package_system)
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$")
    set(package_arch x64)
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" package_arch)
endif()
set(CPACK_PACKAGE_NAME workforce)
set(CPACK_PACKAGE_VERSION "${WORKFORCE_PACKAGE_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "workforce-${WORKFORCE_PACKAGE_VERSION}-${package_system}-${package_arch}")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_VENDOR "Workforce")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Workforce terminal application")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_MONOLITHIC_INSTALL ON)
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
