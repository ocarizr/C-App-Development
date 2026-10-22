file(MAKE_DIRECTORY "${WORK_DIR}")
file(REMOVE "${WORK_DIR}/team.db" "${WORK_DIR}/filtered.csv")
file(WRITE "${WORK_DIR}/app.yaml" "database: {path: team.db, busy_timeout_ms: 0}\n")
file(WRITE "${WORK_DIR}/create.txt"
    "1\nAna\n30\nDeveloper\nEngineering\nSenior\n3000\n1\nBruno\n30\nDesigner\nDesign\nJunior\n2000\n3\n1\n2026\n10\n1\n10\n3\n1\n2026\n10\n1\n20\n9\n1\n2026\n11\n4000\n0\n")
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/create.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "Contrato salvo. ID: 2")
    message(FATAL_ERROR "Contract creation failed: ${output}\n${error}")
endif()
file(WRITE "${WORK_DIR}/edit-export.txt"
    "4\n1\n1\n11\n1\n2\n2026\n11\n10\n30\n13\n2026\n11\nEngineering\nSenior\nfiltered.csv\n0\n")
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/edit-export.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "Relatorio exportado para filtered.csv")
    message(FATAL_ERROR "Contract edit/export failed: ${output}\n${error}")
endif()
file(READ "${WORK_DIR}/filtered.csv" csv)
if(NOT csv MATCHES "WORKER,2026,11,1," OR NOT csv MATCHES "4300.00" OR csv MATCHES "Bruno" OR
   NOT csv MATCHES "DEPARTMENT,2026,11,,,,\"Engineering\",,1,4000.00,300.00,4300.00")
    message(FATAL_ERROR "Filtered CSV contents are incorrect: ${csv}")
endif()
file(WRITE "${WORK_DIR}/verify.txt"
    "7\n1\n12\n2026\n11\n*\n*\n13\n2026\n11\n*\n*\nfiltered.csv\n0\n")
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/verify.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "Contrato ID 2.*2026-11" OR
   output MATCHES "Contrato ID 1 " OR NOT output MATCHES "Total da equipe: 6300.00" OR
   NOT output MATCHES "Nao foi possivel exportar")
    message(FATAL_ERROR "Persistent contract identity/report check failed: ${output}\n${error}")
endif()
file(READ "${WORK_DIR}/filtered.csv" after)
if(NOT after STREQUAL csv)
    message(FATAL_ERROR "Existing export was overwritten")
endif()
file(REMOVE "${WORK_DIR}/team.db" "${WORK_DIR}/filtered.csv")
