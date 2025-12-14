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

/****************************************************************************
 *  @def
 ****************************************************************************/
 #define COMPILE_YEAR                    (\
    (__DATE__[7] - '0') * 1000 +    \
    (__DATE__[8] - '0') *  100 +    \
    (__DATE__[9] - '0') *   10 +    \
    (__DATE__[10] - '0')   \
)

#define CLOCK_NVM_ID                    NVM_DEFINE_KEY('C', 'L', 'K', 'C')
#define CLOCK_ALARM_NVM_ID              NVM_DEFINE_KEY('C', 'A', 'L', 'M')
#define CLOCK_REMINDER_NVM_ID           NVM_DEFINE_KEY('C', 'R', 'M', 'D')

#define ALARM_COUNT                     (NVM_MAX_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))
#define REMINDER_COUNT                  (NVM_MAX_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))
#define ALARM_RINGTONE_ID_APP_SPECIFY   (0xFF)

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

    struct CLOCK_moment_t app_specify_moment;
};
static_assert(sizeof(struct CLOCK_nvm_t) <= NVM_MAX_OBJECT_SIZE, "");

struct CLOCK_runtime_t
{
    struct tm dt;
    time_t ts;

    struct timeout_t intv_next;
    time_t ts_alarm_snooze_end;
    time_t ts_reminder_slient_end;

    bytebool_t dst_active;
    int8_t alarming_idx;


    int16_t app_specify_cb_mtime;
    void (* app_specify_cb_moment)(void);
};

/****************************************************************************
 *  @internal
 ****************************************************************************/
static struct CLOCK_runtime_t clock_runtime = {0};

static struct CLOCK_moment_t alarms[ALARM_COUNT];
static struct CLOCK_moment_t reminders[ALARM_COUNT];

static int8_t CLOCK_peek_start_alarms(struct CLOCK_nvm_t const *nvm_ptr);
static void CLOCK_intv_next_callback(void *arg);
static unsigned CLOCK_reminders(struct tm const *dt, bool ignore_snooze, bool saying);

// shell commands
static int SHELL_clock(struct UCSH_env *env);       // REVIEW: misc clock settings
static int SHELL_alarm(struct UCSH_env *env);
static int SHELL_reminder(struct UCSH_env *env);

/****************************************************************************
 *  @implements: override time.c
 ****************************************************************************/
int get_timezone_offset(void)
{
    struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));

    if (NULL == nvm_ptr)
        return 0;
    else
        return nvm_ptr->timezone_offset;
}

int get_dst_offset(struct tm *tm)
{
    struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));

    if (NULL == nvm_ptr)
        return 0;

    if (nvm_ptr->dst.en && 0 != nvm_ptr->dst.minute_offset && 0 < nvm_ptr->dst.tbl_count)
    {
        int dt = (tm->tm_year + 1900) * (1000000) + (tm->tm_mon + 1) * 10000 + tm->tm_mday * 100 + tm->tm_hour;

        for (unsigned i = 0; i < nvm_ptr->dst.tbl_count; i ++)
        {
            if (dt >= nvm_ptr->dst.tbl[i].start && dt < nvm_ptr->dst.tbl[i].end)
            {
                clock_runtime.dst_active = true;
                return 60 * nvm_ptr->dst.minute_offset;
            }
        }
    }

    clock_runtime.dst_active = false;
    return 0;
}

/****************************************************************************
 *  @implements: override rtc.c
 ****************************************************************************/
void RTC_updated_callback(time_t ts)
{
    struct tm const *dt = localtime_r(&ts, &clock_runtime.dt);

    if (ts != clock_runtime.ts)
        CLOCK_update_display_callback(dt);

    clock_runtime.ts = ts;
}

/****************************************************************************
 *  @implements
 ****************************************************************************/
__attribute__((weak))
bool CLOCK_alarm_switch_is_on(void)
{
    return true;
}

__attribute__((weak))
void CLOCK_update_display_callback(struct tm const *dt)
{
    ARG_UNUSED(dt);
}

__attribute__((weak))
uint8_t CLOCK_get_dim_percent(void)
{
    return 0;
}

__attribute__((weak))
void CLOCK_shell_set_dim_percent(uint8_t dim_percent)
{
    ARG_UNUSED(dim_percent);
}

/****************************************************************************
 *  @implements
 ****************************************************************************/
