#include <ultracore/log.h>
#include <ultracore/nvm.h>
#include <ultracore/timeo.h>

#include <stdlib.h>
#include <strings.h>

#include <sh/ucsh.h>
#include <sys/errno.h>
#include <rtc.h>
#include <audio/mplayer.h>

#include "datetime_utils.h"
#include "voice.h"
#include "clock.h"

#include "PERIPHERAL_config.h"

#define CLOCK_DEF_RING_SECONDS          (180)
#define CLOCK_DEF_SNOOZE_SECONDS        (600)

#define CLOCK_DEF_REMINDER_SECONDS      (1200)
#define CLOCK_DEF_REMINDER_INTV_SECONDS (60)

/****************************************************************************
 *  @def
 ****************************************************************************/
 #define COMPILE_YEAR                    (\
    (__DATE__[7] - '0') * 1000 +    \
    (__DATE__[8] - '0') *  100 +    \
    (__DATE__[9] - '0') *   10 +    \
    (__DATE__[10] - '0')   \
)

#define CLOCK_NVM_ID                    NVM_DEFINE_KEY('C', 'S', 'E', 'T')
#define CLOCK_ALARM_NVM_ID              NVM_DEFINE_KEY('C', 'A', 'L', 'M')
#define CLOCK_REMINDER_NVM_ID           NVM_DEFINE_KEY('C', 'R', 'M', 'D')

#define ALARM_COUNT                     (NVM_MAX_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))
#define REMINDER_COUNT                  (NVM_MAX_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))

struct DST_t
{
    bytebool_t en;
    int8_t minute_offset;

    uint8_t tbl_count;
    struct
    {
        int start;      // YYYYMMDDHH
        int end;
    } tbl[20];
};

struct CLOCK_nvm_t
{
    uint16_t ring_seconds;
    uint16_t ring_snooze_seconds;    // TODO: snooze

    uint16_t reminder_seconds;
    uint16_t reminder_intv_seconds;

    int16_t timezone_offset;
    struct DST_t dst;

    uint32_t say_zero_hour_mask     : 24;
    uint32_t say_zero_hour_wdays    : 8;
};
static_assert(sizeof(struct CLOCK_nvm_t) <= NVM_MAX_OBJECT_SIZE, "");


struct CLOCK_runtime_t
{
    time_t alarm_snooze_end_ts;
    time_t reminder_slient_end_ts;

    int8_t alarming_idx;
};

/****************************************************************************
 *  @internal
 ****************************************************************************/
static struct timeout_t next_timeo;
static void next_timeo_callback(void *arg);

static struct CLOCK_runtime_t clock_runtime = {0};
static struct CLOCK_nvm_t const *nvm_ptr;

static struct CLOCK_moment_t alarms[ALARM_COUNT];
static struct CLOCK_moment_t reminders[ALARM_COUNT];

// shell commands
static int SHELL_clock(struct UCSH_env *env);       // REVIEW: misc clock settings
static int SHELL_alarm(struct UCSH_env *env);
static int SHELL_reminder(struct UCSH_env *env);

/****************************************************************************
 *  @implements: override time.c
 ****************************************************************************/
int get_timezone_offset(void)
{
    if (NULL == nvm_ptr)
        return 0;
    else
        return nvm_ptr->timezone_offset;
}

int get_dst_offset(struct tm *tm)
{
    if (NULL == nvm_ptr)
        return 0;

    if (nvm_ptr->dst.en && 0 != nvm_ptr->dst.minute_offset && 0 < nvm_ptr->dst.tbl_count)
    {
        int dt = (tm->tm_year + 1900) * (1000000) + (tm->tm_mon + 1) * 10000 + tm->tm_mday * 100 + tm->tm_hour;

        for (unsigned i = 0; i < nvm_ptr->dst.tbl_count; i ++)
        {
            if (dt >= nvm_ptr->dst.tbl[i].start && dt < nvm_ptr->dst.tbl[i].end)
                return 60 * nvm_ptr->dst.minute_offset;
        }
    }
    return 0;
}

/****************************************************************************
 *  @implements
 ****************************************************************************/
