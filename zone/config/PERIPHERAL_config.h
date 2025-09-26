#ifndef __PERIPHERAL_CONFIG_H
#define __PERIPHERAL_CONFIG_H           1

#include <vinfo.h>
#include <gpio.h>

    #define PROJECT_NAME                "smartcuckoo"
    #define PROJECT_VERSION             VERSION_INFO(1, 0, 1)
    #define PROJECT_ID                  "zone"

// console
    #define CONSOLE_DEV                 USART1
    #define CONSOLE_TXD                 PA09
    #define CONSOLE_RXD                 PA10

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

// USB
    #define USB_PINS                    PA12, PA11

// SDIO
    #define SDIO_CLK                    PE12
    #define SDIO_CMD                    PE13
    #define SDIO_DAT                    PE08, PE09, PE10, PE11, 0
    #define SDIO_POWER_PIN              PB01
    #define SDIO_POWER_PULL             PUSH_PULL_DOWN

// I2S
    #define I2S_PINS                    PC06, PB13, PB12, PB15, 0
    #define I2S_CODEC_I2C_PINS          I2C2, PB10, PB11

// ampifier
    #define AMPIFIER_PIN                PB14
    #define AMPIFIER_EN_PULL            PUSH_PULL_DOWN

// LEDs
    #define LED_POWER                   PD08
    #define LED1                        PA07
    #define LED2                        PC04
    #define LED3                        PC05

    #define POWER_OFF_STEP_SECONDS      (1800U)

// buttons
    #define PIN_TOP_BUTTON              PC11
    #define PIN_POWER_BUTTON            PA02
    #define PIN_PREV_BUTTON             PC01
    #define PIN_NEXT_BUTTON             PC09
    #define PIN_VOLUME_UP_BUTTON        PD15
    #define PIN_VOLUME_DOWN_BUTTON      PD00

    #define LONG_PRESS_VOICE            (1000)
    #define LONG_PRESS_SETTING          (3000)
    #define VOLUME_ADJ_HOLD_INTV        (250)

// volume
    #define VOLUME_MAX_PERCENT          (100)
    #define VOLUME_MIN_PERCENT          (10)

#endif
