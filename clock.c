#include "smartcuckoo.h"

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

/****************************************************************************
 *  @implements: weaks
 ****************************************************************************/
__attribute__((weak))
bool CLOCK_alarm_switch_is_on(void)
{
    return true;
}

/****************************************************************************
 *  @implements
 ****************************************************************************/
void CLOCK_init()
{
    CMU->CLKEN0_SET = CMU_CLKEN0_BURAM;
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
        time_t ts_min = mktime(&dt);

        dt.tm_year = SUPPORTED_YEAR + 1 - 1900;
        dt.tm_mon = 0;
        dt.tm_mday = 1;
        dt.tm_hour = 0;
        dt.tm_min = 0;
        dt.tm_sec = 0;

        if (0 == BURAM->RET[30].REG + BURAM->RET[31].REG)
            RTC_set_epoch_time(BURAM->RET[31].REG);
        else
            RTC_set_epoch_time(ts_min);

        now = time(NULL);
        localtime_r(&now, &dt);

        LOG_printf("%04d/%02d/%02d %02d:%02d:%02d",
            dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday,
            dt.tm_hour, dt.tm_min, dt.tm_sec);
    }

    clock_setting.alarming_idx = -1;
    timeout_init(&clock_setting.reminder_next_timeo, REMINDER_INTV, (void *)CLOCK_say_reminders, 0);

    if (0 != NVM_get(NVM_ALARM, &clock_setting.alarms, sizeof(clock_setting.alarms)))
        memset(&clock_setting.alarms, 0, sizeof(clock_setting.alarms));

    if (0 != NVM_get(NVM_REMINDER, &clock_setting.reminders, sizeof(clock_setting.reminders)))
        memset(&clock_setting.reminders, 0, sizeof(clock_setting.reminders));
}

__attribute__((weak))
bool CLOCK_alarm_switch_on(void)
{
    return true;
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

bool CLOCK_is_alarming(void)
{
    return -1 != clock_setting.alarming_idx;
}

int8_t CLOCK_peek_start_alarms(time_t ts)
{
    if (! CLOCK_alarm_switch_is_on())
        return -1;
    if (ts <= clock_setting.alarm_snooze_ts_end)
        return -1;

    struct tm dt;
    localtime_r(&ts, &dt);
    int16_t mtime = time_to_mtime(ts % 86400);
    struct CLOCK_moment_t *current_alarm = NULL;

    if (-1 != clock_setting.alarming_idx)
    {
        current_alarm = &clock_setting.alarms[clock_setting.alarming_idx];
        time_t ts_end = (mtime_to_time(current_alarm->mtime) + 60 * ALARM_TIMEOUT_MINUTES) % 86400;

        if (ts_end < time(NULL) % 86400)
        {
            current_alarm = NULL;
            clock_setting.alarming_idx = -1;
        }
    }

    for (unsigned idx = 0; idx < lengthof(clock_setting.alarms); idx ++)
    {
        struct CLOCK_moment_t *alarm = &clock_setting.alarms[idx];

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
        clock_setting.alarming_idx = (int8_t)idx;
        break;
    }

    if (NULL != current_alarm)
    {
        VOICE_play_ringtone(&voice_attr, current_alarm->ringtone_id);
        return (int8_t)(current_alarm - clock_setting.alarms);
    }
    else
        return -1;
}

bool CLOCK_stop_current_alarm(void)
{
    if (-1 != clock_setting.alarming_idx)
    {
        clock_setting.alarming_idx = -1;
        clock_setting.alarm_snooze_ts_end = time(NULL) + 60;
        return true;
    }
    else
        return false;
}

unsigned CLOCK_peek_start_reminders(time_t ts)
{
    if (! timeout_is_running(&clock_setting.reminder_next_timeo))
    {
        unsigned reminder_count = CLOCK_say_reminders(ts, false);

        if (0 != reminder_count &&
            clock_setting.reminder_ts_end > clock_setting.reminder_snooze_ts_end)
        {
            timeout_start(&clock_setting.reminder_next_timeo, NULL);
        }
        else
            timeout_stop(&clock_setting.reminder_next_timeo);

        return reminder_count;
    }
    else
        return 0;
}

unsigned CLOCK_say_reminders(time_t ts, bool ignore_snooze)
{
    unsigned reminder_count = 0;
    clock_setting.reminder_ts_end = 0;

    struct tm dt;
    localtime_r(&ts, &dt);

    time_t ts_base = ts - ts % 86400;
    int16_t mtime = time_to_mtime(ts % 86400);

    for (unsigned idx = 0; idx < lengthof(clock_setting.alarms); idx ++)
    {
        struct CLOCK_moment_t *reminder = &clock_setting.reminders[idx];

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

        if (mtime >= reminder->mtime && end_ts > ts)
        {
            if (clock_setting.reminder_ts_end < end_ts)
                clock_setting.reminder_ts_end = end_ts;

            if (ignore_snooze ||
                clock_setting.reminder_ts_end > clock_setting.reminder_snooze_ts_end)
            {
                reminder_count ++;
                VOICE_play_reminder(&voice_attr, reminder->reminder_id);
            }
        }
    }
    return reminder_count;
}

void CLOCK_snooze_reminders(void)
{
    timeout_stop(&clock_setting.reminder_next_timeo);
    clock_setting.reminder_snooze_ts_end = clock_setting.reminder_ts_end;
}
