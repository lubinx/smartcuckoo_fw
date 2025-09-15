#include <ultracore/syscon.h>
#include <audio/renderer.h>
#include <hash/md5.h>

#include "smartcuckoo.h"
#include "ble.hpp"

#if defined(PANEL_B) || defined(PANEL_C)
    #include "panel_private.h"
    #define PANEL_APPLICATION
#endif

/*****************************************************************************/
/** @imports
*****************************************************************************/
extern "C" int SHELL_ota(struct UCSH_env *env);         // shell_ota.c

/*****************************************************************************/
/** @internal
*****************************************************************************/
static int SHELL_locale(struct UCSH_env *env);
static int SHELL_dfmt(struct UCSH_env *env);
static int SHELL_hfmt(struct UCSH_env *env);

/// @var
TUltraCorePeripheral BLE;

/*****************************************************************************/
/** @export
*****************************************************************************/
static void SHELL_register(void)
{
    // locale
    UCSH_REGISTER("loc",        SHELL_locale);
    // UCSH_REGISTER("locale",     SHELL_locale);
    // hour format
    UCSH_REGISTER("hfmt",       SHELL_hfmt);
    // date format: voice only
    UCSH_REGISTER("dfmt",       SHELL_dfmt);

    // upgrade
    UCSH_REGISTER("ota",        SHELL_ota);

    UCSH_REGISTER("batt",
        [](struct UCSH_env *env)
        {
            if (2 == env->argc && 0 == strcasecmp("mv", env->argv[1]))
                UCSH_printf(env, "batt=%u\n", PERIPHERAL_batt_volt());
            else
                UCSH_printf(env, "batt=%u\n", BATT_mv_level(PERIPHERAL_batt_volt()));
            return 0;
        });

    UCSH_REGISTER("rtcc",
        [](struct UCSH_env *env)
        {
            if (2 == env->argc)
            {
                int ppm = strtol(env->argv[1], NULL, 10);
                RTC_set_calibration_ppb(ppm * 1000);
            }

            UCSH_printf(env, "RTC calibration PPM: %d\n", (RTC_get_calibration_ppb() + 500)/ 1000);
            return 0;
        });

    UCSH_REGISTER("md5",
        [](struct UCSH_env *env)
        {
            if (2 != env->argc)
                return EINVAL;

            int fd = open(env->argv[1], O_RDONLY);
            if (-1 == fd)
                return errno;

            MD5_context_t *md5_ctx = (MD5_context_t *)&env->buf[64];
            MD5_init(md5_ctx);

            while (true)
            {
                ssize_t len = read(fd, env->buf, 64);

                if (len > 0)
                    MD5_update(md5_ctx, env->buf, (size_t)len);
                else
                    break;
            }
            MD5_t md5 = MD5_final(md5_ctx);
            close(fd);

            writeln(env->fd, env->buf, (size_t)MD5_sprintf(env->buf, &md5));
            return 0;
        });

    UCSH_REGISTER("vol",
        [](struct UCSH_env *env)
        {
            if (2 == env->argc)
            {
                int volume = strtol(env->argv[1], NULL, 10);
                if (0 > volume || 100 < volume)
                    return EINVAL;

                AUDIO_set_volume_percent((uint8_t)volume);
                setting.media_volume = (uint8_t)volume;
                NVM_set(NVM_SETTING, &setting, sizeof(setting));

                // VOICE_say_time_epoch(time(NULL), clock_runtime.dst_minute_offset);
                VOICE_say_setting(VOICE_SETTING_DONE, NULL);
            }
            UCSH_printf(env, "volume %u%%\n", AUDIO_renderer_get_volume_percent());
            return 0;
        }
    );

    UCSH_REGISTER("heap",
        [](struct UCSH_env *env)
        {
            UCSH_printf(env, "heap avail: %d\n", SYSCON_get_heap_unused());
            return 0;
        });

#ifdef DEBUG
    // mplayer
    UCSH_REGISTER("mplay",
        [](struct UCSH_env *env)
        {
            if (2 > env->argc)
                return EINVAL;
            else
                return mplayer_play(env->argv[1]);
        }
    );
    UCSH_REGISTER("mqueue",
        [](struct UCSH_env *env)
        {
            if (2 > env->argc)
                return EINVAL;
            else
                return mplayer_playlist_queue(env->argv[1]);
        }
    );
    UCSH_REGISTER("mstop",
        [](struct UCSH_env *env)
        {
            (void)env;
            return mplayer_stop();
        }
    );

    UCSH_REGISTER("mute",
        [](struct UCSH_env *env)
        {
            (void)env;
            return AUDIO_renderer_mute();
        }
    );
    UCSH_REGISTER("unmute",
        [](struct UCSH_env *env)
        {
            (void)env;
            return AUDIO_renderer_unmute();
        }
    );
#endif

    UCSH_REGISTER("noise",
        [](struct UCSH_env *env)
        {
            if (2 != env->argc)
                return EINVAL;

            if (0 == strcasecmp(env->argv[1], "start"))
                return MYNOISE_start();
            if (0 == strcasecmp(env->argv[1], "stop"))
                return MYNOISE_stop();

            if (0 == strcasecmp(env->argv[1], "next"))
                return MYNOISE_next();
            if (0 == strcasecmp(env->argv[1], "prev"))
                return MYNOISE_prev();

            if (0 == strcasecmp(env->argv[1], "pause"))
                return MYNOISE_pause();
            if (0 == strcasecmp(env->argv[1], "resume"))
                return MYNOISE_resume();

            return EINVAL;
        }
    );

    // REVIEW: peripheral extensions.
    PERIPHERAL_shell_init();
}

