cmake_minimum_required(VERSION 3.20)
include(".vscode/intro.cmake")
project(smartcuckoo)

set(COMMON_SRCS
    "datetime_utils.c"
    "clock.c"
    "voice.c"
    # main
    "main.cpp"
    "shell.cpp"
    "shell_ota.c"
    "ultracore/src/usb/*.c"
)

# remove prebuild ultracore archs
file(GLOB glob_list "${CMAKE_BINARY_DIR}/talking_button.*")
foreach(iter ${glob_list})
    file(REMOVE ${iter})
endforeach()

build_target(talking_button
    DEVICE
        "N32WB452LE"
    DRIVER
        "../n32x45x"
    CONFIG_DIR
        "talking_button/config"
    INCLUDE_DIRS
        "."
    SRCS
        ${COMMON_SRCS}
        "talking_button/*.c"
    DEFINITIONS
        NVM_TAG="talking button"
    DEPENDENCIES
        "lc3"
    OUTPUT_DIR
        "../_bin"
)

# generate talking_button.gz
add_custom_command(TARGET talking_button
    POST_BUILD
    COMMAND
        echo "firmare: "
    COMMAND
        ${CMAKE_SIZE_UTIL} "talking_button.hex"
    COMMAND
        ${CMAKE_OBJCOPY} -O binary "${CMAKE_BINARY_DIR}/talking_button.elf" "talking_button.bin"
    COMMAND
        echo "ota filesize: "
    COMMAND
        gzip -f "talking_button.bin"
    COMMAND
        stat -c "\t%s" "talking_button.bin.gz"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../_bin"
    VERBATIM
)

# if (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
#     build_target(panel_b
#         DEVICE
#             "EFR32BG22C224F512GM40"
#         DRIVER
#             "../efr32xg22"
#         CONFIG_DIR
#             "panel/b_config"
#         INCLUDE_DIRS
#             "."
#             "./sensors"
#             "./io_ext"
#             "./panel"
#             "./panel/drv"
#         SRCS
#             ${COMMON_SRCS}
#             "panel/*.c"
#             "sensors/*.c"
#             "io_ext/xl9535.c"
#         SRCS-
#             "panel/lamp.c"
#             "panel/panel_ml_i2c_drv.c"
#             "sensors/env_sht4x.c"
#         DEPENDENCIES
#             "lc3"
#         DEFINITIONS
#             NVM_TAG="panel_c"
#         OUTPUT_DIR
#             "../_bin"
#     )

#     build_target(panel_c
#         DEVICE
#             "EFR32BG22C224F512GM40"
#         DRIVER
#             "../efr32xg22"
#         CONFIG_DIR
#             "panel/c_config"
#         INCLUDE_DIRS
#             "."
#             "./sensors"
#             "./io_ext"
#             "./panel"
#             "./panel/drv"
#         SRCS
#             ${COMMON_SRCS}
#             "panel/*.c"
#             "sensors/*.c"
#             "io_ext/bs81xc.c"
#         SRCS-
#             "panel/panel_ml_i2c_drv.c"
#             "sensors/env_sht4x.c"
#         DEPENDENCIES
#             "lc3"
#         DEFINITIONS
#             NVM_TAG="panel_c"
#         OUTPUT_DIR
#             "../_bin"
#     )
# endif()

# NOTE: required lc3
option(LC3_PLUS OFF)
option(LC3_PLUS_HR OFF)
add_subdirectory("${ULTRACORE_ROOT}/3party/lc3" "lc3")

print_targets()