void CLOCK_init(void)
{
    struct CLOCK_nvm_t const *nvm_ptr;
    if (NULL == (nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr))))
    {
        struct CLOCK_nvm_t *nvm = malloc(sizeof(struct CLOCK_nvm_t));
        if (NULL == nvm)
        {
            __BREAK_IFDBG();
            NVIC_SystemReset();
        }
        memset(nvm, 0, sizeof(*nvm));

        nvm->ring_seconds = CLOCK_DEF_RING_SECONDS;
        nvm->ring_snooze_seconds = CLOCK_DEF_SNOOZE_SECONDS;
        nvm->reminder_seconds = CLOCK_DEF_RMD_SECONDS;
        nvm->reminder_intv_seconds = CLOCK_DEF_RMD_INTV_SECONDS;

        NVM_set(CLOCK_NVM_ID, sizeof(*nvm), nvm);
        free(nvm);

        nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));
    }

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

        CLOCK_update_display_callback(&dt);
    }

    clock_runtime.alarming_idx = -1;
    timeout_init(&clock_runtime.intv_next, 1000 * nvm_ptr->reminder_intv_seconds, CLOCK_intv_next_callback, TIMEOUT_FLAG_REPEAT);

    if (0 != NVM_get(CLOCK_ALARM_NVM_ID, sizeof(alarms), &alarms))
        memset(&alarms, 0, sizeof(alarms));

    if (0 != NVM_get(CLOCK_REMINDER_NVM_ID, sizeof(reminders), &reminders))
        memset(&reminders, 0, sizeof(reminders));

    UCSH_REGISTER("clock",      SHELL_clock);
    UCSH_REGISTER("alm",        SHELL_alarm);
    UCSH_REGISTER("rmd",        SHELL_reminder);
}

void CLOCK_schedule(void)
{
    struct tm const *dt = CLOCK_update_timestamp(NULL);
    struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));

    if (NULL != clock_runtime.app_specify_cb_moment && nvm_ptr->app_specify_moment.enabled)
    {
        int16_t mtime = time2mtime(clock_runtime.ts);
        struct CLOCK_moment_t const *moment = &nvm_ptr->app_specify_moment;

        // matching week days mask or mdate
        if (0 == ((1 << dt->tm_wday) & moment->wdays))
        {
            int32_t mdate = (((dt->tm_year + 1900) * 100 + dt->tm_mon + 1) * 100 + dt->tm_mday);

            if (mdate == moment->mdate)
                goto app_moment_check_mtime;
        }
        else
        {
        app_moment_check_mtime:
            if (mtime == moment->mtime && mtime != clock_runtime.app_specify_cb_mtime)
                clock_runtime.app_specify_cb_moment();

            clock_runtime.app_specify_cb_mtime = mtime;
        }
    }

    if (-1 != CLOCK_peek_start_alarms(nvm_ptr))
    {
        timeout_stop(&clock_runtime.intv_next);
        return;
    }

    if (0 != nvm_ptr->say_zero_hour_mask)
    {
        unsigned next_zero_hour = (unsigned)((clock_runtime.ts % 86400) / 3600 + 1) % 24;
        int next_zsec_intv = -1;

        if ((1U << next_zero_hour) & nvm_ptr->say_zero_hour_mask)
        {
            if ((0 == nvm_ptr->say_zero_hour_wdays || 0x7F == nvm_ptr->say_zero_hour_wdays) ||
                ((1U << dt->tm_wday) & nvm_ptr->say_zero_hour_wdays))
            {
                next_zsec_intv = 1000 * (3600 - (int)(clock_runtime.ts % 3600));
            }

            if (0 <= next_zsec_intv && 1000 * nvm_ptr->reminder_intv_seconds > next_zsec_intv)
            {
                timeout_update(&clock_runtime.intv_next, (unsigned)next_zsec_intv);
                timeout_start(&clock_runtime.intv_next, NULL);
            }
        }
    }

    if (! timeout_is_running(&clock_runtime.intv_next))
    {
        if(0 != CLOCK_say_reminders(dt, false))
        {
            timeout_update(&clock_runtime.intv_next, 1000 * nvm_ptr->reminder_intv_seconds);
            timeout_start(&clock_runtime.intv_next, reminders);
        }
    }
}

time_t CLOCK_get_timestamp(void)
{
    return clock_runtime.ts;
}

struct tm const *CLOCK_update_timestamp(time_t *ts_out)
{
    time_t ts = time(NULL);

    if (NULL != ts_out)
        *ts_out = ts;

    struct tm const *dt = localtime_r(&ts, &clock_runtime.dt);

