#ifndef __PANEL_DRV_H
#define __PANEL_DRV_H

#include <features.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "locale.h"

    enum __attribute__((packed)) PANEL_part
    {
        // datetime
        PANEL_YEAR              = 1U << 0,
        PANEL_MONTH             = 1U << 1,
        PANEL_DAY               = 1U << 2,
        PANEL_HOUR              = 1U << 3,
        PANEL_MINUTE            = 1U << 4,
        // alarms
        PANEL_IND_1             = 1U << 6,
        PANEL_IND_2             = 1U << 7,
        PANEL_IND_3             = 1U << 8,
        PANEL_IND_4             = 1U << 9,
        // week days
        PANEL_WDAY_SUNDAY       = 1U << 10,
        PANEL_WDAY_MONDAY       = 1U << 11,
        PANEL_WDAY_THESDAY      = 1U << 12,
        PANEL_WDAY_WEDNESDAY    = 1U << 13,
        PANEL_WDAY_THURSDAY     = 1U << 14,
        PANEL_WDAY_FRIDAY       = 1U << 15,
        PANEL_WDAY_SATURDAY     = 1U << 16,
        // env sensors
        PANEL_TMPR_UNIT         = 1U << 17,
        PANEL_TMPR              = 1U << 18,
        PANEL_HUMIDITY          = 1U << 19,

        // date group
        #define PANEL_DATE      (PANEL_YEAR | PANEL_MONTH | PANEL_DAY)
        // time group
        #define PANEL_TIME      (PANEL_HOUR | PANEL_MINUTE)
        // alarams group
        #define PANEL_INDICATES (PANEL_IND_1 | PANEL_IND_2 | PANEL_IND_3 | PANEL_IND_4)
        // wdays group
        #define PANEL_WDAYS     (PANEL_WDAY_SUNDAY | PANEL_WDAY_MONDAY | PANEL_WDAY_THESDAY |   \
            PANEL_WDAY_WEDNESDAY | PANEL_WDAY_THURSDAY | PANEL_WDAY_FRIDAY | PANEL_WDAY_SATURDAY)
    };

    struct PANEL_attr_t
    {
        int da0_fd;
        int da1_fd;

        uint32_t disable_parts;
        uint32_t blinky_parts;
        uint32_t blinky_mask;

        struct SMARTCUCKOO_locale_t const *locale;
        int16_t std_tmpr;

        struct
        {
            int16_t mtime;

            int16_t year;
            int8_t month;
            int8_t mday;
            int8_t wdays;
            uint8_t alarms;

            int8_t humidity;
            int16_t tmpr;

            uint32_t flags;
        } cache, set;
    };

    struct PANEL_implement
    {
        uint8_t (* PANEL_pwm_max)(struct PANEL_attr_t *attr);
        int (* PANEL_update)(struct PANEL_attr_t *attr, struct tm *dt);
    };

__BEGIN_DECLS

extern __attribute__((nothrow))
    void PANEL_attr_init(struct PANEL_attr_t *attr, void *i2c_dev, struct SMARTCUCKOO_locale_t const *locale);

extern __attribute__((nothrow))
    void PANEL_attr_set_blinky(struct PANEL_attr_t *attr, uint32_t parts);
extern __attribute__((nothrow))
    void PANEL_attr_set_disable(struct PANEL_attr_t *attr, uint32_t parts);
extern __attribute__((nothrow))
    void PANEL_attr_set_hfmt(struct PANEL_attr_t *attr, enum LOCALE_hfmt_t hfmt);

extern __attribute__((nothrow))
    void PANEL_attr_set_datetime(struct PANEL_attr_t *attr, time_t ts);
extern __attribute__((nothrow))
    void PANEL_attr_set_flags(struct PANEL_attr_t *attr, uint32_t flags);
extern __attribute__((nothrow))
    void PANEL_attr_unset_flags(struct PANEL_attr_t *attr, uint32_t flags);

extern __attribute__((nothrow))
    void PANEL_attr_set_mdate(struct PANEL_attr_t *attr, int32_t mdate);
extern __attribute__((nothrow))
    void PANEL_attr_set_mtime(struct PANEL_attr_t *attr, int16_t mtime);
extern __attribute__((nothrow))
    void PANEL_attr_set_wdays(struct PANEL_attr_t *attr, int8_t wdays);
extern __attribute__((nothrow))
    void PANEL_attr_set_ringtone_id(struct PANEL_attr_t *attr, int16_t id);

extern __attribute__((nothrow))
    void PANEL_attr_set_tmpr(struct PANEL_attr_t *attr, int16_t tmpr);
extern __attribute__((nothrow))
    void PANEL_attr_set_humidity(struct PANEL_attr_t *attr, int8_t humidity);

extern __attribute__((nothrow))
    int PANEL_update(struct PANEL_attr_t *attr);

extern __attribute__((nothrow))
    void PANEL_set_dim(struct PANEL_attr_t *attr, uint8_t percent, uint8_t dyn_percent);

/****************************************************************************
 *  @CHIP impl
 ****************************************************************************/
extern __attribute__((nothrow))
    void PANEL_HAL_init(struct PANEL_attr_t *attr, void *i2c_dev);

extern __attribute__((nothrow))
    int PANEL_HAL_write(int fd, void const *buf, size_t count);

__END_DECLS
#endif
