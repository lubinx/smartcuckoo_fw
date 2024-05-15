#include <rtc.h>
#include "datetime_utils.h"

static void TM_mday_correction(struct tm *dt)
{
    int old_mday = dt->tm_mday;
    mktime(dt);

    // mday 31=>30/29/28 round up a month
    if (old_mday != dt->tm_mday)
    {
        dt->tm_mday = 0;
        mktime(dt);
    }
}

void TM_year_add(struct tm *dt, int value)
{
    dt->tm_year += value;

    if (YEAR_ROUND_LO > dt->tm_year + 1900)
        dt->tm_year = YEAR_ROUND_HI - 1900;
    else if (YEAR_ROUND_HI < dt->tm_year + 1900)
        dt->tm_year = YEAR_ROUND_LO - 1900;

    TM_mday_correction(dt);
}

void TM_month_add(struct tm *dt, int value)
{
    dt->tm_mon += value;
    while (0 > dt->tm_mon)
        dt->tm_mon += 12;
    dt->tm_mon %= 12;

    TM_mday_correction(dt);
}

void TM_mday_add(struct tm *dt, int value)
{
    int old_year = dt->tm_year;
    int old_month = dt->tm_mon;
    dt->tm_mday += value;
    mktime(dt);

    dt->tm_year = old_year;
    dt->tm_mon = old_month;
    TM_mday_correction(dt);
}

void RTC_year_add(int value)
{
    time_t ts = time(NULL);
    struct tm dt;

    localtime_r(&ts, &dt);
    TM_year_add(&dt, value);

    RTC_set_epoch_time(mktime(&dt));
}

void RTC_month_add(int value)
{
    time_t ts = time(NULL);
    struct tm dt;

    localtime_r(&ts, &dt);
    TM_month_add(&dt, value);

    RTC_set_epoch_time(mktime(&dt));
}

void RTC_mday_add(int value)
{
    time_t ts = time(NULL);
    struct tm dt;

    localtime_r(&ts, &dt);
    TM_mday_add(&dt, value);

    RTC_set_epoch_time(mktime(&dt));
}

void RTC_hour_add(int value)
{
    time_t ts = time(NULL);
    struct tm dt;
    localtime_r(&ts, &dt);

    dt.tm_hour += value;
    while (0 > dt.tm_hour)
        dt.tm_hour += 24;
    dt.tm_hour %= 24;

    RTC_set_epoch_time(mktime(&dt));
}

void RTC_minute_add(int value)
{
    time_t ts = time(NULL);
    struct tm dt;
    localtime_r(&ts, &dt);

    dt.tm_min += value;
    while (0 > dt.tm_min)
        dt.tm_min += 60;
    dt.tm_min %= 60;

    dt.tm_sec = 0;
    RTC_set_epoch_time(mktime(&dt));
}
