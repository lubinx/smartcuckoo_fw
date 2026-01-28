#ifndef __PERIPHERAL_CONFIG_H
#define __PERIPHERAL_CONFIG_H           1

#include <vinfo.h>
#include <gpio.h>

    #define PROJECT_NAME                "smartcuckoo"
    #define PROJECT_VERSION             VERSION_INFO(1, 0, 0)
    #define PROJECT_ID                  "zinc"

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

    #define BATT_FULL_MV                (4150)
    #define BATT_HINT_MV                (3300)
    #define BATT_LOW_MV                 (3150)
    #define BATT_EMPTY_MV               (3000)

    // batt adc interval
    #define BATT_AD_INTV_SECONDS        (900)

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
    #define EAR_PIN                     PA04

// LEDs
    #define LED_TIME_DAT                PC07
    #define LED_WDAYS_DAT               PC08
    #define LED_LAMP_DAT                PA06

    #define LED_CLOCK_DIS               PB06
    #define LED_LAMP_DIS                PB05

    #define POWER_OFF_STEP_SECONDS      (1800U)

// light sensor
    #define LIGHT_SENSOR_AD             PA03

// buttons
    #define PIN_ROW_1                   PC09
    #define PIN_ROW_2                   PD15
    #define PIN_ROW_3                   PC11
    #define PIN_COL_1                   PC01
    #define PIN_COL_2                   PA02
    #define PIN_COL_3                   PD00

    #define GPIO_FILTER_INTV            (50)
    #define SETTING_VOLUME_ADJ_INTV     (60)
    #define SETTING_LAMP_DIM_INTV       (50)
    #define SETTING_BLINKY_INTV         (500)

    #define LONG_PRESS_VOICE            (1000)
    #define LONG_PRESS_SETTING          (3000)
    #define LONG_PRESS_DIM              (380)

// volume
    #define VOLUME_MAX_PERCENT          (100)
    #define VOLUME_MIN_PERCENT          (5)

#endif