void CLOCK_init()
{
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

        if (now < ts_min)
            RTC_set_epoch_time(ts_min);

        now = time(NULL);
        localtime_r(&now, &dt);

        LOG_info("%04d/%02d/%02d %02d:%02d:%02d",
            dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday,
            dt.tm_hour, dt.tm_min, dt.tm_sec);
    }

    clock_runtime.alarming_idx = -1;
    timeout_init(&next_timeo, 1000 * nvm_ptr->reminder_intv_seconds, next_timeo_callback, 0);

    if (0 != NVM_get(CLOCK_ALARM_NVM_ID, sizeof(alarms), &alarms))
        memset(&alarms, 0, sizeof(alarms));

    if (0 != NVM_get(CLOCK_REMINDER_NVM_ID, sizeof(reminders), &reminders))
        memset(&reminders, 0, sizeof(reminders));

    if (NULL == (nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr))))
    {
        struct CLOCK_nvm_t *nvm = malloc(sizeof(struct CLOCK_nvm_t));

        if (NULL != nvm)
        {
            memset(nvm, 0, sizeof(*nvm));

            nvm->ring_seconds = CLOCK_DEF_RING_SECONDS;
            nvm->ring_snooze_seconds = CLOCK_DEF_SNOOZE_SECONDS;
            nvm->reminder_seconds = CLOCK_DEF_REMINDER_SECONDS;
            nvm->reminder_intv_seconds = CLOCK_DEF_REMINDER_INTV_SECONDS;

            NVM_set(CLOCK_NVM_ID, sizeof(*nvm), nvm);
            free(nvm);
        }
        nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));
    }

    UCSH_REGISTER("clock",      SHELL_clock);

    UCSH_REGISTER("alm",        SHELL_alarm);
    UCSH_REGISTER("rmd",        SHELL_reminder);
}

static int8_t CLOCK_peek_start_alarms(time_t ts)
{
    if (! CLOCK_alarm_switch_is_on())
        return -1;
    if (ts <= clock_runtime.alarm_snooze_end_ts)
        return -1;

    struct tm dt;
    localtime_r(&ts, &dt);

    int16_t mtime = time_to_mtime(ts);
    struct CLOCK_moment_t *current_alarm = NULL;

    if (-1 != clock_runtime.alarming_idx)
    {
        current_alarm = &alarms[clock_runtime.alarming_idx];
        time_t ts_end = (mtime_to_time(current_alarm->mtime) + nvm_ptr->ring_seconds) % 86400;

        if (ts_end < time(NULL) % 86400)
        {
            current_alarm = NULL;
            clock_runtime.alarming_idx = -1;
        }
    }

    for (int8_t idx = 0; idx < (int8_t)lengthof(alarms); idx ++)
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

void next_timeo_callback(void *arg)
{
    if (NULL == arg)
    {
        time_t ts = time(NULL);
        VOICE_say_time_epoch(ts);
    }

    if (0 == CLOCK_say_reminders(0, true))
        timeout_stop(&next_timeo);
    else
        timeout_start(&next_timeo, NULL);
}

void CLOCK_schedule(time_t ts)
{
    if (-1 != CLOCK_peek_start_alarms(ts))
    {
        timeout_stop(&next_timeo);
        return;
    }

    if (0 != nvm_ptr->say_zero_hour_mask)
    {
        unsigned next_zero_hour = (unsigned)((ts % 86400) / 3600 + 1) % 24;
        int next_zero_intv = -1;

        if ((1U << next_zero_hour) & nvm_ptr->say_zero_hour_mask)
        {
            if (0 == nvm_ptr->say_zero_hour_wdays || 0x7F == nvm_ptr->say_zero_hour_wdays)
            {
            set_next_intv:
                next_zero_intv = 1000 * (3600 - (int)(ts % 3600));
            }
            else
            {
                struct tm const *tm = localtime(&ts);
                if ((1U << tm->tm_wday) & nvm_ptr->say_zero_hour_wdays)
                    goto set_next_intv;
            }

            if (0 <= next_zero_intv && 1000 * nvm_ptr->reminder_intv_seconds > next_zero_intv)
            {
                timeout_update(&next_timeo, (unsigned)next_zero_intv);
                timeout_start(&next_timeo, NULL);
            }
        }
    }

    if (! timeout_is_running(&next_timeo))
    {
        if(0 != CLOCK_say_reminders(ts, false))
        {
            timeout_update(&next_timeo, 1000 * nvm_ptr->reminder_intv_seconds);
            timeout_start(&next_timeo, reminders);
        }
    }
}

__attribute__((weak))
bool CLOCK_alarm_switch_is_on(void)
{
    return true;
}

uint16_t CLOCK_get_ring_snooze_seconds(void)
{
    if (NULL != nvm_ptr)
        return nvm_ptr->ring_snooze_seconds;
    else
        return 0;
}

int CLOCK_set_ring_snooze_seconds(uint16_t seconds)
{
    if (nvm_ptr->ring_snooze_seconds != seconds)
    {
        struct CLOCK_nvm_t *nvm = malloc(sizeof(struct CLOCK_nvm_t));
        if (NULL == nvm)
            return ENOMEM;

        NVM_get(CLOCK_NVM_ID, sizeof(*nvm), nvm);
        nvm->ring_snooze_seconds = seconds;
        NVM_set(CLOCK_NVM_ID, sizeof(*nvm), nvm);
        free(nvm);

        nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));
    }
    return 0;
}

