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
#include "env.h"
#include "ioext.h"
#include "lamp.h"
#include "mplayer_noise.h"

    enum PANEL_message_t
    {
        MSG_ALIVE                   = 1,
        MSG_SET_BLINKY,

        MSG_IOEXT,
        MSG_LIGHT_SENS,
        MSG_IRDA,

        MSG_VOLUME_INC,
        MSG_VOLUME_DEC,

        MSG_BUTTON_SETTING,
        MSG_BUTTON_SNOOZE,
        MSG_BUTTON_MESSAGE,
        MSG_BUTTON_NOISE,
        MSG_BUTTON_LAMP_EN,
        MSG_BUTTON_LAMP,
        MSG_BUTTON_RECORD,

        MSG_BUTTON_UP,
        MSG_BUTTON_DOWN,
        MSG_BUTTON_LEFT,
        MSG_BUTTON_RIGHT,
        MSG_BUTTON_OK,

        // PANEL B
        MSG_BUTTON_ALM1,
        MSG_BUTTON_ALM2,
        MSG_BUTTON_MEDIA,

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

        SETTING_GROUP_MIN           = SETTING_DATE_GROUP,
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
        SETTING_HOUR_FORMT,

        /*
        SETTING_A_YEAR,
        SETTING_A_MONTH,
        SETTING_A_DAY,
        */
        SETTING_A_HOUR,
        SETTING_A_MINUTE,

        SETTING_WDAY_MONDAY,
        SETTING_WDAY_THESDAY,
        SETTING_WDAY_WEDNESDAY,
        SETTING_WDAY_THURSDAY,
        SETTING_WDAY_FRIDAY,
        SETTING_WDAY_SATURDAY,
        SETTING_WDAY_SUNDAY,
        #define SETTING_WDAY_START      SETTING_WDAY_MONDAY
        #define SETTING_WDAY_END        SETTING_WDAY_SUNDAY

        SETTING_A_RINGTONE,
    };

    struct TOUCH_keymap_t
    {
        uint16_t key;
        enum PANEL_message_t msg_key;
    };

    struct PANEL_runtime_t
    {
        int mqd;
        int mp3_uartfd;
        int ioext_fd_devfd;
        int env_sensor_devfd;

        struct timeout_t gpio_repeat_intv;
        struct timeout_t schedule_intv;

        struct PANEL_attr_t panel_attr;
        struct PANEL_light_ad_attr_t light_sens;
        struct LAMP_attr_t lamp_attr;
        struct IrDA_attr_t irda;

        struct
        {
            bool en;
            clock_t tick;
            uint8_t alarm_tmp_idx;      // panel c alarm 1/2 button

            uint8_t level;
            bool is_modified;
            bool alarm_is_modified;

            enum PANEL_setting_group_t group;
            enum PANEL_setting_part_t part;
        } setting;

        struct
        {
            int16_t tmpr;
            uint8_t humidity;
            bytebool_t converting;

            time_t last_ts;
        } env_sensor;

        struct
        {
            bool en;
            timeout_t disable_timeo;
            enum PANEL_setting_group_t group;
        } tmp_content;
    };

    extern struct PANEL_runtime_t panel;

__BEGIN_DECLS

static inline
    bool MSG_is_repeatable(enum PANEL_message_t msgid)
    {
        return (MSG_BUTTON_UP <= msgid && MSG_BUTTON_RIGHT >= msgid) ||
            MSG_VOLUME_INC == msgid ||
            MSG_VOLUME_DEC == msgid;
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

extern __attribute__((nothrow))
    void SETTING_defer_save(struct PANEL_runtime_t *);

__END_DECLS
#endif
