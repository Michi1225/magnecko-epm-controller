execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RESULT
)

if(NOT GIT_RESULT EQUAL 0)
    set(GIT_COMMIT_HASH "unknown")
endif()

# Check whether working tree contains uncommitted changes
execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(GIT_STATUS)
    set(GIT_DIRTY 1)
    set(GIT_VERSION "${GIT_COMMIT_HASH}-dirty")
else()
    set(GIT_DIRTY 0)
    set(GIT_VERSION "${GIT_COMMIT_HASH}")
endif()

set(VERSION_CONTENT
"#ifndef VERSION_H
#define VERSION_H

#define GIT_COMMIT_HASH \"${GIT_COMMIT_HASH}\"
#define GIT_DIRTY ${GIT_DIRTY}
#define GIT_VERSION \"${GIT_VERSION}\"

#define EPM_NUMBER ${EPM_NUMBER}

#endif /* VERSION_H */
")

# Write temporary file first
set(TEMP_FILE "${OUTPUT_FILE}.tmp")
file(WRITE "${TEMP_FILE}" "${VERSION_CONTENT}")

# Only change version.h if its contents actually changed
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TEMP_FILE}"
        "${OUTPUT_FILE}"
)

file(REMOVE "${TEMP_FILE}")