unsigned CLOCK_alarm_count(void)
{
    return lengthof(alarms);
}

void CLOCK_update_alarms(void)
{
    NVM_set(CLOCK_ALARM_NVM_ID, sizeof(alarms), &alarms);
}

int8_t CLOCK_get_alarming_idx(void)
{
    return clock_runtime.alarming_idx;
}

int CLOCK_get_ringtone_id(void)
{
    int ringtone_id = -1;

    time_t ts = time(NULL);
    struct tm dt;
    localtime_r(&ts, &dt);

    for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &alarms[idx];
        if (alarm->enabled || -1 == ringtone_id)
            ringtone_id = alarm->ringtone_id;

        if (0 != ((1 << ((dt.tm_wday + 1) % 7)) & alarm->wdays))
        {
            ringtone_id = alarm->ringtone_id;
            break;
        }
    }
    return ringtone_id;
}

struct CLOCK_moment_t *CLOCK_get_alarm(uint8_t idx)
{
    return &alarms[idx];
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

static bool CLOCK_canceling(bool snooze)
{
    // TODO: clock snooze
    (void)snooze;

    if (-1 != clock_runtime.alarming_idx)
    {
        clock_runtime.alarming_idx = -1;
        clock_runtime.alarm_snooze_end_ts = time(NULL) + 60;

        if (! mplayer_is_idle()) mplayer_stop();
        return true;
    }
    else
    {
        // stop activity reminders
        if (timeout_is_running(&next_timeo))
        {
            timeout_stop(&next_timeo);
            clock_runtime.reminder_slient_end_ts = time(NULL) + nvm_ptr->reminder_seconds;
        }

        return false;
    }
}

bool CLOCK_dismiss(void)
{
    return CLOCK_canceling(false);
}

bool CLOCK_snooze(void)
{
    return CLOCK_canceling(true);
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

        time_t reminder_end_ts = ts_base + mtime_to_time(reminder->mtime) + nvm_ptr->reminder_seconds;

        // matching week days mask or mdate
        if (0 == ((1 << dt.tm_wday) & reminder->wdays))
        {
            int32_t mdate =
                (((dt.tm_year + 1900) * 100 + dt.tm_mon + 1) * 100 + dt.tm_mday);

            if (mdate != reminder->mdate)
                continue;
        }

        if (mtime >= reminder->mtime && ts < reminder_end_ts)
        {
            if (ignore_snooze || reminder_end_ts > clock_runtime.reminder_slient_end_ts)
            {
                if (reminder_end_ts > clock_runtime.reminder_slient_end_ts)
                    reminder_count ++;

                VOICE_play_reminder(reminder->reminder_id);
            }
        }
    }
    return reminder_count;
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
static int SHELL_clock(struct UCSH_env *env)
{
    if (NULL == nvm_ptr)
        return EHAL_NOT_CONFIGURED;

    if (1 == env->argc)
    {
        char *buf = env->buf + 32;
        int flush_bytes = MIN(200, (env->bufsize - 32) / 2);
        int pos = 0;

        pos += sprintf(&buf[pos], "{");
        if (1)  // ring & reminder
        {
            pos += sprintf(&buf[pos], "\n\t\"ring\": %d", nvm_ptr->ring_seconds);
            pos += sprintf(&buf[pos], ",\n\t\"snooze\": %d", nvm_ptr->ring_snooze_seconds);
            pos += sprintf(&buf[pos], ",\n\t\"reminder\": %d", nvm_ptr->reminder_seconds);
            pos += sprintf(&buf[pos], ",\n\t\"reminder_intv\": %d", nvm_ptr->reminder_intv_seconds);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (1)  // zero hour voide
        {
            pos += sprintf(&buf[pos], ",\n\t\"zero_hour_voice\":\n\t{");
            pos += sprintf(&buf[pos], "\n\t\t\"mask\": %d", nvm_ptr->say_zero_hour_mask);
            pos += sprintf(&buf[pos], ",\n\t\t\"wdays\": %d", nvm_ptr->say_zero_hour_wdays);

            pos += sprintf(&buf[pos], "\n\t}");
            if (flush_bytes < pos)
            {
                writebuf(env->fd, buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (1)  // timezone & dst
        {
            pos += sprintf(&buf[pos], ",\n\t\"timezone_offset\": %d", nvm_ptr->timezone_offset);

            pos += sprintf(&buf[pos], ",\n\t\"dst\":\n\t{");
            pos += sprintf(&buf[pos], "\n\t\t\"en\": %s", nvm_ptr->dst.en ? "true" : "false");
            pos += sprintf(&buf[pos], ",\n\t\t\"minute_offset\": %d", nvm_ptr->dst.minute_offset);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, buf, (unsigned)pos);
                pos = 0;
            }

            pos += sprintf(&buf[pos], ",\n\t\t\"ranges\": [");
            for (unsigned i = 0; i < nvm_ptr->dst.tbl_count; i ++)
            {
                if (0 == i)
                    pos += sprintf(&buf[pos], "\n\t\t\t");
                else
                    pos += sprintf(&buf[pos], ",\n\t\t\t");

                pos += sprintf(&buf[pos], "{\"start\": %d, \"end\": %d}", nvm_ptr->dst.tbl[i].start, nvm_ptr->dst.tbl[i].end);

                if (flush_bytes < pos)
                {
                    writebuf(env->fd, buf, (unsigned)pos);
                    pos = 0;
                }
            }

            pos += sprintf(&buf[pos], "\n\t\t]\n\t}");
        }

        pos += sprintf(&buf[pos], "\n}\n");
        writebuf(env->fd, buf, (unsigned)pos);
        return 0;
    };

    int err = 0;
    struct CLOCK_nvm_t *nvm = malloc(sizeof(struct CLOCK_nvm_t));
    if (NULL == nvm)
        return ENOMEM;
    else
        NVM_get(CLOCK_NVM_ID, sizeof(*nvm), nvm);

// REVIEW: clock ring & snooze seconds
    if (0 == strcmp("ring", env->argv[1]))
    {
        if (3 != env->argc)
            err = EINVAL;

        int seconds = strtol(env->argv[2], NULL, 10);
        if (0 > seconds || 3600 < seconds)
            err = EINVAL;

        if (0 == err)
            nvm->ring_seconds = (uint16_t)seconds;
    }
    else if (0 == strcmp("snooze", env->argv[1]))
    {
        if (3 != env->argc)
            err = EINVAL;

        int seconds = strtol(env->argv[2], NULL, 10);
        if (0 > seconds || 3600 < seconds)
            err = EINVAL;

        if (0 == err)
            nvm->ring_snooze_seconds = (uint16_t)seconds;
    }
// REVIEW: clock reminder & intv
    else if (0 == strcmp("reminder", env->argv[1]))
    {
        if (3 != env->argc && 4 != env->argc)
            err = EINVAL;

        int seconds = strtol(env->argv[2], NULL, 10);
        if (0 > seconds || 3600 < seconds)
            err = EINVAL;

        int intv_seconds = CLOCK_DEF_REMINDER_INTV_SECONDS;
        if (4 == env->argc)
        {
            intv_seconds = strtol(env->argv[3], NULL, 10);
            if (15 > intv_seconds || 600 < intv_seconds)
                err = EINVAL;
        }

        if (0 == err)
        {
            nvm->reminder_seconds = (uint16_t)seconds;
            nvm->reminder_intv_seconds = (uint16_t)intv_seconds;
        }
    }
// REVIEW: clock timezone & dst
    else if (0 == strcmp("tz", env->argv[1]))
    {
        if (3 != env->argc)
            err = EINVAL;

        int tz = strtol(env->argv[2], NULL, 10);
        if (-7200 > tz || 7200 < tz)
            err = EINVAL;

        if (0 == err)
            nvm->timezone_offset = (int16_t)tz;
    }
    else if (0 == strcmp("dst", env->argv[1]))
    {
        if (3 == env->argc)
        {
            if (0 != strcasecmp(env->argv[1], "on") && 0 != strcasecmp(env->argv[2], "off"))
                err = EINVAL;

            nvm->dst.en = 0 == strcasecmp(env->argv[1], "on");
        }
        else
        {
            if (1)
            {
                int offset = strtol(env->argv[2], NULL, 10);
                if (120 < abs(offset))
                {
                    nvm->dst.en = true;
                    nvm->dst.minute_offset = (int8_t)offset;
                }
                else
                {
                    nvm->dst.en = false;
                    nvm->dst.minute_offset = 0;
                }
            }

            // parse YYYYMMDDHH~YYYYMMDDHH ...
            for (int i = 3; i < env->argc; i ++)
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
                nvm->dst.tbl[nvm->dst.tbl_count].start = val;

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
                nvm->dst.tbl[nvm->dst.tbl_count].end = val;

                if (nvm->dst.tbl[nvm->dst.tbl_count].start > nvm->dst.tbl[nvm->dst.tbl_count].end)
                {
                    int tmp = nvm->dst.tbl[nvm->dst.tbl_count].start;
                    nvm->dst.tbl[nvm->dst.tbl_count].start = nvm->dst.tbl[nvm->dst.tbl_count].end;
                    nvm->dst.tbl[nvm->dst.tbl_count].end = tmp;
                }

                if (++ nvm->dst.tbl_count == lengthof(nvm->dst.tbl))
                    break;
            }
        }
    }
// REVIEW: zero hour voice
    else if (0 == strcmp("zhour", env->argv[1]))
    {
        if (3 != env->argc && 4 != env->argc)
            err = EINVAL;

        unsigned zhour_mask;
        if (1)
        {
            char *end;
            zhour_mask = strtoul(env->argv[2], &end, 10);
            if ('\0' != *end)
                zhour_mask = strtoul(env->argv[2], &end, 16);

            if (0xFFFFFF < zhour_mask)
                err = EINVAL;
        }

        int wdays = 0x7F;
        if (true)
        {
            char *wday_str = CMD_paramvalue_byname("wdays", env->argc, env->argv);
            if (wday_str)
            {
                wdays = strtol(wday_str, NULL, 10);
                if (0 == wdays)
                    wdays = strtol(wday_str, NULL, 16);
                if (0 == wdays || 0x7F < wdays)
                    err = EINVAL;
            }
        }

        if (0 == err)
        {
            nvm->say_zero_hour_mask = 0xFFFFFF & zhour_mask;
            nvm->say_zero_hour_wdays = 0x7F & wdays;
        }
    }
    else
        err = EINVAL;

    if (0 == err)
    {
        NVM_set(CLOCK_NVM_ID, sizeof(*nvm), nvm);
        nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));

        VOICE_say_setting(VOICE_SETTING_DONE);
    }

