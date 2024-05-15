find_package(Git REQUIRED)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# locate build script parent folder or local clone
if((NOT EXISTS "${PROJECT_DIR}/.cmake") AND  (EXISTS "${PROJECT_DIR}/../.cmake"))
    get_filename_component(dir "${PROJECT_DIR}/../.cmake" ABSOLUTE)

    execute_process(
        COMMAND
            ln -s "${PROJECT_DIR}/../.cmake" "${PROJECT_DIR}/.cmake"
        WORKING_DIRECTORY
            ${PROJECT_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

string(TIMESTAMP today "%Y%m%d")

if(NOT EXISTS "${PROJECT_DIR}/.cmake")
    message("💡 update build script")
    message("\tgit clone https://github.com/lubinx/.cmake.git")

    execute_process(
        COMMAND
            ${GIT_EXECUTABLE} clone https://github.com/lubinx/.cmake.git --depth=1 -q
        WORKING_DIRECTORY
            ${PROJECT_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
else()
    if((NOT DEFINED CACHE{update_daily_ts}) OR (NOT $CACHE{update_daily_ts} STREQUAL ${today}))
        message("💡 update build script")
        message("\tthis will only execute once per day")

        execute_process(
            COMMAND
                ${GIT_EXECUTABLE} checkout master -q
            WORKING_DIRECTORY
                ${PROJECT_DIR}/.cmake
            COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(
            COMMAND
                ${GIT_EXECUTABLE} pull -q
            WORKING_DIRECTORY
                ${PROJECT_DIR}/.cmake
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()
endif()
set(update_daily_ts ${today} CACHE INTERNAL "update build script once per day")

set(TOOLCHAIN arm-none-eabi)
include("${PROJECT_DIR}/.cmake/build.cmake")
