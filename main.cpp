#include <ultracore/diskio.h>
#include <audio/lc3bin.h>
#include <usb/usbd_scsi.h>
#include <fs/fat.h>

#include <adc.h>
#include <dac.h>
#include <i2c.h>
#include <sdmmc.h>

#include "smartcuckoo.h"

#ifdef I2S_PINS
    #include <i2s.h>
    #include <audio/codec/es8156.h>
#endif

#ifdef DEBUG
    #include <rcc.h>
#endif

#ifndef BATT_FULL_MV
    #pragma GCC error "not defined BATT_FULL_MV");
#endif
#ifndef BATT_HINT_MV
    #pragma GCC error "not defined BATT_HINT_MV for hint low battery");
#endif

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MPLAYER_QUEUE_SIZE              (32)
#define MPLAYER_STACK_SIZE              (8192)

/****************************************************************************
 *  @public
 ****************************************************************************/
struct SMARTCUCKOO_setting_t setting;

bool CLOCK_alarm_switch_is_on(void)
{
    return setting.alarm_is_on;
}

enum LOCALE_dfmt_t LOCALE_dfmt(void)
{
    if (DFMT_DEFAULT == setting.locale.dfmt)
        return VOICE_get_default_dfmt();
    else
        return setting.locale.dfmt;
}

enum LOCALE_hfmt_t LOCALE_hfmt(void)
{
    if (DFMT_DEFAULT == setting.locale.dfmt)
        return VOICE_get_default_hfmt();
    else
        return setting.locale.hfmt;
}


/****************************************************************************
 *  @private
 ****************************************************************************/
static struct DISKIO_attr_t sdmmc_diskio;
static struct SDMMC_attr_t sdmmc;
static struct FAT_attr_t fat;

static struct USBD_SCSI_attr_t usbd_scsi;
static struct PMU_attr_t pmu_attr;

#ifdef I2S_PINS
    static struct I2S_attr_t i2s_attr;
#else
    static struct DAC_audio_sink_attr_t dac_sink;
#endif

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

    static struct batt_ad_t batt_ad;
#endif

void SDIO_power_ctrl(struct SDMMC_implement_t const *sdio_impl, bool en)
{
    (void)sdio_impl, (void)en;
    if (en)
    {
        GPIO_setdir_output(SDIO_POWER_PULL, SDIO_POWER_PIN);
    }
    else
    {
        GPIO_config_np(SDIO_POWER_PIN, GPIO_ANALOG);
    }
}

/****************************************************************************
 *  @implements: main
 ****************************************************************************/