    if (ts != clock_runtime.ts)
        CLOCK_update_display_callback(dt);

    clock_runtime.ts = ts;
    return dt;
}

bool CLOCK_get_dst_is_active(void)
{
    return clock_runtime.dst_active;
}

/****************************************************************************
 *  @implements: app specify callback
 ****************************************************************************/
void CLOCK_app_specify_callback(void (*callback)(void))
{
    clock_runtime.app_specify_cb_moment = callback;
}

struct CLOCK_moment_t const *CLOCK_get_app_specify_moment(void)
{
    struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));
    if (NULL != nvm_ptr)
        return &nvm_ptr->app_specify_moment;
    else
        return NULL;
}

int CLOCK_store_app_specify_moment(struct CLOCK_moment_t *moment)
{
    struct CLOCK_nvm_t *nvm = malloc(sizeof(struct CLOCK_nvm_t));
    if (NULL == nvm)
        return ENOMEM;

    NVM_get(CLOCK_NVM_ID, sizeof(*nvm), nvm);
    nvm->app_specify_moment = *moment;

    NVM_set(CLOCK_NVM_ID, sizeof(*nvm), nvm);
    free(nvm);

    return 0;
}

 /***************************************************************************
 * @def: alarms & reminders
 ***************************************************************************/
unsigned CLOCK_alarm_max_count(void)
{
    return lengthof(alarms);
}

bool CLOCK_is_reminding(void)
{
    time_t ts = time(NULL);
    return 0 < CLOCK_reminders(localtime(&ts), true, false);
}

int CLOCK_get_ringtone_id(void)
{
    int ringtone_id = -1;
    struct tm const *dt = CLOCK_update_timestamp(NULL);

    for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &alarms[idx];
        if (alarm->enabled || -1 == ringtone_id)
            ringtone_id = alarm->ringtone_id;

        if (0 != ((1 << ((dt->tm_wday + 1) % 7)) & alarm->wdays))
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

void CLOCK_update_alarms(void)
{
    NVM_set(CLOCK_ALARM_NVM_ID, sizeof(alarms), &alarms);
}

static bool CLOCK_canceling(bool snooze)
{
    // TODO: clock snooze
    (void)snooze;

    if (-1 != clock_runtime.alarming_idx)
    {
        struct CLOCK_moment_t *ptr = &alarms[clock_runtime.alarming_idx];
        clock_runtime.alarming_idx = -1;
        clock_runtime.ts_alarm_snooze_end = time(NULL) + 60;

        if (! mplayer_is_idle()) mplayer_stop();

        if (ALARM_RINGTONE_ID_APP_SPECIFY == ptr->ringtone_id)
            CLOCK_stop_app_ringtone_cb((uint8_t)(ptr - alarms));

        return true;
    }
    else
    {
        // stop activity reminders
        if (timeout_is_running(&clock_runtime.intv_next))
        {
            timeout_stop(&clock_runtime.intv_next);

            struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));
            clock_runtime.ts_reminder_slient_end = time(NULL) + nvm_ptr->reminder_seconds;
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

static unsigned CLOCK_reminders(struct tm const *dt, bool ignore_snooze, bool saying)
{
    unsigned reminder_count = 0;
    struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));

    time_t ts_base = clock_runtime.ts - clock_runtime.ts % 86400;
    int16_t mtime = time2mtime(clock_runtime.ts);

    for (unsigned idx = 0; idx < lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t *reminder = &reminders[idx];

        if (! reminder->enabled)
            continue;

        time_t reminder_end_ts = ts_base + mtime2time(reminder->mtime) + nvm_ptr->reminder_seconds;

        // matching week days mask or mdate
        if (0 == ((1 << dt->tm_wday) & reminder->wdays))
        {
            int32_t mdate =
                (((dt->tm_year + 1900) * 100 + dt->tm_mon + 1) * 100 + dt->tm_mday);

            if (mdate != reminder->mdate)
                continue;
        }

        if (mtime >= reminder->mtime && clock_runtime.ts < reminder_end_ts)
        {
            if (ignore_snooze || reminder_end_ts > clock_runtime.ts_reminder_slient_end)
            {
                if (reminder_end_ts > clock_runtime.ts_reminder_slient_end)
                    reminder_count ++;

                if (saying)
                    VOICE_play_reminder(reminder->reminder_id);
            }
        }
    }
    return reminder_count;
}

