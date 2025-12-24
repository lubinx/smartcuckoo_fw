cmake_minimum_required(VERSION 3.20)
include(".vscode/intro.cmake")
project(smartcuckoo)

set(COMMON_SRCS
    "datetime_utils.c"
    "clock.c"
    "voice.c"
    "main.cpp"
    "shell.cpp"
    "shell_ota.c"
    "ultracore/src/usb/*.c"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND COMMON_SRCS
        "ultracore/src/audio/mplayer.c"
        "ultracore/src/audio/mynoise.c"
        "ultracore/src/audio/renderer.c"
        # "ultracore/src/audio/wavefile.c"
        # "ultracore/src/audio/codec/es8156.c"
        # "ultracore/src/hw/common_i2s.c"
        # "ultracore/src/core/retarget.c"
    )
endif()

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
add_custom_command(TARGET talking_button
    POST_BUILD
    COMMAND
        ${CMAKE_OBJCOPY} -O binary "${CMAKE_BINARY_DIR}/talking_button.elf" "talking_button.bin"
    COMMAND
        gzip -f "talking_button.bin"
    COMMAND
        stat -c "talking_button.bin.gz: %s" "talking_button.bin.gz"
    COMMAND
        echo --------------------------------------------------
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../_bin"
    VERBATIM
)

build_target(zone
    DEVICE
        "N32WB452LE"
    DRIVER
        "../n32x45x"
    CONFIG_DIR
        "zone/config"
    INCLUDE_DIRS
        "."
    SRCS
        ${COMMON_SRCS}
        "zone/*.c"
        "noise_ringtone.c"
    DEFINITIONS
        NVM_TAG="zone"
        ADC_VREF=0
    DEPENDENCIES
        "lc3"
    OUTPUT_DIR
        "../_bin"
)
add_custom_command(TARGET zone
    POST_BUILD
    COMMAND
        ${CMAKE_OBJCOPY} -O binary "${CMAKE_BINARY_DIR}/zone.elf" "zone.bin"
    COMMAND
        gzip -f "zone.bin"
    COMMAND
        stat -c "zone.bin.gz: %s" "zone.bin.gz"
    COMMAND
        echo --------------------------------------------------
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../_bin"
    VERBATIM
)

build_target(zinc
    DEVICE
        "N32WB452LE"
    DRIVER
        "../n32x45x"
    CONFIG_DIR
        "zinc/config"
    INCLUDE_DIRS
        "."
    SRCS
        ${COMMON_SRCS}
        "zinc/*.c"
        "ws2812b_driver.c"
        "ultracore/src/hw/smart_led.c"
    DEFINITIONS
        NVM_TAG="zinc"
        ADC_VREF=0
    DEPENDENCIES
        "lc3"
    OUTPUT_DIR
        "../_bin"
)
add_custom_command(TARGET zinc
    POST_BUILD
    COMMAND
        ${CMAKE_OBJCOPY} -O binary "${CMAKE_BINARY_DIR}/zinc.elf" "zinc.bin"
    COMMAND
        gzip -f "zinc.bin"
    COMMAND
        stat -c "zinc.bin.gz: %s" "zinc.bin.gz"
    COMMAND
        echo --------------------------------------------------
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../_bin"
    VERBATIM
)

# NOTE: required lc3
option(LC3_PLUS OFF)
add_subdirectory("${ULTRACORE_ROOT}/3party/lc3" "lc3")

print_targets()