void SHELL_bootstrap(void)
{
    SHELL_register();

    #if 0 == PMU_EM2_EN
        __VOLATILE_DATA static struct UCSH_env UART_sh_env;
        __VOLATILE_DATA static uint32_t UART_sh_stack[1536 / sizeof(uint32_t)];

        UCSH_init_instance(&UART_sh_env, __stdout_fd, sizeof(UART_sh_stack), UART_sh_stack);
    #else
        LOG_printf("smartcuckoo %s startup, RTC calib: %d", PROJECT_ID, RTC_get_calibration_ppb());
    #endif

    LOG_info("heap avail: %d", SYSCON_get_heap_unused());
    BLE.Run();
}

void PERIPHERAL_adv_start(void)
{
    BLE.ADV_Start();
}

void PERIPHERAL_adv_stop(void)
{
    BLE.ADV_Stop();
}

void PERIPHERAL_adv_update(void)
{
    BLE.ADV_Update();
}

__attribute__((weak))
void PERIPHERAL_ota_init(void)
{
}

void PERIPHERAL_write_tlv(TMemStream &advs)
{
    if (1)
    {
        BluetoothTLV tlv(ATT_UNIT_UNITLESS, 0, (uint32_t)time(NULL));
        advs.Write(&tlv, tlv.size());
    }

    #ifdef PANEL_APPLICATION
    if (1)
    {
        BluetoothTLV tlv(ATT_UNIT_CELSIUS, 1, panel.env_sensor.tmpr);
        advs.Write(&tlv, tlv.size());
    }

    if (1)
    {
        BluetoothTLV tlv(ATT_UNIT_PERCENTAGE, 0, panel.env_sensor.humidity);
        advs.Write(&tlv, tlv.size());
    }
    #endif
}

/***************************************************************************/
/** UCSH @override
****************************************************************************/
void UCSH_startup_handle(struct UCSH_env *env)
{
    if (env->fd == __stdout_fd)
    {
        UCSH_printf(env, "smartcuckoo %s Shell ", PROJECT_ID);
        UCSH_version(env);
    }
}

int UCSH_version(struct UCSH_env *env)
{
    UCSH_printf(env, "v.%d.%d.%d\n",
        __MAJOR(PROJECT_VERSION), __MINOR(PROJECT_VERSION), __RELEASE(PROJECT_VERSION));
    return 0;
}

void UCSH_prompt_handle(struct UCSH_env *env)
{
    if (env->fd == __stdout_fd)
        UCSH_puts(env, "$ ");
    else
        UCSH_puts(env, "\n");
}

