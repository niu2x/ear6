if(NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE is required")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

file(READ "${INPUT_FILE}" NES_DB_TEXT)

string(REPLACE "\\" "\\\\" NES_DB_TEXT_ESCAPED "${NES_DB_TEXT}")
string(REPLACE "\"" "\\\"" NES_DB_TEXT_ESCAPED "${NES_DB_TEXT_ESCAPED}")
string(REPLACE "\n" "\\n\"\n\"" NES_DB_TEXT_ESCAPED "${NES_DB_TEXT_ESCAPED}")

set(OUTPUT_CONTENT
"#pragma once

inline const char* get_embedded_nes_db_text() {
    return
\"${NES_DB_TEXT_ESCAPED}\";
}
")

file(WRITE "${OUTPUT_FILE}" "${OUTPUT_CONTENT}")
