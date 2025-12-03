#ifndef __CLOCK_H
#define __CLOCK_H

#include <features.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifndef CLOCK_DEF_RING_SECONDS
    #define CLOCK_DEF_RING_SECONDS      (180)
#endif

#ifndef CLOCK_DEF_SNOOZE_SECONDS
    #define CLOCK_DEF_SNOOZE_SECONDS    (600)
#endif

#ifndef CLOCK_DEF_RMD_SECONDS
    #define CLOCK_DEF_RMD_SECONDS       (1200)
#endif

#ifndef CLOCK_DEF_RMD_INTV_SECONDS
    #define CLOCK_DEF_RMD_INTV_SECONDS  (60)
#endif

    #define time2mtime(ts)              ((int16_t)(((ts % 86400) / 3600) * 100 + ((ts) % 3600) / 60))
    #define mtime2time(mt)              (time_t)((((mt) / 100) * 3600 + ((mt) % 100) * 60))

    struct CLOCK_moment_t
    {
        bytebool_t enabled;
        int8_t wdays;

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
     *  CLOCK_schedule()
     */
extern __attribute__((nothrow))
    void CLOCK_schedule(void);

    /**
     *  CLOCK_get_timestamp()
    */
extern __attribute__((nothrow, pure))
    time_t CLOCK_get_timestamp(void);

    /**
     *  CLOCK_update_timestamp()
    */
extern __attribute__((nothrow))
    struct tm const *CLOCK_update_timestamp(time_t *ts_out);

    /**
     *  CLOCK_get_dst_is_active()
    */
extern __attribute__((nothrow, pure))
    bool CLOCK_get_dst_is_active(void);

/***************************************************************************
 * @def: weak
 ***************************************************************************/
    /**
     *  CLOCK_alarm_switch_is_on()
     *
     *  NOTE: override to return hardward switcher of alarm
     *      default is alrays on
    */
extern __attribute__((nothrow, pure))
    bool CLOCK_alarm_switch_is_on(void);

    /**
     *  CLOCK_update_display_callback()
    */
extern __attribute__((nothrow))
    void CLOCK_update_display_callback(struct tm const *dt);

    /**
     *  CLOCK_get_dim_percent()
    */
extern __attribute__((nothrow, pure))
    uint8_t CLOCK_get_dim_percent(void);

    /**
     *  CLOCK_shell_set_dim_percent()
    */
extern __attribute__((nothrow))
    void CLOCK_shell_set_dim_percent(uint8_t dim_percent);

/***************************************************************************
 * @def: app specify callback
 ***************************************************************************/
    /**
     *  CLOCK_app_specify_callback()
    */
extern __attribute__((nothrow, nonnull))
    void CLOCK_app_specify_callback(void (*callback)(void));

    /**
     *  CLOCK_get_app_specify_moment() / CLOCK_store_app_specify_moment()
    */
extern __attribute__((nothrow, pure))
    struct CLOCK_moment_t const * CLOCK_get_app_specify_moment(void);
extern __attribute__((nothrow, nonnull))
    int CLOCK_store_app_specify_moment(struct CLOCK_moment_t *moment);

 /***************************************************************************
 * @def: alarms & reminders
 ***************************************************************************/
    /**
     *  CLOCK_alarm_max_count()
     *      get maxinum alarms count was supported
    */
extern __attribute__((nothrow, const))
    unsigned CLOCK_alarm_max_count(void);

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
     *      store external modified alarms parameters into nvm
     *
     *  NOTE: external may direct modify "moment" obtain from CLOCK_get_alarm()
    */
extern __attribute__((nothrow))
    void CLOCK_update_alarms(void);

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
     *  CLOCK_now_say_reminders()
     *
     *  @returns
     *      reminders count
    */
extern __attribute__((nothrow))
    unsigned CLOCK_now_say_reminders(bool ignore_snooze);
extern __attribute__((nothrow))
    unsigned CLOCK_say_reminders(struct tm const *dt, bool ignore_snooze);

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
