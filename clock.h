#ifndef __CLOCK_H
#define __CLOCK_H

#include <features.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

    #define time_to_mtime(ts)           ((int16_t)(((ts % 86400) / 3600) * 100 + ((ts) % 3600) / 60))
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



__BEGIN_DECLS

    /**
     *  CLOCK_init()
     */
extern __attribute__((nothrow))
    void CLOCK_init(void);

    /**
     *  CLOCK_alarm_switch_is_on()
     *
     *  NOTE: override to return hardward switcher of alarm
     *      default is alrays on
    */
extern __attribute__((nothrow, pure))
    bool CLOCK_alarm_switch_is_on(void);

    /**
     *  CLOCK_alarm_count()
     *      get maxinum alarms count was supported
    */
extern __attribute__((nothrow, const))
    unsigned CLOCK_alarm_count(void);

    /**
     *  CLOCK_get_alarming_idx()
    */
extern __attribute__((nothrow, pure))
    int8_t CLOCK_get_alarming_idx(void);

    /**
     *  CLOCK_get_ringtone_id()
    */
extern __attribute__((nothrow))
    int CLOCK_get_ringtone_id(void);

    /**
     *  CLOCK_get_alarm()
    */
extern __attribute__((nothrow, pure))
    struct CLOCK_moment_t *CLOCK_get_alarm(uint8_t idx);

    /**
     *  CLOCK_update_alarms()
    */
extern __attribute__((nothrow))
    void CLOCK_update_alarms(void);

    /**
     *  CLOCK_schedule()
    */
extern __attribute__((nothrow))
    void CLOCK_schedule(time_t ts);

    /**
     *  CLOCK_snooze() / CLOCK_dismiss()
     *  @returns
     *      false if no alarming is ringing
     *      true if current alarm is ringing and snoozed / stopped
    */
extern __attribute__((nothrow))
    bool CLOCK_snooze(void);
extern __attribute__((nothrow))
    bool CLOCK_dismiss(void);

    /**
     *  CLOCK_say_reminders()
     *
     *  @returns
     *      reminders count
    */
extern __attribute__((nothrow))
    unsigned CLOCK_say_reminders(time_t ts, bool ignore_snooze);

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
