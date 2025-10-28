#include <ultracore/log.h>
#include <ultracore/timeo.h>

#include <stdlib.h>
#include <strings.h>

#include <sh/ucsh.h>
#include <sys/errno.h>
#include <flash.h>
#include <rtc.h>

#include "datetime_utils.h"
#include "voice.h"
#include "clock.h"

#include "PERIPHERAL_config.h"

#ifndef ALARM_TIMEOUT_MINUTES
    #define ALARM_TIMEOUT_MINUTES       (3)
#endif

#ifndef REMINDER_INTV
    #define REMINDER_INTV               (60000)
#endif

#ifndef REMINDER_TIMEOUT_MINUTES
    #define REMINDER_TIMEOUT_MINUTES    (15)
#endif

/****************************************************************************
 *  @def
 ****************************************************************************/
 #define COMPILE_YEAR                    (\
    (__DATE__[7] - '0') * 1000 +    \
    (__DATE__[8] - '0') *  100 +    \
    (__DATE__[9] - '0') *   10 +    \
    (__DATE__[10] - '0')   \
)
#define SUPPORTED_YEAR                  (2037)

#define NVM_ALARM                       NVM_DEFINE_KEY('A', 'L', 'R', 'M')
#define NVM_REMINDER                    NVM_DEFINE_KEY('R', 'E', 'M', 'D')
#define NVM_TIME_ZONE                   NVM_DEFINE_KEY('T', 'I', 'Z', 'E')
#define NVM_DST                         NVM_DEFINE_KEY('D', 'S', 'T', 'B')

#define NVM_ALARM_COUNT                 (FLASH_NVM_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))
#define NVM_REMINDER_COUNT              (FLASH_NVM_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))

struct DST_t
{
    bytebool_t en;
    int8_t dst_minute_offset;

    uint8_t tbl_count;
    struct
    {
        int start;      // YYYYMMDDHH
        int end;
    } tbl[20];
};
#define NVM_DST_COUNT                   sizeof(struct DST_t)

/****************************************************************************
 *  @public: runtime
 ****************************************************************************/
struct CLOCK_runtime_t clock_runtime;

/****************************************************************************
 *  @implements: weaks
 ****************************************************************************/
int get_dst_offset(struct tm *tm)
{
    struct DST_t *dst = NVM_get_ptr(NVM_DST);

    if (dst->en && 0 < dst->tbl_count)
    {
        int dt = (tm->tm_year + 1900) * (1000000) + (tm->tm_mon + 1) * 10000 + tm->tm_mday * 100 + tm->tm_hour;

        for (unsigned i = 0; i < dst->tbl_count; i ++)
        {
            if (dt >= dst->tbl[i].start && dt < dst->tbl[i].end)
            {
                if (dt == dst->tbl[i].start && 0 == tm->tm_sec)
                    return 0;
                else
                    return 60 * dst->dst_minute_offset;
            }
        }
    }
    return 0;
}

__attribute__((weak))
bool CLOCK_get_alarm_is_on(void)
{
    return true;
}

/****************************************************************************
 *  @internal
 ****************************************************************************/
static struct timeout_t reminder_next_timeo;
static void reminder_next_timeo_callback(void *arg);

static struct CLOCK_moment_t alarms[NVM_ALARM_COUNT];
static struct CLOCK_moment_t reminders[NVM_ALARM_COUNT];

// shell commands
static int SHELL_alarm(struct UCSH_env *env);
static int SHELL_reminder(struct UCSH_env *env);
static int SHELL_timezone(struct UCSH_env *env);
static int SHELL_dst(struct UCSH_env *env);

/****************************************************************************
 *  @implements
 ****************************************************************************/
