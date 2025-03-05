find_package(Git REQUIRED)

set(TOOLCHAIN arm-none-eabi)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# locate build script parent folder or local clone
if((NOT EXISTS "${PROJECT_DIR}/.cmake") AND  (EXISTS "${PROJECT_DIR}/../.cmake"))
    get_filename_component(dir "${PROJECT_DIR}/../.cmake" ABSOLUTE)

    execute_process(
        COMMAND
            ln -s "${dir}" "${PROJECT_DIR}/.cmake"
        WORKING_DIRECTORY
            ${PROJECT_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

if(NOT EXISTS "${PROJECT_DIR}/.cmake")
    message("💡 clone build script")
    message("\tgit clone https://github.com/lubinx/.cmake.git")

    execute_process(
        COMMAND
            ${GIT_EXECUTABLE} clone https://github.com/lubinx/.cmake.git --depth=1 -q
        WORKING_DIRECTORY
            ${PROJECT_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
include("${PROJECT_DIR}/.cmake/build.cmake")

# if((NOT EXISTS "${PROJECT_DIR}/liblc3") AND  (EXISTS "${PROJECT_DIR}/../ultracore/3party/liblc3"))
#     get_filename_component(dir "${PROJECT_DIR}/../ultracore/3party/liblc3" ABSOLUTE)
#     message(${dir})

#     execute_process(
#         COMMAND
#             ln -s "${dir}" "${PROJECT_DIR}/liblc3"
#         WORKING_DIRECTORY
#             ${PROJECT_DIR}
#         COMMAND_ERROR_IS_FATAL ANY
#     )
# endif()
