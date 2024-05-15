#ifndef __DATETIME_UTILS_H
#define __DATETIME_UTILS_H              1

#include <features.h>
#include <time.h>
#include <limits.h>

__BEGIN_DECLS

extern __attribute__((nothrow))
    void TM_year_add(struct tm *dt, int value);
extern __attribute__((nothrow))
    void TM_month_add(struct tm *dt, int value);
extern __attribute__((nothrow))
    void TM_mday_add(struct tm *dt, int value);

extern __attribute__((nothrow))
    void RTC_year_add(int value);
extern __attribute__((nothrow))
    void RTC_month_add(int value);
extern __attribute__((nothrow))
    void RTC_mday_add(int value);

extern __attribute__((nothrow))
    void RTC_hour_add(int value);
extern __attribute__((nothrow))
    void RTC_minute_add(int value);

__END_DECLS
#endif
