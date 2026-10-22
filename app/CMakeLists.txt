add_executable(workforce main.c)
target_link_libraries(workforce PRIVATE workforce_application workforce_ui workforce_config project_options)
# Refresh assets even for XML-only edits, beside each configuration's executable.
add_custom_target(workforce_ui_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/ui $<TARGET_FILE_DIR:workforce>/ui VERBATIM)
add_dependencies(workforce workforce_ui_assets)
if(BUILD_TESTING)
    add_test(NAME app_ui_sessions COMMAND ${CMAKE_COMMAND}
        -DAPP=$<TARGET_FILE:workforce>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/ui-session-test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/sessions.cmake)
    set_tests_properties(app_ui_sessions PROPERTIES LABELS "app;integration;ui" TIMEOUT 30)
endif()