    free(nvm);
    return err;
}

static int SHELL_alarm(struct UCSH_env *env)
{
    if (1 == env->argc)
    {
        char *buf = env->buf + 32;
        int flush_bytes = MIN(200, (env->bufsize - 32) / 2);
        int pos = 0;
        int cnt = 0;

        pos += sprintf(&buf[pos], "{\n\t\"alarms\": [");
        for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
        {
            struct CLOCK_moment_t *alarm = &alarms[idx];

            // deleted condition
            if (! alarm->enabled && 0 == alarm->wdays && 0 == alarm->mdate)
                continue;

            if (0 == cnt ++)
                pos += sprintf(&buf[pos], "\n");
            else
                pos += sprintf(&buf[pos], ",\n");

            pos += sprintf(&buf[pos], "\t\t{\"id\":%d, ", idx + 1);
            pos += sprintf(&buf[pos], "\"enabled\":%s, ", alarm->enabled ? "true" : "false");
            pos += sprintf(&buf[pos], "\"mtime\":%d, ",  alarm->mtime);
            pos += sprintf(&buf[pos], "\"ringtone_id\":%d, ", alarm->ringtone_id);
            pos += sprintf(&buf[pos], "\"mdate\":%lu, ",  alarm->mdate);
            pos += sprintf(&buf[pos], "\"wdays\":%d}", alarm->wdays);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (0 != cnt)
            pos += sprintf(&buf[pos], "\n\t],\n");
        else
            pos += sprintf(&buf[pos], "],\n");

        pos += sprintf(&buf[pos], "\t\"alarm_count\":%d,\n", lengthof(alarms));
        pos += sprintf(&buf[pos], "\t\"alarm_ctrl\":\"%s\"\n}\n", CLOCK_alarm_switch_is_on() ? "on" : "off");

        writebuf(env->fd, buf, (unsigned)pos);
        return 0;
    }
    else if (3 == env->argc)         // alarm <1~COUNT> <enable/disable>
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
            NVM_set(CLOCK_ALARM_NVM_ID, sizeof(alarms), &alarms);
        }

        VOICE_say_setting(VOICE_SETTING_DONE);
        return 0;
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
                if (0 == wdays || 0x7F < wdays)
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
            NVM_set(CLOCK_ALARM_NVM_ID, sizeof(alarms), &alarms);
        }

        VOICE_say_setting(VOICE_SETTING_DONE);
        return 0;
    }
    else
        return EINVAL;
}

