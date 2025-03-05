cmake_minimum_required(VERSION 3.20)

include(".vscode/intro.cmake")
# no enough flash spaces
set(CMAKE_C_FLAGS_DEBUG "-O1 -g" CACHE INTERNAL "debug build")
set(CMAKE_CXX_FLAGS_DEBUG "-O1 -g" CACHE INTERNAL "debug build")

project(smartcuckoo)

set(COMMON_SRCS
    "datetime_utils.c"
    "clock.c"
    "mplayer_voice.c"
    "fstab.c"
    # main
    "main.c"
    "shell.cpp"
    "shell_ota.c"
    # ${ULTRACORE_ROOT}/src/sh/ucsh.c
    # ${ULTRACORE_ROOT}/src/hw/common_i2c.c
)

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
        "ultracore/src/hw/common_timer.c"
    DEFINITIONS
        NVM_TAG="talking button"
    OUTPUT_DIR
        "../_bin"
)

# set_target_properties(ultracore PROPERTIES
#     INTERFACE_LINK_LIBRARIES "${LINK_DRIVER}"
# )


# build_target(panel_b
#     DEVICE
#         "EFR32BG22C224F512GM40"
#     DRIVER
#         "../efr32xg22"
#     CONFIG_DIR
#         "panel/b_config"
#     INCLUDE_DIRS
#         "."
#         "./sensors"
#         "./io_ext"
#         "./panel"
#         "./panel/drv"
#     SRCS
#         ${COMMON_SRCS}
#         "panel/*.c"
#         "sensors/*.c"
#         "io_ext/xl9535.c"
#     SRCS-
#         "panel/lamp.c"
#         "panel/panel_ml_i2c_drv.c"
#         "sensors/env_sht4x.c"
#     DEFINITIONS
#         NVM_TAG="panel_c"
#     OUTPUT_DIR
#         "../_bin"
# )

# build_target(panel_c
#     DEVICE
#         "EFR32BG22C224F512GM40"
#     DRIVER
#         "../efr32xg22"
#     CONFIG_DIR
#         "panel/c_config"
#     INCLUDE_DIRS
#         "."
#         "./sensors"
#         "./io_ext"
#         "./panel"
#         "./panel/drv"
#     SRCS
#         ${COMMON_SRCS}
#         "panel/*.c"
#         "sensors/*.c"
#         "io_ext/bs81xc.c"
#     SRCS-
#         "panel/panel_ml_i2c_drv.c"
#         "sensors/env_sht4x.c"
#     DEFINITIONS
#         NVM_TAG="panel_c"
#     OUTPUT_DIR
#         "../_bin"
# )

print_targets()
