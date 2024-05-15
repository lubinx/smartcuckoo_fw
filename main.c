#include <adc.h>
#include <pmu.h>
#include <uart.h>
#include <wdt.h>
#include <i2c.h>

#include "smartcuckoo.h"

/****************************************************************************
 *  @public
 ****************************************************************************/
struct CLOCK_setting_t clock_setting = {0};
struct SMARTCUCKOO_setting_t setting = {0};

struct VOICE_attr_t voice_attr = {0};

/****************************************************************************
 *  @private: batt
 ****************************************************************************/
#ifdef PIN_BATT_ADC
    struct batt_ad_t
    {
        int value;

        struct
        {
            int cumul;
            int cumul_count;
        };

        struct ADC_attr_t attr;
        struct timeout_t lowbatt_intv;
    };

    static struct batt_ad_t batt_ad = {0};
    static void batt_adc_callback(int volt, int raw, struct batt_ad_t *ad);
#endif

/****************************************************************************
 *  @main
 ****************************************************************************/
int main(void)
{
    #ifdef UART0_TXD
        UART_configure(USART0, UART0_TXD, UART0_RXD);
    #endif
    #ifdef UART1_TXD
        UART_configure(USART1, UART1_TXD, UART1_RXD);
    #endif
    #ifdef I2C0_SCL
        I2C_configure(I2C0, I2C0_SCL, I2C0_SDA);
    #endif
    #ifdef I2C1_SCL
        I2C_configure(I2C1, I2C1_SCL, I2C1_SDA);
    #endif
    #ifdef PIN_MUTE
        GPIO_setdir_output(PUSH_PULL_DOWN, PIN_MUTE);
    #endif
    #ifdef PIN_MP3_POWER
        GPIO_setdir_output(PUSH_PULL_DOWN, PIN_MP3_POWER);
    #endif

    extern int __stdout_fd;
    __stdout_fd = UART_createfd(CONSOLE_DEV, 115200, UART_PARITY_NONE, UART_STOP_BITS_ONE);
    PERIPHERAL_gpio_init();

#ifdef PIN_BATT_ADC
    CMU->CLKEN0_SET = CMU_CLKEN0_BURAM;
    __NOP();

    ADC_attr_init(&batt_ad.attr, 1000, (void *)batt_adc_callback);
    ADC_attr_positive_input(&batt_ad.attr, PIN_BATT_ADC);
    ADC_attr_scale(&batt_ad.attr, BATT_AD_NUMERATOR, BATT_AD_DENOMINATOR);

    while (true)
    {
        PERIPHERAL_batt_ad_start();

        if (BATT_EMPTY_MV > batt_ad.value)
        {
            PERIPHERAL_on_sleep();
            msleep(500);
        }
        else
            break;
    }
#endif
    // WDOG_init(4000);
    PERIPHERAL_init();

    SHELL_bootstrap();
}

enum LOCALE_dfmt_t LOCALE_dfmt(void)
{
    if (DFMT_DEFAULT == setting.locale.dfmt)
        return VOICE_get_default_dfmt(&voice_attr);
    else
        return setting.locale.dfmt;
}

enum LOCALE_hfmt_t LOCALE_hfmt(void)
{
    if (DFMT_DEFAULT == setting.locale.dfmt)
        return VOICE_get_default_hfmt(&voice_attr);
    else
        return setting.locale.hfmt;
}

/****************************************************************************
 *  @implements: mplayer gpio override
 ****************************************************************************/
#ifndef PIN_MP3_POWER
    static bool mp3_is_powered;
#endif

bool mplayer_gpio_is_powered(void)
{
    #ifdef PIN_MP3_POWER
        return 0 != GPIO_peek_output(PIN_MP3_POWER);
    #else
        return mp3_is_powered;
    #endif
}

void mplayer_gpio_power_on(void)
{
    PMU_power_acquire();
    mplayer_gpio_mute();

    #ifdef PIN_MP3_POWER
        GPIO_set(PIN_MP3_POWER);
    #else
        mp3_is_powered = true;
    #endif
}

void mplayer_gpio_power_off(void)
{
    mplayer_gpio_mute();

    #ifdef PIN_MP3_POWER
        GPIO_clear(PIN_MP3_POWER);
    #else
        mp3_is_powered = false;
    #endif

    PMU_power_release();
}

void mplayer_gpio_mute(void)
{
    #ifdef PIN_MUTE
        GPIO_set(PIN_MUTE);
    #endif
}

void mplayer_gpio_unmute(void)
{
    #ifdef PIN_MUTE
        GPIO_clear(PIN_MUTE);
    #endif
}

/***************************************************************************/
/** @weak
****************************************************************************/
__attribute__((weak))
void PERIPHERAL_gpio_init(void)
{
}

__attribute__((weak))
void PERIPHERAL_init(void)
{
}

__attribute__((weak))
uint8_t PERIPHERAL_sync_id(void)
{
    return 0;
}

__attribute__((weak))
void PERIPHERAL_on_ready(void)
{
    PERIPHERAL_adv_start();
}

__attribute__((weak))
void PERIPHERAL_on_connected(void)
{
}

__attribute__((weak))
void PERIPHERAL_on_disconnect(void)
{
}

__attribute__((weak))
void PERIPHERAL_on_sleep(void)
{
}

__attribute__((weak))
void PERIPHERAL_on_wakeup(void)
{
}

/***************************************************************************/
/** battery
****************************************************************************/
void PERIPHERAL_batt_ad_start(void)
{
#ifdef PIN_BATT_ADC
    if (0 == ADC_start_convert(&batt_ad.attr, &batt_ad))
        PMU_power_acquire();
#endif
}

uint16_t PERIPHERAL_batt_ad_sync(void)
{
#ifdef PIN_BATT_ADC
    batt_ad.value = 0;
    PERIPHERAL_batt_ad_start();

    while (0 == batt_ad.value) msleep(0);

    if ((int)(BATT_EMPTY_MV - 300) > batt_ad.value)
        NVIC_SystemReset();
    else
        return (uint16_t)batt_ad.value;
#else
    return 0;
#endif
}

uint16_t PERIPHERAL_batt_volt(void)
{
#ifdef PIN_BATT_ADC
    return (uint16_t)batt_ad.value;
#else
    return 0;
#endif
}

#ifdef PIN_BATT_ADC
static void batt_adc_callback(int volt, int raw, struct batt_ad_t *ad)
{
    ARG_UNUSED(raw);
    batt_ad.cumul += volt;

    if (5 == ++ ad->cumul_count)
    {
        batt_ad.value = ad->cumul / ad->cumul_count;
        ad->cumul = 0;
        ad->cumul_count = 0;

        ADC_stop_convert(&ad->attr);
        PMU_power_release();
    }
}
#endif

uint8_t BATT_mv_level(uint32_t mV)
{
    if (mV < BATT_LOW_MV)
    {
        if (BATT_EMPTY_MV > mV) // 0%
            return 0;
        else                    // 0~10%
            return (uint8_t)((mV - BATT_EMPTY_MV) * 10 / (BATT_LOW_MV - BATT_EMPTY_MV));
    }
    else
    {
        if (BATT_FULL_MV > mV)  // 10~100%
            return (uint8_t)(10 + (mV - BATT_LOW_MV) * 90 / (BATT_FULL_MV - BATT_LOW_MV));
        else                    // 100%
            return 100;
    }
}
