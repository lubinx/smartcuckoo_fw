#include <ultracore/diskio.h>
#include <audio/file/lc3bin.h>
#include <fs/fat.h>

#include <adc.h>
#include <dac.h>
#include <i2c.h>
#include <pmu.h>
#include <uart.h>
#include <wdt.h>
#include <sdio.h>

#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MPLAYER_QUEUE_SIZE              (64)
#define MPLAYER_STACK_SIZE              (8192)


/****************************************************************************
 *  @public
 ****************************************************************************/
struct CLOCK_setting_t clock_setting = {0};
struct SMARTCUCKOO_setting_t setting = {0};

struct VOICE_attr_t voice_attr = {0};
static struct PMU_attr_t pmu_attr = {0};

/****************************************************************************
 *  @private
 ****************************************************************************/
static struct PMU_attr_t pmu_attr;
static struct DISKIO_attr_t sdmmc_diskio;
static struct SDMMC_attr_t sdmmc;
static struct FAT_attr_t fat;

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

static void MAIN_pmu_subscription(enum PMU_event_t event, enum PMU_mode_t mode, void *)
{
    if (PMU_EM3 == mode)
    {
        switch (event)
        {
        case PMU_EVENT_POWER_DOWN:
            break;

        case PMU_EVENT_SLEEP:
            DISKIO_flush_cache(&sdmmc_diskio);
            mplayer_thread_destroy();
            break;

        case PMU_EVENT_WAKEUP:
            mplayer_thread_create(MPLAYER_QUEUE_SIZE, MPLAYER_STACK_SIZE, NULL);
            break;
        }
    }
}

/****************************************************************************
 *  @implements: main
 ****************************************************************************/
int main(void)
{
    PMU_power_acquire();
    PMU_deepsleep_subscribe(&pmu_attr, MAIN_pmu_subscription, NULL);

    #ifndef NDEBUG
        LOG_set_level(LOG_VERBOSE);
    #endif

    UART_pin_mux(CONSOLE_DEV, CONSOLE_TXD, CONSOLE_RXD);
    __stdout_fd = UART_createfd(CONSOLE_DEV, 115200, UART_PARITY_NONE, UART_STOP_BITS_ONE);
    LOG_info("smartcuckoo %s booting", PROJECT_ID);

    #ifdef I2C0_SCL
        I2C_pin_mux(I2C0, I2C0_SCL, I2C0_SDA);
    #endif
    #ifdef I2C1_SCL
        I2C_pin_mux(I2C1, I2C1_SCL, I2C1_SDA);
    #endif

    PERIPHERAL_gpio_init();
    CLOCK_init();

    DISKIO_init(&sdmmc_diskio, 8);
    SDMMC_attr_init(&sdmmc, 3300, &sdmmc_diskio);
    FAT_attr_init(&fat, &sdmmc_diskio);

    if (1)
    {
        int err;

        if (0 != (err = SDMMC_pin_mux(&sdmmc, SDIO_DEV, SDIO_CLK, SDIO_CMD, SDIO_DAT)))
            goto sdmmc_print_err;

        if (0 != (err = SDMMC_card_insert(&sdmmc)))
            goto sdmmc_print_err;

        if (0 != (err = FAT_mount_fs_root(&fat, NULL)))
            goto sdmmc_print_err;

        #ifndef NDEBUG
            SDMMC_print(&sdmmc);
            FAT_attr_print(&fat);
        #endif

        if (0)
        {
        sdmmc_print_err:
            __BREAK_IFDBG();
            LOG_error("SDMMC error HALT: %s", SDMMC_strerror(err));
        }
    }

    #ifdef PIN_BATT_ADC
        ADC_attr_init(&batt_ad.attr, 3000, (void *)batt_adc_callback);
        ADC_attr_positive_input(&batt_ad.attr, PIN_BATT_ADC);
        ADC_attr_scale(&batt_ad.attr, BATT_AD_NUMERATOR, BATT_AD_DENOMINATOR);

        while (true)
        {
            PERIPHERAL_batt_ad_start();
            msleep(10);

            if (BATT_EMPTY_MV > batt_ad.value)
            {
                PERIPHERAL_on_sleep();
                msleep(250);
            }
            else
                break;
        }
    #endif

    if (1)  // REVIEW: bind DAC => audio renderer
    {
        static struct DAC_attr_t dac_attr;

        DAC_init(&dac_attr, true);
        DAC_amplifier_pin(&dac_attr, PIN_MUTE, PUSH_PULL_UP, 150);

        AUDIO_renderer_init(DAC_renderer, &dac_attr);
    }
    if (1)  // REVIEW: register LC3 & init mplayer 64 queue, 8k stack for LC3 decoding
    {
        LC3_register_fileio();
        mplayer_thread_create(MPLAYER_QUEUE_SIZE, MPLAYER_STACK_SIZE, NULL);
    }

    UCSH_register_fileio();

    // FIXME: debug IWDOG
    // #ifdef NDEBUG
    //     WDOG_init(8000);
    // #endif
    PERIPHERAL_init();

    PMU_power_release();
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
    ADC_start_convert(&batt_ad.attr, &batt_ad);
#endif
}

uint16_t PERIPHERAL_batt_ad_sync(void)
{
#ifdef PIN_BATT_ADC
    batt_ad.value = 0;
    PERIPHERAL_batt_ad_start();

    while (0 == batt_ad.value) pthread_yield();
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
