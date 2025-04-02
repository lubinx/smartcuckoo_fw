#include <ultracore/syscon.h>
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

static int SHELL_alarm(struct UCSH_env *env);
static int SHELL_reminder(struct UCSH_env *env);

/// @var
TUltraCorePeripheral BLE;

// #if 0 == PMU_EM2_EN
// #else
//     static struct UCSH_env BLE_sh_env;
//     __THREAD_STACK static uint32_t BLE_sh_stack[1536 / sizeof(uint32_t)];
// #endif

/*****************************************************************************/
/** @export
*****************************************************************************/
static void SHELL_register(void)
{
    // locale
    UCSH_register("loc",        SHELL_locale);
    UCSH_register("locale",     SHELL_locale);
    // hour format
    UCSH_register("hfmt",       SHELL_hfmt);
    // date format: voice only
    UCSH_register("dfmt",       SHELL_dfmt);

    // alarm
    UCSH_register("alm",        SHELL_alarm);
    UCSH_register("alarm",      SHELL_alarm);
    // reminder
    UCSH_register("rmd",        SHELL_reminder);
    UCSH_register("reminder",   SHELL_reminder);

    // upgrade
    UCSH_register("ota",        SHELL_ota);

    UCSH_register("batt",
        [](struct UCSH_env *env)
        {
            if (2 == env->argc && 0 == strcasecmp("mv", env->argv[1]))
                UCSH_printf(env, "batt=%u\n", PERIPHERAL_batt_volt());
            else
                UCSH_printf(env, "batt=%u\n", BATT_mv_level(PERIPHERAL_batt_volt()));
            return 0;
        });

    UCSH_register("heap",
        [](struct UCSH_env *env)
        {
            UCSH_printf(env, "heap avail: %d\n", SYSCON_get_heap_avail());
            return 0;
        });

    UCSH_register("rtcc",
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

    UCSH_register("md5",
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

    // mplayer
    UCSH_register("mplay",
        [](struct UCSH_env *env)
        {
            if (2 > env->argc)
                return EINVAL;
            else
                return mplayer_play(env->argv[1]);
        }
    );
    UCSH_register("mqueue",
        [](struct UCSH_env *env)
        {
            if (2 > env->argc)
                return EINVAL;
            else
                return mplayer_playlist_queue(env->argv[1]);
        }
    );
    UCSH_register("mstop",
        [](struct UCSH_env *env)
        {
            (void)env;
            return mplayer_stop();
        }
    );

    UCSH_register("mvol",
        [](struct UCSH_env *env)
        {
            if (1 == env->argc)
            {
                uint8_t vol = mplayer_get_volume();

                UCSH_printf(env, "volume %u%%\n", vol);
                return 0;

            }
            else if (2 == env->argc)
            {
                int volume = strtol(env->argv[1], NULL, 10);
                if (0 > volume)
                    return EINVAL;
                else
                    return mplayer_set_volume((uint8_t)volume);
            }
            else
                return EINVAL;
        }
    );

    UCSH_register("mute",
        [](struct UCSH_env *env)
        {
            (void)env;
            return mplayer_mute();
        }
    );
    UCSH_register("unmute",
        [](struct UCSH_env *env)
        {
            (void)env;
            return mplayer_unmute();
        }
    );

    // REVIEW: peripheral extensions.
    PERIPHERAL_shell_init();
}

void SHELL_bootstrap(void)
{
    SHELL_register();

    // uart shell
    #if 0 == PMU_EM2_EN
        __EM3_VOLATILE static struct UCSH_env UART_sh_env;
        __EM3_VOLATILE static uint32_t UART_sh_stack[1536 / sizeof(uint32_t)];

        UCSH_init_instance(&UART_sh_env, __stdout_fd, sizeof(UART_sh_stack), UART_sh_stack);
    #else
        LOG_printf("smartcuckoo %s startup, RTC calib: %d", PROJECT_ID, RTC_get_calibration_ppb());
    #endif

    // UCSH_init_instance(&BLE_sh_env, BLE.Shell.CreateVFd(), sizeof(BLE_sh_stack), BLE_sh_stack);

    LOG_info("heap avail: %d", SYSCON_get_heap_avail());
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
#if 0 == PMU_EM2_EN
    if (__stdout_fd == env->fd)
        UCSH_puts(env, "$ ");
#else
    (void)env;
#endif
}

/*****************************************************************************/
/** @internal
*****************************************************************************/
static void voice_avail_locales_callback(int id, char const *lcid, char const *voice, void *arg, bool final)
{
    UCSH_printf((struct UCSH_env *)arg, "\t{\"id\":%d, ", id);
    UCSH_printf((struct UCSH_env *)arg, "\"lcid\":\"%s\", ", lcid);
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
        char *end_ptr = env->argv[1];
        int id = strtol(env->argv[1], &end_ptr, 10);
        int16_t old_voice_id = setting.sel_voice_id;

        if ('\0' == *end_ptr)
            setting.sel_voice_id = VOICE_select_voice(&voice_attr, (int16_t)id);
        else
            setting.sel_voice_id = VOICE_select_lcid(&voice_attr, env->argv[1]);

        if (old_voice_id != setting.sel_voice_id)
            NVM_set(NVM_SETTING, &setting, sizeof(setting));

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }

    UCSH_printf(env, "{\"voice_id\": %d,\n\"locales\": [\n", setting.sel_voice_id);
    VOICE_enum_avail_locales(voice_avail_locales_callback, env);
    UCSH_puts(env, "]}\n");

    return 0;
}

static int SHELL_alarm(struct UCSH_env *env)
{
    if (3 == env->argc)         // alarm <1~COUNT> <enable/disable>
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(clock_setting.alarms))
            return EINVAL;

        bool enabled = true;
        bool deleted = false;

        if (0 == strcasecmp("disable", env->argv[2]))
            enabled = false;
        else if (0 == strcasecmp("enable", env->argv[2]))
            enabled = true;
        else if (0 == strcasecmp("delete", env->argv[2]))
            (deleted = true, enabled = false);
        else
            return EINVAL;

        struct CLOCK_moment_t *alarm = &clock_setting.alarms[idx - 1];
        if (enabled != alarm->enabled || deleted)
        {
            alarm->enabled = enabled;

            if (deleted)
            {
                alarm->mdate = 0;
                alarm->wdays = 0;
            }
            NVM_set(NVM_ALARM, &clock_setting.alarms, sizeof(clock_setting.alarms));
        }

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }
    else if (5 < env->argc)     // alarm <1~COUNT> <enable/disable> 1700 <0~COUNT> wdays=0x7f
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(clock_setting.alarms))
            return EINVAL;

        bool enabled;

        if (0 == strcasecmp("disable", env->argv[2]))
            enabled = false;
        else if (0 == strcasecmp("enable", env->argv[2]))
            enabled = true;
        else
            return EINVAL;

        int mtime = strtol(env->argv[3], NULL, 10);
        if (60 <= mtime % 100 || 24 <= mtime / 100)     // 0000 ~ 2359
            return EINVAL;

        int ringtone = strtol(env->argv[4], NULL, 10);
        ringtone = VOICE_select_ringtone(&voice_attr, ringtone);

        int wdays = 0;
        if (true)
        {
            char *wday_str = CMD_paramvalue_byname("wdays", env->argc, env->argv);
            if (wday_str)
            {
                wdays = strtol(wday_str, NULL, 10);
                if (0 == wdays)
                    wdays = strtol(wday_str, NULL, 16);
                if (0 == wdays)
                    return EINVAL;
            }
        }
        else
            wdays = 0;

        int mdate = 0; // format integer: yyyymmdd
        if (true)
        {
            char *mdate_str = CMD_paramvalue_byname("mdate", env->argc, env->argv);
            if (mdate_str)      // soo.. mdate can set anything, except it will never alarm
                mdate = strtol(mdate_str, NULL, 10);
        }

        // least one of alarm date or week days masks must set
        if (0 == mdate && 0 == wdays)
            return EINVAL;

        struct CLOCK_moment_t *alarm = &clock_setting.alarms[idx - 1];
        if (enabled != alarm->enabled || mtime != alarm->mtime ||
            ringtone != alarm->ringtone_id ||
            mdate != alarm->mdate || wdays != alarm->wdays)
        {
            alarm->enabled = enabled;
            alarm->mtime = (int16_t)mtime;
            alarm->ringtone_id = (uint8_t)ringtone;
            alarm->mdate = mdate;
            alarm->wdays = (int8_t)wdays;
            NVM_set(NVM_ALARM, &clock_setting.alarms, sizeof(clock_setting.alarms));
        }

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }

    UCSH_puts(env, "{\n\t\"alarms\": [\n");
    for (unsigned idx = 0, count = 0; idx < lengthof(clock_setting.alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &clock_setting.alarms[idx];

        // deleted condition
        if (! alarm->enabled && 0 == alarm->wdays && 0 == alarm->mdate)
            continue;
        else if (0 != count ++)
            UCSH_puts(env, ",\n");

        UCSH_printf(env, "\t\t{\"id\":%d, ", idx + 1);
        UCSH_printf(env, "\"enabled\":%s, ", alarm->enabled ? "true" : "false");
        UCSH_printf(env, "\"mtime\":%d, ",  alarm->mtime);
        UCSH_printf(env, "\"ringtone_id\":%d, ", alarm->ringtone_id);
        UCSH_printf(env, "\"mdate\":%lu, ",  alarm->mdate);
        UCSH_printf(env, "\"wdays\":%d}", alarm->wdays);
    }
    UCSH_puts(env, "\t],\n");

    UCSH_printf(env, "\t\"alarm_count\":%d,\n", lengthof(clock_setting.alarms));
    UCSH_printf(env, "\t\"alarm_ctrl\":\"%s\"\n}\n", CLOCK_alarm_switch_is_on() ? "on" : "off");
    return 0;
}