void CLOCK_init()
{
    // CMU->CLKEN0_SET = CMU_CLKEN0_BURAM;
    if (1)
    {
        time_t now = time(NULL);
        struct tm dt;

        dt.tm_year = COMPILE_YEAR - 1900;
        dt.tm_mon = 0;
        dt.tm_mday = 1;
        dt.tm_hour = 0;
        dt.tm_min = 0;
        dt.tm_sec = 0;
        dt.tm_isdst = 0;
        time_t ts_min = mktime(&dt);

        dt.tm_year = SUPPORTED_YEAR + 1 - 1900;
        dt.tm_mon = 0;
        dt.tm_mday = 1;
        dt.tm_hour = 0;
        dt.tm_min = 0;
        dt.tm_sec = 0;

        if (now < ts_min)
            RTC_set_epoch_time(ts_min);

        now = time(NULL);
        localtime_r(&now, &dt);

        LOG_info("%04d/%02d/%02d %02d:%02d:%02d",
            dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday,
            dt.tm_hour, dt.tm_min, dt.tm_sec);
    }

    clock_runtime.alarming_idx = -1;
    timeout_init(&reminder_next_timeo, REMINDER_INTV, reminder_next_timeo_callback, 0);

    if (0 != NVM_get(NVM_ALARM, &alarms, sizeof(alarms)))
        memset(&alarms, 0, sizeof(alarms));

    if (0 != NVM_get(NVM_REMINDER, &reminders, sizeof(reminders)))
        memset(&reminders, 0, sizeof(reminders));

    // alarm
    UCSH_REGISTER("alm",        SHELL_alarm);
    UCSH_REGISTER("alarm",      SHELL_alarm);

    // reminder
    UCSH_REGISTER("rmd",        SHELL_reminder);
    UCSH_REGISTER("reminder",   SHELL_reminder);

    // dst
    UCSH_REGISTER("tz",         SHELL_timezone);
    UCSH_REGISTER("dst",        SHELL_dst);
}

void CLOCK_update_alarms(void)
{
    NVM_set(NVM_ALARM, &alarms, sizeof(alarms));
}

unsigned CLOCK_alarm_count(void)
{
    return lengthof(alarms);
}

static struct tm CLOCK_moment_to_dt(struct CLOCK_moment_t *moment)
{
    time_t ts;
    struct tm dt = {0};

    if (0 == moment->mdate)
    {
        dt.tm_year = moment->mdate / 10000;
        if (1900 <= dt.tm_year)
            dt.tm_year -= 1900;

        dt.tm_mon = moment->mdate % 10000 / 100 - 1;
        dt.tm_mday = moment->mdate % 100;

        dt.tm_hour = moment->mtime / 100;
        dt.tm_min = moment->mtime % 100;

        ts = mktime(&dt);
        localtime_r(&ts, &dt);
    }
    else
    {
        ts = time(NULL);
        localtime_r(&ts, &dt);

        dt.tm_hour = moment->mtime / 100;
        dt.tm_min = moment->mtime % 100;
        dt.tm_sec = 0;
    }

    return dt;
}

bool CLOCK_is_alarming(void)
{
    return -1 != clock_runtime.alarming_idx;
}

struct CLOCK_moment_t *CLOCK_get_alarm(uint8_t idx)
{
    return &alarms[idx];
}

int8_t CLOCK_peek_start_alarms(time_t ts)
{
    if (! CLOCK_get_alarm_is_on())
        return -1;
    if (ts <= clock_runtime.alarm_snooze_ts_end)
        return -1;

    struct tm dt;
    localtime_r(&ts, &dt);

    int16_t mtime = time_to_mtime(ts);
    struct CLOCK_moment_t *current_alarm = NULL;

    if (-1 != clock_runtime.alarming_idx)
    {
        current_alarm = &alarms[clock_runtime.alarming_idx];
        time_t ts_end = (mtime_to_time(current_alarm->mtime) + 60 * ALARM_TIMEOUT_MINUTES) % 86400;

        if (ts_end < time(NULL) % 86400)
        {
            current_alarm = NULL;
            clock_runtime.alarming_idx = -1;
        }
    }

    for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &alarms[idx];

        if (! alarm->enabled || alarm == current_alarm)
            continue;

        // matching week days mask or mdate
        if (0 == ((1 << dt.tm_wday) & alarm->wdays))
        {
            int32_t mdate = (((dt.tm_year + 1900) * 100 + dt.tm_mon + 1) * 100 + dt.tm_mday);

            if (mdate != alarm->mdate)
                continue;
        }

        if (mtime != alarm->mtime)
            continue;

        current_alarm = alarm;
        clock_runtime.alarming_idx = (int8_t)idx;
        break;
    }

    if (NULL != current_alarm)
    {
        VOICE_play_ringtone(current_alarm->ringtone_id);
        return (int8_t)(current_alarm - alarms);
    }
    else
        return -1;
}

