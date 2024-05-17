#ifndef __SMARTCUCKOO_H
#define __SMARTCUCKOO_H                 1

#include <features.h>

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <vinfo.h>
#include <flash.h>
#include <rtc.h>
#include <uart.h>
#include <wdt.h>

#include <ultracore/log.h>
#include <ultracore/mq.h>
#include <ultracore/timeo.h>
#include <ultracore/nvm.h>
#include <sh/ucsh.h>

#include "datetime_utils.h"
#include "locale.h"
#include "limits.h"

#include "mplayer.h"
#include "mplayer_voice.h"
#include "mplayer_noise.h"

#include "EFR32_config.h"
#include "PERIPHERAL_config.h"

/***************************************************************************
 *  @def: global consts
 ***************************************************************************/
#ifndef PERIPHERIAL_CONN_TIMEOUT
    #define PERIPHERIAL_CONN_TIMEOUT    (30000)
#endif

#ifndef SETTING_TIMEOUT
    #define SETTING_TIMEOUT             (4800)
#endif

#ifndef ALARM_TIMEOUT_MINUTES
    #define ALARM_TIMEOUT_MINUTES       (3)
#endif

#ifndef REMINDER_INTV
    #define REMINDER_INTV               (60000)
#endif

#ifndef REMINDER_TIMEOUT_MINUTES
    #define REMINDER_TIMEOUT_MINUTES    (15)
#endif

#ifndef RTC_CALIBRATION_SECONDS
    #define RTC_CALIBRATION_SECONDS     (3)
#endif

#ifndef BATT_FULL_MV
    #define BATT_FULL_MV                (4100)
#endif
#ifndef BATT_LOW_MV
    #define BATT_LOW_MV                 (3300)
#endif
#ifndef BATT_EMPTY_MV
    #define BATT_EMPTY_MV               (3000)
#endif

/***************************************************************************
 *  @def: common PIN mux
 ***************************************************************************/
    #define I2C0_SCL                    (PC00)
    #define I2C0_SDA                    (PC01)
    #define I2C1_SCL                    (PD02)
    #define I2C1_SDA                    (PD03)

    #define UART0_TXD                   (PC06)
    #define UART0_RXD                   (PC07)
    #define UART1_TXD                   (PB03)
    #define UART1_RXD                   (PB04)

    #define CONSOLE_DEV                 (USART1)

/***************************************************************************
 *  @def: NVM
 ***************************************************************************/
    #define NVM_SETTING                 NVM_DEFINE_KEY('S', 'E', 'T', 'T')
    #define NVM_ALARM                   NVM_DEFINE_KEY('A', 'L', 'R', 'M')
    #define NVM_REMINDER                NVM_DEFINE_KEY('R', 'E', 'M', 'D')

    #define NVM_ALARM_COUNT             (FLASH_NVM_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))
    #define NVM_REMINDER_COUNT          (FLASH_NVM_OBJECT_SIZE / sizeof(struct CLOCK_moment_t))

/***************************************************************************
 *  @def
 ***************************************************************************/
    #define time_to_mtime(ts)           ((int16_t)(((ts) / 3600) * 100 + ((ts) % 3600) / 60))
    #define mtime_to_time(mt)           (time_t)((((mt) / 100) * 3600 + ((mt) % 100) * 60))

    struct CLOCK_moment_t
    {
        bool enabled;
        int8_t wdays;
        bool snooze_once;

        union
        {
            uint8_t ringtone_id;
            uint8_t reminder_id;
        };

        int16_t mtime;  // 1700
        int32_t mdate;  // yyyy/mm/dd
    };

    struct CLOCK_setting_t
    {
        struct CLOCK_moment_t alarms[NVM_ALARM_COUNT];
        int8_t alarming_idx;
        time_t alarm_snooze_ts_end;

        struct CLOCK_moment_t reminders[NVM_ALARM_COUNT];
        time_t reminder_ts_end;
        time_t reminder_snooze_ts_end;
        struct timeout_t reminder_next_timeo;
    };

    struct SMARTCUCKOO_setting_t
    {
        struct
        {
            uint8_t volume;

            int16_t voice_id;
            int16_t noise_id;
        } media;

        struct SMARTCUCKOO_locale_t locale;
    };

/***************************************************************************
 *  @public
 ***************************************************************************/
    extern struct VOICE_attr_t voice_attr;

    extern struct CLOCK_setting_t clock_setting;
    extern struct SMARTCUCKOO_setting_t setting;

/***************************************************************************
 *  c++
 ***************************************************************************/
#ifdef __cplusplus
    #include <bluetooth/gap.tlv.hpp>
    #include <uc++/stream.hpp>

    extern void PERIPHERAL_write_tlv(TMemStream &scanrsp);
#endif

__BEGIN_DECLS
/***************************************************************************
 *  peripheral
 ***************************************************************************/
extern __attribute__((nothrow))
    void PERIPHERAL_gpio_init(void);
extern __attribute__((nothrow))
    void PERIPHERAL_shell_init(void);

extern __attribute__((nothrow))
    void PERIPHERAL_init(void);
extern __attribute__((nothrow))
    void PERIPHERAL_ota_init(void);

extern __attribute__((nothrow))
    uint8_t PERIPHERAL_sync_id(void);

extern __attribute__((nothrow))
    void PERIPHERAL_batt_ad_start(void);
extern __attribute__((nothrow))
    uint16_t PERIPHERAL_batt_ad_sync(void);
extern __attribute__((nothrow))
    uint16_t PERIPHERAL_batt_volt(void);

extern __attribute__((nothrow))
    void PERIPHERAL_adv_start(void);
extern __attribute__((nothrow))
    void PERIPHERAL_adv_stop(void);
extern __attribute__((nothrow))
    void PERIPHERAL_adv_update(void);

    // BLE event callbacks
extern __attribute__((nothrow))
    void PERIPHERAL_on_ready(void);
extern __attribute__((nothrow))
    void PERIPHERAL_on_connected(void);
extern __attribute__((nothrow))
    void PERIPHERAL_on_disconnect(void);
extern __attribute__((nothrow))
    void PERIPHERAL_on_sleep(void);
extern __attribute__((nothrow))
    void PERIPHERAL_on_wakeup(void);

/***************************************************************************
 *  RTC
 ***************************************************************************/
extern __attribute__((nothrow))
    void RTC_calibration_init(void);

/***************************************************************************
 *  CLOCK: alarms & reminders
 ***************************************************************************/
extern __attribute__((nothrow))
    void CLOCK_init(void);

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

    // return alarm switch on/off
extern __attribute__((nothrow, pure))
    bool CLOCK_alarm_switch_is_on(void);

    // return if now is alarming
extern __attribute__((nothrow, pure))
    bool CLOCK_is_alarming(void);

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

extern __attribute__((nothrow))
    void CLOCK_snooze_reminders(void);

/***************************************************************************
 *  shell
 ***************************************************************************/
extern __attribute__((nothrow))
    void SHELL_init(void);
extern __attribute__((nothrow))
    void SHELL_voice_say_reminders(void);

extern __attribute__((nothrow))
    void SHELL_bootstrap(void);

/***************************************************************************
 *  batt milli-voltage => level
 ***************************************************************************/
extern __attribute__((nothrow))
    uint8_t BATT_mv_level(uint32_t mV);

__END_DECLS
#endif
