#ifndef __SMARTCUCKOO_PANEL_PRIVATE_H
#define __SMARTCUCKOO_PANEL_PRIVATE_H   1

#include <features.h>
#include <stdbool.h>
#include <stdlib.h>

#include <adc.h>
#include <pmu.h>

#include "smartcuckoo.h"
#include "panel_light_sens.h"
#include "panel_i2c_drv.h"

#include "irda.h"
#include "SHT4x.h"
#include "lamp.h"
#include "mplayer_noise.h"

#if defined(PANEL_B)
    #include "xl9535.h"
#elif defined(PANEL_C)
    #include "bs81xc.h"
#else
#endif

    enum PANEL_message_t
    {
        MSG_ALIVE                   = 1,

        MSG_SET_BLINKY,

        MSG_LIGHT_SENS,
        MSG_IRDA,

        MSG_BUTTON_SNOOZE,
        MSG_BUTTON_MESSAGE,

        MSG_KEY_SETTING,
        MSG_KEY_WHITE_NOISE,
        MSG_KEY_VOLUME_DEC,
        MSG_KEY_VOLUME_INC,
        MSG_KEY_LAMP_EN,
        MSG_KEY_LAMP,
        MSG_KEY_RECORD,
        MSG_KEY_UP,
        MSG_KEY_DOWN,
        MSG_KEY_LEFT,
        MSG_KEY_RIGHT,
        MSG_KEY_OK,

        // PANEL B
        MSG_KEY_ALM1,
        MSG_KEY_ALM2,
        MSG_KEY_USB,

        MSG_IOEXT               = 0x80,
    };

    enum PANEL_setting_group_t
    {
        SETTING_DATE_GROUP,
        SETTING_TIME_GROUP,
        SETTING_ALARM_1_GROUP,
        SETTING_ALARM_2_GROUP,
        SETTING_ALARM_3_GROUP,
        SETTING_ALARM_4_GROUP,
        SETTING_TMPR_UNIT_GROUP,

        SETTING_GROUP_MAX           = SETTING_TMPR_UNIT_GROUP,
    };

    enum PANEL_setting_part_t
    {
        SETTING_NONE,

        SETTING_YEAR,
        SETTING_MONTH,
        SETTING_DAY,

        SETTING_HOUR,
        SETTING_MINUTE,

        /*
        SETTING_A_YEAR,
        SETTING_A_MONTH,
        SETTING_A_DAY,
        */
        SETTING_A_HOUR,
        SETTING_A_MINUTE,
        SETTING_A_RINGTONE,

        SETTING_WDAY_MONDAY,
        SETTING_WDAY_THESDAY,
        SETTING_WDAY_WEDNESDAY,
        SETTING_WDAY_THURSDAY,
        SETTING_WDAY_FRIDAY,
        SETTING_WDAY_SATURDAY,
        SETTING_WDAY_SUNDAY,

        #define SETTING_WDAY_START      SETTING_WDAY_MONDAY
        #define SETTING_WDAY_END        SETTING_WDAY_SUNDAY
    };

    struct TOUCH_keymap_t
    {
        uint16_t key;
        enum PANEL_message_t msg_key;
    };

    struct PANEL_runtime_t
    {
        int mqd;
        int ioext_fd_devfd;
        int env_sensor_devfd;

        struct timeout_t schedule_intv;
        struct timeout_t gpio_repeat_intv;

        struct PANEL_attr_t panel_attr;
        struct PANEL_light_ad_attr_t light_sens;
        struct LAMP_attr_t lamp_attr;
        struct NOISE_attr_t noise_attr;
        struct IrDA_attr_t irda;

        struct
        {
            bool en;
            uint8_t alarm_tmp_idx;      // panel c alarm 1/2 button

            uint8_t level;
            bool is_modified;
            bool alarm_is_modified;

            enum PANEL_setting_group_t group;
            enum PANEL_setting_part_t part;
            clock_t activity;
        } setting;

        struct
        {
            int16_t tmpr;
            uint8_t humidity;

            time_t last_ts;
        } env_sensor;
    };

    extern struct PANEL_runtime_t panel;

__BEGIN_DECLS

static inline
    bool MSG_is_repeatable(enum PANEL_message_t msgid)
    {
        return (MSG_KEY_UP <= msgid && MSG_KEY_RIGHT >= msgid) ||
            MSG_KEY_VOLUME_INC == msgid ||
            MSG_KEY_VOLUME_DEC == msgid;
    }

static inline
    bool SETTING_part_is_wdays(enum PANEL_setting_part_t part)
    {
        return SETTING_WDAY_START <= part && SETTING_WDAY_END >= part;
    }

static inline
    bool SETTING_group_is_alarms(enum PANEL_setting_group_t group)
    {
        return SETTING_ALARM_1_GROUP <= group &&  SETTING_ALARM_4_GROUP >= group;
    }

static inline
    int ENV_sensor_createfd(void *const dev)
    {
        return SHT4X_open(dev, SHT4X_A_DA, 100);
    }

static inline
    int ENV_sensor_read(int env_sensor_fd, int16_t *tmpr, uint8_t *humidity)
    {
        return SHT4X_read(env_sensor_fd, tmpr, humidity);
    }

static inline
    int IOEXT_createfd(void *const dev)
    {
        #ifdef PANEL_C
            return BS81xC_createfd(dev, 100);
        #else
            return XL9535_createfd(dev, 100);
        #endif
    }

static inline
    int IOEXT_read_key(int ioext_fd, uint32_t *key)
    {
        #ifdef PANEL_C
            return BS81xC_read_key(ioext_fd, key);
        #else
            return XL9535_read_key(ioext_fd, key);
        #endif
    }

__END_DECLS
#endif
