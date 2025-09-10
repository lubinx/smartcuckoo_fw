#ifndef __CLOCK_H
#define __CLOCK_H

#include <features.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

    #define time_to_mtime(ts)           ((int16_t)(((ts) / 3600) * 100 + ((ts) % 3600) / 60))
    #define mtime_to_time(mt)           (time_t)((((mt) / 100) * 3600 + ((mt) % 100) * 60))

    struct CLOCK_moment_t
    {
        bytebool_t enabled;
        int8_t wdays;
        // bytebool_t snooze_once;

        union
        {
            uint8_t ringtone_id;
            uint8_t reminder_id;
        };

        int16_t mtime;  // 1700
        int32_t mdate;  // yyyy/mm/dd
    };

    struct CLOCK_runtime_t
    {
        int8_t alarming_idx;

        time_t alarm_snooze_ts_end;
        time_t reminder_ts_end;
        time_t reminder_snooze_ts_end;
    };
    extern struct CLOCK_runtime_t clock_runtime;

__BEGIN_DECLS

    /**
     *  CLOCK_init()
     */
extern __attribute__((nothrow))
    void CLOCK_init(void);

    /**
     *  CLOCK_alarm_switch_is_on()
    */
extern __attribute__((nothrow, pure))
    bool CLOCK_alarm_switch_is_on(void);

    /**
     *  CLOCK_update_alarms()
    */
extern __attribute__((nothrow))
    void CLOCK_update_alarms(void);

    /**
     *  CLOCK_alarm_count()
     *      get maxinum alarms supported
    */
extern __attribute__((nothrow, const))
    unsigned CLOCK_alarm_count(void);

    // return if now is alarming
extern __attribute__((nothrow, pure))
    bool CLOCK_is_alarming(void);

    /**
     *  CLOCK_get_alarm()
    */
extern __attribute__((nothrow, pure))
    struct CLOCK_moment_t *CLOCK_get_alarm(uint8_t idx);

    /**
     *  CLOCK_peek_start_reminders()
     *      iterator alarms start if nessary
     *
     *  @returns
     *      -1 none alarm is started
     *      index of alarm is started
    */
extern __attribute__((nothrow))
    int8_t CLOCK_peek_start_alarms(time_t ts);

    /**
     *  CLOCK_stop_current_alarm()
     *  @returns
     *      true if current alarm is ringing and stopped
    */
extern __attribute__((nothrow))
    bool CLOCK_stop_current_alarm(void);

    /**
     *  CLOCK_peek_start_reminders()
     *      iterator reminders start if nessary
     *
     *  @returns
     *      reminders count
    */
extern __attribute__((nothrow))
    unsigned CLOCK_peek_start_reminders(time_t ts);

    /**
     *  CLOCK_say_reminders()
     *  @returns
     *      reminders count
    */
extern __attribute__((nothrow))
    unsigned CLOCK_say_reminders(time_t ts, bool ignore_snooze);

    /**
     * CLOCK_snooze_reminders()
    */
extern __attribute__((nothrow))
    void CLOCK_snooze_reminders(void);

/***************************************************************************
 * utils
 ***************************************************************************/
extern __attribute__((nothrow))
    void CLOCK_year_add(struct CLOCK_moment_t *moment, int value);
extern __attribute__((nothrow))
    void CLOCK_month_add(struct CLOCK_moment_t *moment, int value);
extern __attribute__((nothrow))
    void CLOCK_day_add(struct CLOCK_moment_t *moment, int value);
extern __attribute__((nothrow))
    void CLOCK_hour_add(struct CLOCK_moment_t *moment, int value);
extern __attribute__((nothrow))
    void CLOCK_minute_add(struct CLOCK_moment_t *moment, int value);

__END_DECLS
#endif