int CLOCK_get_next_alarm_ringtone_id(void)
{
    int ringtone_id = 0;

    time_t ts = time(NULL);
    struct tm dt;
    localtime_r(&ts, &dt);

    for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &alarms[idx];
        if (alarm->enabled)
            ringtone_id = alarm->ringtone_id;

        if (0 != ((1 << ((dt.tm_wday + 1) % 7)) & alarm->wdays))
            return alarm->ringtone_id;
    }
    return ringtone_id;
}

bool CLOCK_stop_current_alarm(void)
{
    if (-1 != clock_runtime.alarming_idx)
    {
        clock_runtime.alarming_idx = -1;
        clock_runtime.alarm_snooze_ts_end = time(NULL) + 60;
        return true;
    }
    else
        return false;
}

void CLOCK_peek_start_reminders(time_t ts)
{
    if (! timeout_is_running(&reminder_next_timeo))
        CLOCK_say_reminders(ts, false);
}

static void reminder_next_timeo_callback(void *arg)
{
    ARG_UNUSED(arg);
    CLOCK_say_reminders(0, true);
}

unsigned CLOCK_say_reminders(time_t ts, bool ignore_snooze)
{
    unsigned reminder_count = 0;
    struct tm dt;
    localtime_r(&ts, &dt);

    time_t ts_base = ts - ts % 86400;
    int16_t mtime = time_to_mtime(ts);

    for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *reminder = &reminders[idx];

        if (! reminder->enabled)
            continue;

        time_t end_ts = ts_base + mtime_to_time(reminder->mtime) + 60 * REMINDER_TIMEOUT_MINUTES;

        // matching week days mask or mdate
        if (0 == ((1 << dt.tm_wday) & reminder->wdays))
        {
            int32_t mdate =
                (((dt.tm_year + 1900) * 100 + dt.tm_mon + 1) * 100 + dt.tm_mday);

            if (mdate != reminder->mdate)
                continue;
        }

        if (mtime >= reminder->mtime && ts < end_ts)
        {
            if (clock_runtime.reminder_ts_end < end_ts)
                clock_runtime.reminder_ts_end = end_ts;

            if (ignore_snooze ||
                clock_runtime.reminder_ts_end > clock_runtime.reminder_snooze_ts_end)
            {
                if (clock_runtime.reminder_ts_end > clock_runtime.reminder_snooze_ts_end)
                    reminder_count ++;

                VOICE_play_reminder(reminder->reminder_id);
            }
        }
    }

    if (0 == reminder_count)
        timeout_stop(&reminder_next_timeo);
    else
        timeout_start(&reminder_next_timeo, NULL);

    return reminder_count;
}

void CLOCK_snooze_reminders(void)
{
    timeout_stop(&reminder_next_timeo);
    clock_runtime.reminder_snooze_ts_end = clock_runtime.reminder_ts_end;
}

/***************************************************************************
 * @implements: utils
 ***************************************************************************/
static void CLOCK_dt_to_moment(struct CLOCK_moment_t *moment, struct tm const *dt)
{
    moment->mdate = ((dt->tm_year + 1900) * 100 + (dt->tm_mon + 1)) * 100 + dt->tm_mday;
}

void CLOCK_year_add(struct CLOCK_moment_t *moment, int value)
{
    struct tm dt = CLOCK_moment_to_dt(moment);

    if (0 != moment->mdate)
        TM_year_add(&dt, value);

    CLOCK_dt_to_moment(moment, &dt);
}

void CLOCK_month_add(struct CLOCK_moment_t *moment, int value)
{
    struct tm dt = CLOCK_moment_to_dt(moment);

    if (0 != moment->mdate)
        TM_month_add(&dt, value);

    CLOCK_dt_to_moment(moment, &dt);
}

void CLOCK_day_add(struct CLOCK_moment_t *moment, int value)
{
    struct tm dt = CLOCK_moment_to_dt(moment);

    if (0 != moment->mdate)
        TM_mday_add(&dt, value);

    CLOCK_dt_to_moment(moment, &dt);
}