/*****************************************************************************/
/** @internal
*****************************************************************************/
static void voice_avail_locales_callback(int id, char const *lcid,
    enum LOCALE_dfmt_t dfmt, enum LOCALE_hfmt_t hfmt,  char const *voice, void *arg, bool final)
{
    UCSH_printf((struct UCSH_env *)arg, "\t{\"id\":%d, ", id);
    UCSH_printf((struct UCSH_env *)arg, "\"lcid\":\"%s\", ", lcid);
    UCSH_printf((struct UCSH_env *)arg, "\"dfmt\":\"%d\", ", dfmt);
    UCSH_printf((struct UCSH_env *)arg, "\"hfmt\":\"%d\", ", hfmt);
    UCSH_printf((struct UCSH_env *)arg, "\"voice\":\"%s\"}", voice);

    if (final)
        UCSH_puts((struct UCSH_env *)arg, "\n");
    else
        UCSH_puts((struct UCSH_env *)arg, ",\n");
}

static int SHELL_locale(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        if (0 != strcasecmp("get", env->argv[1]))
        {
            char *end_ptr = env->argv[1];
            int id = strtol(env->argv[1], &end_ptr, 10);
            int16_t old_voice_id = setting.sel_voice_id;

            if ('\0' == *end_ptr)
                setting.sel_voice_id = VOICE_select_voice((int16_t)id);
            else
                setting.sel_voice_id = VOICE_select_lcid(env->argv[1]);

            if (old_voice_id != setting.sel_voice_id)
            {
                NVM_set(NVM_SETTING, &setting, sizeof(setting));
                VOICE_say_setting(VOICE_SETTING_DONE, NULL);
            }
        }
        UCSH_printf(env, "voice_id=%d\n", setting.sel_voice_id);
    }
    else if (1 == env->argc)
    {
        UCSH_printf(env, "{\"voice_id\": %d,\n\"locales\": [\n", setting.sel_voice_id);
        VOICE_enum_avail_locales(voice_avail_locales_callback, env);
        UCSH_puts(env, "]}\n");
    }

    return 0;
}

static int SHELL_hfmt(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        unsigned fmt = strtoul(env->argv[1], NULL, 10);
        enum LOCALE_hfmt_t old_fmt = setting.locale.hfmt;

        switch (fmt)
        {
        default:
            return EINVAL;

        case HFMT_DEFAULT:
        case HFMT_12:
        case HFMT_24:
            setting.locale.hfmt = (LOCALE_hfmt_t)fmt;
            break;
        }

        if (old_fmt != fmt)
            NVM_set(NVM_SETTING, &setting, sizeof(setting));

        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
    }

    if (1)
    {
        enum LOCALE_hfmt_t fmt = setting.locale.hfmt;
        if (HFMT_DEFAULT == fmt)
            fmt = VOICE_get_default_hfmt();

        UCSH_printf(env, "hfmt=%d\n", fmt);
    }
    return 0;
}

static int SHELL_dfmt(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        unsigned fmt = strtoul(env->argv[1], NULL, 10);
        enum LOCALE_dfmt_t old_fmt = setting.locale.dfmt;

        switch (fmt)
        {
        default:
            return EINVAL;

        case DFMT_DEFAULT:
        case DFMT_DDMMYY:
        case DFMT_YYMMDD:
        case DFMT_MMDDYY:
            setting.locale.dfmt = (enum LOCALE_dfmt_t)fmt;
            break;
        }

        if (old_fmt != fmt)
            NVM_set(NVM_SETTING, &setting, sizeof(setting));

        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
    }

    if (1)
    {
        enum LOCALE_dfmt_t fmt = setting.locale.dfmt;
        if (DFMT_DEFAULT == fmt)
            fmt =  VOICE_get_default_dfmt();

        switch (fmt)
        {
        case DFMT_DEFAULT:
            break;

        case DFMT_DDMMYY:
            UCSH_printf(env, "dfmt=(%u)DDMMYY\n", DFMT_DDMMYY);
            break;
        case DFMT_YYMMDD:
            UCSH_printf(env, "dfmt=(%u)YYMMDD\n", DFMT_YYMMDD);
            break;
        case DFMT_MMDDYY:
            UCSH_printf(env, "dfmt=(%u)MMDDYY\n", DFMT_MMDDYY);
            break;
        }
    }
    return 0;
}
