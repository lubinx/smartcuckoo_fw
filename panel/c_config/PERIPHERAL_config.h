#ifndef __PERIPHERAL_CONFIG_H
#define __PERIPHERAL_CONFIG_H           1

#include <features.h>
#include <gpio.h>

    #define PROJECT_NAME                "smartcuckoo"
    #define PROJECT_VERSION             VERSION_INFO(1, 0, 1)
    #define PROJECT_ID                  "pnlc"
    #define PANEL_C

    #define SETTING_TIMEOUT             (15000)
    // common key and volume+/- is hold to repeat
    #define COMMON_KEY_REPEAT_INTV      (300)
    // light sensor det intv
    #define LIGHT_SENS_CUMUL_INTV       (1000)

// I2C
    #define DISPLAY_I2C                 (I2C0)

// environment sensor update seconds
    #define ENV_SENSOR_UPDATE_SECONDS   (5)

// buttons
    #define PIN_SNOOZE                  PC05
    #define PIN_MESSAGE                 PA03
    #define PIN_EXTIO                   PA04

// RTC calib
    #define PIN_RTC_CAL_IN              PB01

// LED
    #define PIN_LAMP                    PA06

// AD / IrDA
    #define PIN_LIGHT_SENS_AD           PC04
    #define PIN_IRDA                    PC03

// mp3 player & lineout
    #define PIN_MUTE                    PA00
    #define PIN_HEADPHONE_DET           PA08
    #define PIN_PLAY_BUSYING            PB00

// misc
    #define PIN_EXT_5V_DET              PB02

#endif
