#ifndef __PERIPHERAL_CONFIG_H
#define __PERIPHERAL_CONFIG_H           1

#include <gpio.h>

    #define PROJECT_NAME                "smartcuckoo"
    #define PROJECT_VERSION             VERSION_INFO(1, 0, 23)
    #define PROJECT_ID                  "tbtn"

// console
    #define CONSOLE_DEV                 USART1
    #define CONSOLE_TXD                 PA09
    #define CONSOLE_RXD                 PA10

// buttons
    #define PIN_VOICE_BUTTON            PC00
    #define PIN_SETTING_BUTTON          PC01
    #define PIN_ALARM_ON                PA02

// rtc
    #define PIN_RTC_CAL_IN              PB00

// batt
    #define PIN_BATT_ADC                PA01
    #define BATT_AD_NUMERATOR           (390U + 750U)
    #define BATT_AD_DENOMINATOR         (390U)

    #define BATT_FULL_MV                (3150)
    #define BATT_HINT_MV                (2300)
    #define BATT_LOW_MV                 (2150)
    #define BATT_EMPTY_MV               (1950)

    // batt adc interval
    #define BATT_AD_INTV_SECONDS        (3600)

// SDIO
    #define SDIO_DEV                    SDIO
    #define SDIO_CLK                    PE12
    #define SDIO_CMD                    PE13
    #define SDIO_DAT                    PE08, 0     // PE09, PE10, PE11, 0
    #define SDIO_POWER_PIN              PB01
    #define SDIO_POWER_PULL             PUSH_PULL_DOWN

// mplayer & lineout
    #define PIN_MUTE                    PB14

#endif
