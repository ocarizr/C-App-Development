add_library(workforce_console STATIC console.c)
target_include_directories(workforce_console PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(workforce_console PUBLIC workforce_infra PRIVATE workforce_export project_options)

add_executable(workforce main.c)
target_link_libraries(workforce PRIVATE workforce_console workforce_config project_options)
if(BUILD_TESTING)
    add_executable(console_tests tests/console_tests.c)
    target_link_libraries(console_tests PRIVATE workforce_console workforce_test_support)
    add_test(NAME console COMMAND console_tests)
    set_tests_properties(console PROPERTIES LABELS app TIMEOUT 30)
    add_test(NAME app_persistence COMMAND ${CMAKE_COMMAND}
        -DAPP=$<TARGET_FILE:workforce>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/persistence-test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/persistence.cmake)
    set_tests_properties(app_persistence PROPERTIES LABELS "app;integration" TIMEOUT 30)
    add_test(NAME app_reports COMMAND ${CMAKE_COMMAND}
        -DAPP=$<TARGET_FILE:workforce>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/report-contract-test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/reports.cmake)
    set_tests_properties(app_reports PROPERTIES LABELS "app;integration" TIMEOUT 30)
endif()
