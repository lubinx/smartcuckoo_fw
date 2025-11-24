#ifndef __SMARTCUCKOO_H
#define __SMARTCUCKOO_H                 1

#include <features.h>

#include <ultracore/log.h>
#include <ultracore/mq.h>
#include <ultracore/timeo.h>
#include <ultracore/nvm.h>
#include <ultracore/thread.h>
#include <ultracore/syscon.h>
#include <sh/ucsh.h>

#include <audio/renderer.h>
#include <audio/mplayer.h>
#include <audio/mynoise.h>

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stropts.h>

#include <pmu.h>
#include <rtc.h>
#include <uart.h>
#include <wdt.h>

#include "datetime_utils.h"
#include "locale.h"
#include "limits.h"

#include "clock.h"
#include "voice.h"

#include "N32X45X_config.h"
#include "PERIPHERAL_config.h"

/***************************************************************************
 *  @def: global consts
 ***************************************************************************/
#ifndef PERIPHERIAL_CONN_TIMEOUT
    #define PERIPHERIAL_CONN_TIMEOUT    (30000)
#endif

#ifndef SETTING_TIMEOUT
    #define SETTING_TIMEOUT             (6000)
#endif

#ifndef RTC_CALIBRATION_SECONDS
    #define RTC_CALIBRATION_SECONDS     (3)
#endif

/***************************************************************************
 *  @def: common PIN mux
 ***************************************************************************/
#define CONSOLE_DEV                     USART1
    #define CONSOLE_TXD                 PA09
    #define CONSOLE_RXD                 PA10

/***************************************************************************
 *  @def: NVM
 ***************************************************************************/
    #define NVM_SETTING                 NVM_DEFINE_KEY('S', 'E', 'T', 'T')

/***************************************************************************
 *  @def
 ***************************************************************************/
    struct SMARTCUCKOO_setting_t
    {
        uint8_t media_volume;
        uint8_t dim;
        bytebool_t alarm_is_on; // REVIEW: has no hardware switcher

        int16_t voice_sel_id;
        struct LOCALE_t locale;

        char pwr_noise[96];     // REVIEW: for shell pwr command
    };

/***************************************************************************
 *  @public
 ***************************************************************************/
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
extern __attribute__((nothrow, pure))
    bool PERIPHERAL_is_enable_usb(void);

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
