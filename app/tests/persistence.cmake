file(MAKE_DIRECTORY "${WORK_DIR}/db data")
file(REMOVE "${WORK_DIR}/db data/team.db")
file(WRITE "${WORK_DIR}/app.yaml" "database:\n  path: 'db data/team.db'\n  busy_timeout_ms: 0\n")
file(WRITE "${WORK_DIR}/overlay.yaml" "database: {busy_timeout_ms: 100}\n")
file(WRITE "${WORK_DIR}/create.txt"
    "1\nAna Silva\n30\nDeveloper\nEngineering\nSenior\n3000\n3\n1\n2026\n10\n10\n25.50\n0\n")
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/create.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "um novo foi criado" OR NOT output MATCHES "ID: 1")
    message(FATAL_ERROR "Initial persistent session failed: ${result}\n${output}\n${error}")
endif()
file(WRITE "${WORK_DIR}/read.txt" "2\n5\n2026\n10\n0\n")
execute_process(COMMAND "${APP}" --config app.yaml --config overlay.yaml WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/read.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "Conexao com o banco de dados realizada com sucesso" OR
   NOT output MATCHES "Ana Silva: 3255.00" OR NOT output MATCHES "Contrato 1")
    message(FATAL_ERROR "Data did not survive process restart: ${result}\n${output}\n${error}")
endif()
file(WRITE "${WORK_DIR}/delete.txt" "6\n1\n0\n")
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/delete.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "Operacao concluida")
    message(FATAL_ERROR "Persistent removal failed: ${output}\n${error}")
endif()
execute_process(COMMAND "${APP}" WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/read.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT output MATCHES "Nenhum funcionario cadastrado")
    message(FATAL_ERROR "Removed worker reappeared: ${output}\n${error}")
endif()
file(WRITE "${WORK_DIR}/bad.yaml" "database: {busy_timeout_ms: -1}\n")
execute_process(COMMAND "${APP}" --config bad.yaml WORKING_DIRECTORY "${WORK_DIR}"
    INPUT_FILE "${WORK_DIR}/read.txt" OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result)
if(result EQUAL 0 OR NOT error MATCHES "Erro de configuracao")
    message(FATAL_ERROR "Invalid configuration was not rejected: ${output}\n${error}")
endif()
file(REMOVE "${WORK_DIR}/db data/team.db")