int main(void)
{
    #ifndef NDEBUG
        LOG_set_level(LOG_VERBOSE);
    #endif

    UART_pin_mux(CONSOLE_DEV, CONSOLE_TXD, CONSOLE_RXD);
    __stdout_fd = UART_createfd(CONSOLE_DEV, 115200, UART_PARITY_NONE, UART_STOP_BITS_ONE);
    LOG_info("smartcuckoo %s booting", PROJECT_ID);

    #ifdef DEBUG
        printf("CLK configure:\n");
        printf("\tSYSCLK: %u MHz\n", (unsigned)(SYSCLK_FREQ / _MHZ));
        printf("\tAPB1  : %u MHz\n", (unsigned)(APB1_FREQ / _MHZ));
        printf("\tAPB2  : %u MHz\n", (unsigned)(APB2_FREQ / _MHZ));
        printf("\n");
    #endif

    #ifdef I2C0_SCL
        I2C_pin_mux(I2C0, I2C0_SCL, I2C0_SDA);
    #endif
    #ifdef I2C1_SCL
        I2C_pin_mux(I2C1, I2C1_SCL, I2C1_SDA);
    #endif

#ifdef NDEBUG
    WDOG_init(8000);
#endif

    PERIPHERAL_gpio_init();
    CLOCK_init();

    #ifdef I2S_PINS
        I2S_attr_init(&i2s_attr, I2S2, I2S_PINS);
        I2S_amplifier_pin(&i2s_attr, AMPIFIER_PIN, AMPIFIER_EN_PULL, 350);

        I2S_attr_init_codec(&i2s_attr, ES8156_codec, I2S_CODEC_I2C_PINS);
    #else
        DAC_init_audio_sink(&dac_sink, true);
        DAC_amplifier_pin(&dac_sink, AMPIFIER_PIN, AMPIFIER_EN_PULL, 150);
    #endif

    #ifdef PIN_BATT_ADC
        ADC_attr_init(&batt_ad.attr, 3000, [](int volt, int raw, void *arg)-> void
            {
                struct batt_ad_t *ad = (struct batt_ad_t *)arg;

            #ifdef DEBUG
                ARG_UNUSED(volt, raw);
                ad->value = BATT_FULL_MV;
                ADC_stop_convert(&ad->attr);
            #else
                ARG_UNUSED(raw);
                batt_ad.cumul += volt;

                if (5 == ++ ad->cumul_count)
                {
                    ad->value = ad->cumul / ad->cumul_count;
                    ad->cumul = 0;
                    ad->cumul_count = 0;
                    ADC_stop_convert(&ad->attr);
                }
            #endif
            });

        ADC_attr_positive_input(&batt_ad.attr, PIN_BATT_ADC);
        ADC_attr_scale(&batt_ad.attr, BATT_AD_NUMERATOR, BATT_AD_DENOMINATOR);

        PERIPHERAL_batt_ad_start();
    #endif

    PMU_power_lock();
    PMU_deepsleep_subscribe(&pmu_attr, [](enum PMU_event_t event, enum PMU_mode_t, void *) -> void
        {
            if (PMU_EVENT_SLEEP == event)
            {
                SDMMC_card_remove(&sdmmc);
                DISKIO_flush_cache(&sdmmc_diskio);
            }
        },
    NULL);

    DISKIO_init(&sdmmc_diskio, 48);
    FAT_attr_init(&fat, &sdmmc_diskio);
    SDMMC_attr_init(&sdmmc, 3300, 0, &sdmmc_diskio);

    if (1)
    {
        int err;

        if (0 != (err = SDMMC_pin_mux(&sdmmc, sdio, SDIO_CLK, SDIO_CMD, SDIO_DAT)))
            goto sdmmc_print_err;
        if (0 != (err = SDMMC_card_insert(&sdmmc)))
            goto sdmmc_print_err;
        if (0 != (err = FAT_mount_fs_root(&fat)))
            goto sdmmc_print_err;

        #ifndef NDEBUG
            SDMMC_print(&sdmmc);
            FAT_attr_print(&fat);
        #endif

        if (0)
        {
        sdmmc_print_err:
            LOG_error("SDMMC error: %s", SDMMC_strerror(err));
        }
    }
    if (PERIPHERAL_is_enable_usb())
    {
        USBD_pin_mux(USB, USB_PINS);
        USBD_SCSI_init(&usbd_scsi, USB, &sdmmc_diskio);

        USBD_suspend_callback(&usbd_scsi.usbd_attr, [](struct USBD_attr_t *) -> void
        {
            if (0 == FAT_mount_fs_root(&fat))
                FAT_attr_print(&fat);
        });
    }
    if (1)
    {
        LC3_register_codec();
        mplayer_init(MPLAYER_QUEUE_SIZE);
    }

    PERIPHERAL_init();
    UCSH_register_fileio();

    PMU_power_unlock();
    SHELL_bootstrap();
}

/***************************************************************************/
/** @weak
****************************************************************************/
__attribute__((weak))
void PERIPHERAL_gpio_init(void)
{
}

__attribute__((weak))
bool PERIPHERAL_is_enable_usb(void)
{
    return true;
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

__attribute__((weak))
void PERIPHERAL_shell_init(void)
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

uint8_t BATT_mv_level(uint32_t mV)
{
#if ! defined(BATT_LOW_MV) || ! defined(BATT_EMPTY_MV)
    #pragma GCC error "no defined BATT_LOW_MV / BATT_EMPTY_MV for 10%");
    return (uint8_t)mV;
#else
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
#endif
}