void CLOCK_hour_add(struct CLOCK_moment_t *moment, int value)
{
    int hour = moment->mtime / 100 + value;
    while (0 > hour) hour += 24;
    hour %= 24;

    moment->mtime = (int16_t)(100 * hour + moment->mtime % 100);
}

void CLOCK_minute_add(struct CLOCK_moment_t *moment, int value)
{
    int minute = moment->mtime % 100 + value;
    while (0 > minute) minute += 60;
    minute %= 60;

    moment->mtime = (int16_t)((moment->mtime - moment->mtime % 100) +  minute);
}

/****************************************************************************
 *  @internal: shell commands
 ****************************************************************************/
static int SHELL_alarm(struct UCSH_env *env)
{
    if (3 == env->argc)         // alarm <1~COUNT> <enable/disable>
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(alarms))
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

        struct CLOCK_moment_t *alarm = &alarms[idx - 1];
        if (enabled != alarm->enabled || deleted)
        {
            alarm->enabled = enabled;

            if (deleted)
            {
                alarm->mdate = 0;
                alarm->wdays = 0;
            }
            NVM_set(NVM_ALARM, &alarms, sizeof(alarms));
        }

        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
    }
    else if (5 < env->argc)     // alarm <1~COUNT> <enable/disable> 1700 <0~COUNT> wdays=0x7f
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(alarms))
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
        ringtone = VOICE_select_ringtone(ringtone);

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

        struct CLOCK_moment_t *alarm = &alarms[idx - 1];
        if (enabled != alarm->enabled || mtime != alarm->mtime ||
            ringtone != alarm->ringtone_id ||
            mdate != alarm->mdate || wdays != alarm->wdays)
        {
            alarm->enabled = enabled;
            alarm->mtime = (int16_t)mtime;
            alarm->ringtone_id = (uint8_t)ringtone;
            alarm->mdate = mdate;
            alarm->wdays = (int8_t)wdays;
            NVM_set(NVM_ALARM, &alarms, sizeof(alarms));
        }

        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
    }

    UCSH_puts(env, "{\n\t\"alarms\": [\n");
    for (unsigned idx = 0, count = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &alarms[idx];

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

    UCSH_printf(env, "\t\"alarm_count\":%d,\n", lengthof(alarms));
    UCSH_printf(env, "\t\"alarm_ctrl\":\"%s\"\n}\n", CLOCK_get_alarm_is_on() ? "on" : "off");
    return 0;
}

static int SHELL_reminder(struct UCSH_env *env)
{
    if (3 == env->argc)         // rmd <1~COUNT> <enable/disable>
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(reminders))
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

        struct CLOCK_moment_t *reminder = &reminders[idx - 1];
        if (enabled != reminder->enabled || deleted)
        {
            reminder->enabled = enabled;

            if (deleted)
            {
                reminder->mdate = 0;
                reminder->wdays = 0;
            }
            NVM_set(NVM_REMINDER, &reminders, sizeof(reminders));
        }

        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
    }
    else if (5 < env->argc)     // rmd <1~COUNT> <enable/disable> 1700 <0~COUNT> wdays=0x7f
    {
        int idx = strtol(env->argv[1], NULL, 10);
        if (0 == idx || (unsigned)idx > lengthof(reminders))
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

        struct CLOCK_moment_t *reminder = &reminders[idx - 1];

        if (enabled != reminder->enabled || mtime != reminder->mtime ||
            reminder_id != reminder->reminder_id ||
            mdate != reminder->mdate || wdays != reminder->wdays)
        {
            reminder->enabled = enabled;
            reminder->mtime = (int16_t)mtime;
            reminder->reminder_id = (uint8_t)reminder_id;
            reminder->mdate = mdate;
            reminder->wdays = (int8_t)wdays;
            NVM_set(NVM_REMINDER, &reminders, sizeof(reminders));
        }

        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
    }

    UCSH_puts(env, "{\n\t\"reminders\": [\n");
    for (unsigned idx = 0, count = 0; idx < lengthof(reminders); idx ++)
    {
        struct CLOCK_moment_t *reminder = &reminders[idx];

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
    UCSH_printf(env, "\t\"reminder_count\": %d\n}\n", lengthof(reminders));

    return 0;
}