unsigned CLOCK_say_reminders(struct tm const *dt, bool ignore_snooze)
{
    return CLOCK_reminders(dt, ignore_snooze, true);
}

unsigned CLOCK_now_say_reminders(bool ignore_snooze)
{
    struct tm const *dt = CLOCK_update_timestamp(NULL);
    return CLOCK_say_reminders(dt, ignore_snooze);
}

__attribute__((weak))
char const *CLOCK_get_app_ringtone_cb(uint8_t alarm_idx)
{
    ARG_UNUSED(alarm_idx);
    return NULL;
}

__attribute__((weak))
int CLOCK_set_app_ringtone_cb(uint8_t alarm_idx, char *str)
{
    ARG_UNUSED(alarm_idx, str);
    return ENOSYS;
}

__attribute__((weak))
void CLOCK_start_app_ringtone_cb(uint8_t alarm_idx)
{
    ARG_UNUSED(alarm_idx);
}

__attribute__((weak))
void CLOCK_stop_app_ringtone_cb(uint8_t alarm_idx)
{
    ARG_UNUSED(alarm_idx);
}

/***************************************************************************
 * @implements: utils
 ***************************************************************************/
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
 *  @internal
 ****************************************************************************/
static int8_t CLOCK_peek_start_alarms(struct CLOCK_nvm_t const *nvm_ptr)
{
    if (! CLOCK_alarm_switch_is_on())
        return -1;
    if (clock_runtime.ts <= clock_runtime.ts_alarm_snooze_end)
        return -1;

    struct tm const *dt = &clock_runtime.dt;
    int16_t mtime = time2mtime(clock_runtime.ts);

    struct CLOCK_moment_t const *current_alarm = NULL;
    int8_t old_alarming_idx = clock_runtime.alarming_idx;

    if (-1 != clock_runtime.alarming_idx)
    {
        current_alarm = &alarms[clock_runtime.alarming_idx];
        time_t ts_end = (mtime2time(current_alarm->mtime) + nvm_ptr->ring_seconds) % 86400;

        if (ts_end < clock_runtime.ts % 86400)
        {
            current_alarm = NULL;
            clock_runtime.alarming_idx = -1;
        }
    }

    for (int8_t idx = 0; idx < (int8_t)lengthof(alarms); idx ++)
    {
        struct CLOCK_moment_t const *alarm = &alarms[idx];

        if (! alarm->enabled || alarm == current_alarm)
            continue;

        // matching week days mask or mdate
        if (0 == ((1 << dt->tm_wday) & alarm->wdays))
        {
            int32_t mdate = (((dt->tm_year + 1900) * 100 + dt->tm_mon + 1) * 100 + dt->tm_mday);

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
        if (old_alarming_idx == clock_runtime.alarming_idx)
        {
            if (mplayer_is_idle())
                VOICE_play_ringtone(current_alarm->ringtone_id);
        }
        else
        {
            if (ALARM_RINGTONE_ID_APP_SPECIFY == current_alarm->ringtone_id)
                CLOCK_start_app_ringtone_cb((uint8_t)clock_runtime.alarming_idx);
            else
                VOICE_play_ringtone(current_alarm->ringtone_id);
        }
        return clock_runtime.alarming_idx;
    }
    else
        return -1;
}

static void CLOCK_intv_next_callback(void *arg)
{
    timeout_stop(&clock_runtime.intv_next);
    struct tm const *dt = CLOCK_update_timestamp(NULL);

    if (NULL == arg)
        VOICE_say_time(dt);

    if (0 != CLOCK_say_reminders(dt, true))
        timeout_start(&clock_runtime.intv_next, arg);
}

 /****************************************************************************
 *  @internal: shell commands
 ****************************************************************************/
static int SHELL_clock(struct UCSH_env *env)
{
    struct CLOCK_nvm_t const *nvm_ptr = NVM_get_ptr(CLOCK_NVM_ID, sizeof(*nvm_ptr));

    if (1 == env->argc)
    {
        int flush_bytes = MIN(200, (env->bufsize - 32) / 2);
        int pos = 0;

        pos += sprintf(env->buf + pos, "{");
        if (1)  // ring & reminder
        {
            pos += sprintf(env->buf + pos, "\n\t\"ring\": %d", nvm_ptr->ring_seconds);
            pos += sprintf(env->buf + pos, ",\n\t\"snooze\": %d", nvm_ptr->ring_snooze_seconds);
            pos += sprintf(env->buf + pos, ",\n\t\"reminder\": %d", nvm_ptr->reminder_seconds);
            pos += sprintf(env->buf + pos, ",\n\t\"reminder_intv\": %d", nvm_ptr->reminder_intv_seconds);
            pos += sprintf(env->buf + pos, ",\n\t\"dim\": %d", CLOCK_get_dim_percent());

            if (flush_bytes < pos)
            {
                writebuf(env->fd, env->buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (1)  // zero hour voide
        {
            pos += sprintf(env->buf + pos, ",\n\t\"zero_hour_voice\":\n\t{");
            pos += sprintf(env->buf + pos, "\n\t\t\"mask\": %d", nvm_ptr->say_zero_hour_mask);
            pos += sprintf(env->buf + pos, ",\n\t\t\"wdays\": %d", nvm_ptr->say_zero_hour_wdays);

            pos += sprintf(env->buf + pos, "\n\t}");
            if (flush_bytes < pos)
            {
                writebuf(env->fd, env->buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (1)  // timezone & dst
        {
            pos += sprintf(env->buf + pos, ",\n\t\"timezone_offset\": %d", nvm_ptr->timezone_offset);

            pos += sprintf(env->buf + pos, ",\n\t\"dst\":\n\t{");
            pos += sprintf(env->buf + pos, "\n\t\t\"enabled\": %s", nvm_ptr->dst.en ? "true" : "false");
            pos += sprintf(env->buf + pos, ",\n\t\t\"activated\": %s", clock_runtime.dst_active ? "true" : "false");
            pos += sprintf(env->buf + pos, ",\n\t\t\"minute_offset\": %d", nvm_ptr->dst.minute_offset);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, env->buf, (unsigned)pos);
                pos = 0;
            }

            pos += sprintf(env->buf + pos, ",\n\t\t\"ranges\": [");
            for (unsigned i = 0; i < nvm_ptr->dst.tbl_count; i ++)
            {
                if (0 == i)
                    pos += sprintf(env->buf + pos, "\n\t\t\t");
                else
                    pos += sprintf(env->buf + pos, ",\n\t\t\t");

                pos += sprintf(env->buf + pos, "{\"start\": %d, \"end\": %d}", nvm_ptr->dst.tbl[i].start, nvm_ptr->dst.tbl[i].end);

                if (flush_bytes < pos)
                {
                    writebuf(env->fd, env->buf, (unsigned)pos);
                    pos = 0;
                }
            }

            pos += sprintf(env->buf + pos, "\n\t\t]\n\t}");
        }

        pos += sprintf(env->buf + pos, "\n}\n");
        writebuf(env->fd, env->buf, (unsigned)pos);
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

        int intv_seconds = CLOCK_DEF_RMD_INTV_SECONDS;
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
    else if (0 == strcmp("dim", env->argv[1]))
    {
        if (3 != env->argc)
            err = EINVAL;

        int dim = strtol(env->argv[2], NULL, 10);
        if ((0 > dim || 100 < dim))
            err = EINVAL;

        if (0 == err)
            CLOCK_shell_set_dim_percent((uint8_t)dim);
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
        if (2 == env->argc)
            return EINVAL;

        if (3 == env->argc)
        {
            if (0 != strcasecmp(env->argv[2], "on") && 0 != strcasecmp(env->argv[2], "off"))
                err = EINVAL;

            nvm->dst.en = 0 == strcasecmp(env->argv[2], "on");
        }
        else
        {
            if (1)
            {
                int offset = strtol(env->argv[2], NULL, 10);
                if (120 >= abs(offset))
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

            nvm->dst.tbl_count = 0;
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

            if (1)
            {
                struct tm dt;
                time_t ts = time(NULL);
                localtime_r(&ts, &dt);
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
        VOICE_say_setting(VOICE_SETTING_DONE);
    }

    free(nvm);
    return err;
}

static int SHELL_alarm(struct UCSH_env *env)
{
    if (1 == env->argc)
    {
        int flush_bytes = MIN(200, (env->bufsize - 32) / 2);
        int pos = 0;
        int cnt = 0;

        pos += sprintf(env->buf + pos, "{\n\t\"alarms\": [");
        for (uint8_t idx = 0; idx < lengthof(alarms); idx ++)
        {
            struct CLOCK_moment_t *alarm = &alarms[idx];

            // deleted condition
            if (! alarm->enabled && 0 == alarm->wdays && 0 == alarm->mdate)
                continue;

            if (0 == cnt ++)
                pos += sprintf(env->buf + pos, "\n");
            else
                pos += sprintf(env->buf + pos, ",\n");

            pos += sprintf(env->buf + pos, "\t\t{\"id\":%d, ", idx + 1);
            pos += sprintf(env->buf + pos, "\"enabled\":%s, ", alarm->enabled ? "true" : "false");
            pos += sprintf(env->buf + pos, "\"mtime\":%d, ",  alarm->mtime);
            if (1)
            {
                char const *str;
                if (ALARM_RINGTONE_ID_APP_SPECIFY == alarm->ringtone_id)
                {
                    str = CLOCK_get_app_ringtone_cb(idx);
                    pos += sprintf(env->buf + pos, "\"ringtone_id\":%s, ", NULL == str ? "\"\"" : str);
                }
                else
                    pos += sprintf(env->buf + pos, "\"ringtone_id\":%d, ", alarm->ringtone_id);
            }
            pos += sprintf(env->buf + pos, "\"mdate\":%lu, ",  alarm->mdate);
            pos += sprintf(env->buf + pos, "\"wdays\":%d}", alarm->wdays);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, env->buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (0 != cnt)
            pos += sprintf(env->buf + pos, "\n\t],\n");
        else
            pos += sprintf(env->buf + pos, "],\n");

        pos += sprintf(env->buf + pos, "\t\"alarm_count\":%d,\n", lengthof(alarms));
        pos += sprintf(env->buf + pos, "\t\"alarm_ctrl\":\"%s\"\n}\n", CLOCK_alarm_switch_is_on() ? "on" : "off");

        writebuf(env->fd, env->buf, (unsigned)pos);
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
    else if (5 < env->argc)     // alarm <1~COUNT> <enable/disable> 1700 <ringtone_id/"string"> wdays=0x7f
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

        int ringtone;
        if (1)
        {
            char *endptr;
            ringtone = strtol(env->argv[4], &endptr, 10);
            if ('\0' != *endptr)
            {
                int err = CLOCK_set_app_ringtone_cb((uint8_t)(idx - 1), env->argv[4]);
                if (0 == err)
                    ringtone = ALARM_RINGTONE_ID_APP_SPECIFY;
                else
                    ringtone = VOICE_select_ringtone(0);    // REVIEW: fallback to using ringtone 0
            }
            else
                ringtone = VOICE_select_ringtone(ringtone);
        }

        int wdays = 0;
        if (true)
        {
            char *wday_str = CMD_paramvalue_byname("wdays", env->argc, env->argv);
            if (wday_str)
            {
                wdays = strtol(wday_str, NULL, 10);
                if (0 == wdays)
                    wdays = strtol(wday_str, NULL, 16);
                if (0x7F < wdays)
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
        int flush_bytes = MIN(200, (env->bufsize - 32) / 2);
        int pos = 0;
        int cnt = 0;

        pos += sprintf(env->buf + pos, "{\n\t\"reminders\": [");
        for (unsigned idx = 0; idx < lengthof(reminders); idx ++)
        {
            struct CLOCK_moment_t *reminder = &reminders[idx];

            // deleted condition
            if (! reminder->enabled && 0 == reminder->wdays && 0 == reminder->mdate)
                continue;
            if (0 == cnt ++)
                pos += sprintf(env->buf + pos, "\n");
            else
                pos += sprintf(env->buf + pos, ",\n");

            pos += sprintf(env->buf + pos, "\t{\"id\":%d, ", idx + 1);
            pos += sprintf(env->buf + pos, "\"enabled\":%s, ", reminder->enabled ? "true" : "false");
            pos += sprintf(env->buf + pos, "\"mtime\":%d, ",  reminder->mtime);
            pos += sprintf(env->buf + pos, "\"reminder_id\":%d, ", reminder->reminder_id);
            pos += sprintf(env->buf + pos, "\"mdate\":%lu, ",  reminder->mdate);
            pos += sprintf(env->buf + pos, "\"wdays\":%d}", reminder->wdays);

            if (flush_bytes < pos)
            {
                writebuf(env->fd, env->buf, (unsigned)pos);
                pos = 0;
            }
        }
        if (0 != cnt)
            pos += sprintf(env->buf + pos, "\n\t],\n");
        else
            pos += sprintf(env->buf + pos, "],\n");

        pos += sprintf(env->buf + pos, "\t\"reminder_count\": %d\n}\n", lengthof(reminders));

        writebuf(env->fd, env->buf, (unsigned)pos);
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