static int SHELL_reminder(struct UCSH_env *env)
{
    if (1 == env->argc)
    {
        char *buf = env->buf + 32;
        int flush_bytes = MIN(200, (env->bufsize - 32) / 2);
        int pos = 0;
        int cnt = 0;

        pos += sprintf(&buf[pos], "{\n\t\"reminders\": [");
        for (unsigned idx = 0; idx < lengthof(reminders); idx ++)
        {
            struct CLOCK_moment_t *reminder = &reminders[idx];

            // deleted condition
            if (! reminder->enabled && 0 == reminder->wdays && 0 == reminder->mdate)
                continue;
            if (0 == cnt ++)
                pos += sprintf(&buf[pos], "\n");
            else
                pos += sprintf(&buf[pos], ",\n");

            pos += sprintf(&buf[pos], "\t{\"id\":%d, ", idx + 1);
            pos += sprintf(&buf[pos], "\"enabled\":%s, ", reminder->enabled ? "true" : "false");
            pos += sprintf(&buf[pos], "\"mtime\":%d, ",  reminder->mtime);
            pos += sprintf(&buf[pos], "\"reminder_id\":%d, ", reminder->reminder_id);
            pos += sprintf(&buf[pos], "\"mdate\":%lu, ",  reminder->mdate);
            pos += sprintf(&buf[pos], "\"wdays\":%d}", reminder->wdays);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (0 != cnt)
            pos += sprintf(&buf[pos], "\n\t],\n");
        else
            pos += sprintf(&buf[pos], "],\n");

        pos += sprintf(&buf[pos], "\t\"reminder_count\": %d\n}\n", lengthof(reminders));

        writebuf(env->fd, buf, (unsigned)pos);
        return 0;
    }
    else if (3 == env->argc)         // rmd <1~COUNT> <enable/disable>
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
            NVM_set(CLOCK_REMINDER_NVM_ID, sizeof(reminders), &reminders);
        }

        VOICE_say_setting(VOICE_SETTING_DONE);
        return 0;
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
                if (0 == wdays || 0x7F < wdays)
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
            NVM_set(CLOCK_REMINDER_NVM_ID, sizeof(reminders), &reminders);
        }

        VOICE_say_setting(VOICE_SETTING_DONE);
        return 0;
    }
    else
        return EINVAL;
}