static int SHELL_timezone(struct UCSH_env *env)
{
    char *tz = malloc(FLASH_NVM_OBJECT_SIZE);

    if (2 == env->argc)
    {
        strncpy(tz, env->argv[1], FLASH_NVM_OBJECT_SIZE);
        NVM_set(NVM_TIME_ZONE, tz, FLASH_NVM_OBJECT_SIZE);
    }

    if (0 == NVM_get(NVM_TIME_ZONE, tz, FLASH_NVM_OBJECT_SIZE))
        UCSH_printf(env, "%s\n", tz);
    else
        UCSH_puts(env, "0");

    free(tz);
    return 0;
}

static int SHELL_dst(struct UCSH_env *env)
{
    struct DST_t *dst = malloc(sizeof(struct DST_t));
    int err = 0;

    if (1 == env->argc)
    {
        if (0 == NVM_get(NVM_DST, dst, sizeof(struct DST_t)))
        {
            UCSH_printf(env, "dst=%s\n", dst->en ? "ON" : "OFF");
            UCSH_printf(env, "\t%d minute\n", dst->dst_minute_offset);

            for (unsigned i = 0; i < dst->tbl_count; i ++)
                UCSH_printf(env, "\t%d~%d\n", dst->tbl[i].start, dst->tbl[i].end);
        }
        else
        {
            dst->en = false;
            UCSH_printf(env, "dst=%s\n", dst->en ? "ON" : "OFF");
        }
    }
    else if (2 == env->argc)
    {
        if (0 != strcasecmp(env->argv[1], "ON") && 0 != strcasecmp(env->argv[1], "OFF"))
            err = EINVAL;

        if (0 == err)
        {
            if (0 == NVM_get(NVM_DST, dst, sizeof(*dst)))
            {
                dst->en = 0 == strcasecmp(env->argv[1], "ON");

                if (0 == NVM_set(NVM_DST, dst, sizeof(*dst)))
                    VOICE_say_setting(VOICE_SETTING_DONE, NULL);
                else
                    dst->en = false;
            }
            else
                err = EIO;
        }
    }
    else
    {
        memset(dst, 0, sizeof(*dst));
        if (1)
        {
            int offset = strtol(env->argv[1], NULL, 10);
            if (60 < abs(offset))
                err = ERANGE;
            else
                dst->dst_minute_offset = (int8_t)offset;
        }

        if (0 == err)
        {
            for (int i = 2; i < env->argc; i ++)
            {
                char *p = env->argv[i];
                while (*p && '~' != *p) p ++;

                if ('~' != *p)
                {
                    err = EINVAL;
                    break;
                }
                else
                    *p ++ = '\0';

                int val = strtol(env->argv[i], NULL, 10);
                int hour = val % 100;
                if ((COMPILE_YEAR - 1) * (100 * 100 * 100) > val)
                {
                    err = EINVAL;
                    break;
                }
                if ((COMPILE_YEAR + 50) * (100 * 100 * 100) < val)
                {
                    err = EINVAL;
                    break;
                }
                if (24 < hour)
                {
                    err = EINVAL;
                    break;
                }
                dst->tbl[dst->tbl_count].start = val;

                val = strtol(p, NULL, 10);
                hour = val % 100;
                if ((COMPILE_YEAR - 1) * (100 * 100 * 100) > val)
                {
                    err = EINVAL;
                    break;
                }
                if ((COMPILE_YEAR + 50) * (100 * 100 * 100) < val)
                {
                    err = EINVAL;
                    break;
                }
                if (24 < hour)
                {
                    err = EINVAL;
                    break;
                }
                dst->tbl[dst->tbl_count].end = val;

                if (dst->tbl[dst->tbl_count].start > dst->tbl[dst->tbl_count].end)
                {
                    int tmp = dst->tbl[dst->tbl_count].start;
                    dst->tbl[dst->tbl_count].start = dst->tbl[dst->tbl_count].end;
                    dst->tbl[dst->tbl_count].end = tmp;
                }

                dst->tbl_count ++;
            }
        }

        if (0 == err)
        {
            dst->en = 0 != dst->dst_minute_offset && 0 != dst->tbl_count;

            if (0 == NVM_set(NVM_DST, dst, sizeof(*dst)))
                VOICE_say_setting(VOICE_SETTING_DONE, NULL);
        }
    }

    free(dst);
    return err;
}