static int SHELL_reminder(struct UCSH_env *env)
{
    if (3 == env->argc)         // rmd <1~COUNT> <enable/disable>
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(clock_setting.reminders))
            return EINVAL;

        bool enabled = true;
        bool deleted = false;

        if (0 == strcasecmp("disable", env->argv[2]))
            enabled = false;
        else if (0 == strcasecmp("enable", env->argv[2]))
            enabled = true;
        else if (0 == strcasecmp("delete", env->argv[2]))
            (deleted = true, enabled = false);
        else
            return EINVAL;

        struct CLOCK_moment_t *reminder = &clock_setting.reminders[idx - 1];
        if (enabled != reminder->enabled || deleted)
        {
            reminder->enabled = enabled;

            if (deleted)
            {
                reminder->mdate = 0;
                reminder->wdays = 0;
            }
            NVM_set(NVM_REMINDER, &clock_setting.reminders, sizeof(clock_setting.reminders));
        }

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }
    else if (5 < env->argc)     // rmd <1~COUNT> <enable/disable> 1700 <0~COUNT> wdays=0x7f
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(clock_setting.reminders))
            return EINVAL;

        bool enabled = true;
        if (0 == strcasecmp("disable", env->argv[2]))
            enabled = false;
        else if (0 == strcasecmp("enable", env->argv[2]))
            enabled = true;
        else
            return EINVAL;

        int mtime = strtol(env->argv[3], NULL, 10);
        if (60 <= mtime % 100 || 24 <= mtime / 100)     // 0000 ~ 2359
            return EINVAL;

        int reminder_id = strtol(env->argv[4], NULL, 10);
        int wdays = 0;
        if (true)
        {
            char *wday_str = CMD_paramvalue_byname("wdays", env->argc, env->argv);
            if (wday_str)
            {
                wdays = strtol(wday_str, NULL, 10);
                if (0 == wdays)
                    wdays = strtol(wday_str, NULL, 16);
                if (0 == wdays)
                    return EINVAL;
            }
        }

        int32_t mdate = 0; // format integer: yyyymmdd
        if (true)
        {
            char *mdate_str = CMD_paramvalue_byname("mdate", env->argc, env->argv);
            if (mdate_str)      // soo.. mdate can set anything, except it will never alarm
                mdate = strtol(mdate_str, NULL, 10);
        }

        // least one of alarm date or week days masks must set
        if (0 == mdate && 0 == wdays)
            return EINVAL;

        struct CLOCK_moment_t *reminder = &clock_setting.reminders[idx - 1];

        if (enabled != reminder->enabled || mtime != reminder->mtime ||
            reminder_id != reminder->reminder_id ||
            mdate != reminder->mdate || wdays != reminder->wdays)
        {
            reminder->enabled = enabled;
            reminder->mtime = (int16_t)mtime;
            reminder->reminder_id = (uint8_t)reminder_id;
            reminder->mdate = mdate;
            reminder->wdays = (int8_t)wdays;
            NVM_set(NVM_REMINDER, &clock_setting.reminders, sizeof(clock_setting.reminders));
        }

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }

    UCSH_puts(env, "{\n\t\"reminders\": [\n");
    for (unsigned idx = 0, count = 0; idx < lengthof(clock_setting.reminders); idx ++)
    {
        struct CLOCK_moment_t *reminder = &clock_setting.reminders[idx];

        // deleted condition
        if (! reminder->enabled && 0 == reminder->wdays && 0 == reminder->mdate)
            continue;
        else if (0 != count ++)
            UCSH_puts(env, ",\n");

        UCSH_printf(env, "\t{\"id\":%d, ", idx + 1);
        UCSH_printf(env, "\"enabled\":%s, ", reminder->enabled ? "true" : "false");
        UCSH_printf(env, "\"mtime\":%d, ",  reminder->mtime);
        UCSH_printf(env, "\"reminder_id\":%d, ", reminder->reminder_id);
        UCSH_printf(env, "\"mdate\":%lu, ",  reminder->mdate);
        UCSH_printf(env, "\"wdays\":%d}", reminder->wdays);
    }
    UCSH_puts(env, "\t],\n");
    UCSH_printf(env, "\t\"reminder_count\": %d\n}\n", lengthof(clock_setting.reminders));

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

        case 0:
            setting.locale.hfmt = HFMT_DEFAULT;
            break;
        case 12:
            setting.locale.hfmt = HFMT_12;
            break;
        case 24:
            setting.locale.hfmt = HFMT_24;
            break;
        }

        if (old_fmt != fmt)
            NVM_set(NVM_SETTING, &setting, sizeof(setting));

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }

    UCSH_printf(env, "hfmt=%d\n", voice_attr.locale->hfmt);
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
            setting.locale.dfmt = DFMT_DEFAULT;
            break;
        case DFMT_DDMMYY:
            setting.locale.dfmt  = DFMT_DDMMYY;
            break;
        case DFMT_YYMMDD:
            setting.locale.dfmt  = DFMT_YYMMDD;
            break;
        case DFMT_MMDDYY:
            setting.locale.dfmt  = DFMT_MMDDYY;
            break;
        }

        if (old_fmt != fmt)
            NVM_set(NVM_SETTING, &setting, sizeof(setting));

        VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }

    switch (voice_attr.locale->dfmt)
    {
    case DFMT_DDMMYY:
        UCSH_printf(env, "dfmt=(%u)DDMMYY\n", DFMT_DDMMYY);
        break;
    case DFMT_YYMMDD:
        UCSH_printf(env, "dfmt=(%u)YYMMDD\n", DFMT_YYMMDD);
        break;
    case DFMT_MMDDYY:
        UCSH_printf(env, "dfmt=(%u)MMDDYY\n", DFMT_MMDDYY);
        break;

    case DFMT_DEFAULT:
        // no possiable value
        break;
    }
    return 0;
}
