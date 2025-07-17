#ifndef __PERIPHERAL_CONFIG_H
#define __PERIPHERAL_CONFIG_H           1

#include <vinfo.h>
#include <gpio.h>

    #define PROJECT_NAME                "smartcuckoo"
    #define PROJECT_VERSION             VERSION_INFO(1, 0, 2)
    #define PROJECT_ID                  "pnlb"
    #define PANEL_B

    #define SETTING_TIMEOUT             (15000)
    #define TMP_CONTENT_TIMEOUT         (3000)

    // common key and volume+/- is hold to repeat
    #define COMMON_KEY_REPEAT_INTV      (300)
    // light sensor det intv
    #define LIGHT_SENS_CUMUL_INTV       (1500)

// I2C
    #define I2C0_BUS_SPEED              (100)
    #define I2C1_BUS_SPEED              (100)

// environment sensor update seconds
    #define ENV_SENSOR_UPDATE_SECONDS   (5)

// buttons
    #define PIN_EXTIO                   PA04
    #define PIN_DIAL_CWA                PA06
    #define PIN_DIAL_CWB                PA07

// RTC calib
    #define PIN_RTC_CAL_IN              PB01

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
