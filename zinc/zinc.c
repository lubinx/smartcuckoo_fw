#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (4)
#define MQUEUE_LENGTH                   (8)

#define MQUEUE_ALIVE_INTV_SECONDS       (5)

enum zinc_message_t
{
    MSG_NOISE_BUTTON            = 0x11,
    MSG_VOLUME_UP_BUTTON,
    MSG_VOLUME_DOWN_BUTTON,
    MSG_SNOOZE_BUTTON           = 0x21,
    MSG_PREV_BUTTON,
    MSG_NEXT_BUTTON,
    MSG_TIMER_BUTTON            = 0x31,
    MSG_LAMP_BUTTON,
    MSG_DIMMER_BUTTON,
};

struct zinc_runtime_t
{
    int mqd;
    struct SMART_LED_attr_t led_time;
    struct SMART_LED_attr_t led_date;

    timeout_t gpio_filter_timeo;
    timeout_t setting_timeo;
    timeout_t volume_adj_intv;

    enum zinc_message_t gpio_button;
    bytebool_t setting;
    bytebool_t setting_is_modified;
    bytebool_t setting_alarm_is_modified;

    enum VOICE_setting_t setting_part;
    struct tm setting_dt;

    clock_t voice_last_tick;
    time_t batt_last_ts;
};

/****************************************************************************
 *  @private
 ****************************************************************************/
static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zinc_runtime_t *runtime);

static void GPIO_button_callback(uint32_t pins, struct zinc_runtime_t *runtime);
static void GPIO_button_filter_callback(enum zinc_message_t msg_button);
static bool GPIO_button_is_down(enum zinc_message_t msg_button);

static void SETTING_volume_adj_intv(enum zinc_message_t msg_button);
static void SETTING_timeout_save(struct zinc_runtime_t *runtime);

static void MYNOISE_power_off_tickdown_callback(uint32_t power_off_seconds_remain);

// var
static struct zinc_runtime_t zinc = {0};
__THREAD_STACK static uint32_t zinc_stack[1280 / sizeof(uint32_t)];

/****************************************************************************
 *  @implements
 ****************************************************************************/
void SMART_LED_HAL_ext_configure(struct SMART_LED_attr_t *attr)
{
    GPIO_output_alt_speed(attr->pin_alias, GPIO_SPEED_10M);

    GPIO_Module *gpio = GPIO_port_of(&attr->pin_alias);
    attr->DOUT_REG = &gpio->POD;
}

void CLOCK_update_display_callback(struct tm const *dt)
{
    uint64_t mask = 0;
    uint8_t val;

    val = (uint8_t)(dt->tm_min % 10);
    mask |= (uint64_t)SMART_LED_digit_mapping[val];

    mask <<= 7;
    val = (uint8_t)(dt->tm_min / 10);
    mask |= (uint64_t)SMART_LED_digit_mapping[val];

    mask <<= 2;
    mask |= 0x03;

    mask <<= 7;
    val = (uint8_t)(dt->tm_hour % 10);
    mask |= (uint64_t)SMART_LED_digit_mapping[val];

    mask <<= 7;
    val = (uint8_t)(dt->tm_hour / 10);
    mask |= (uint64_t)SMART_LED_digit_mapping[val];

    SMART_LED_update(&zinc.led_time, mask);
}

void PERIPHERAL_gpio_init(void)
{
    SMART_LED_init(&zinc.led_time, LED_TIME_DAT, 30);
    SMART_LED_set_color(&zinc.led_time, LED_PINK);
    SMART_LED_update(&zinc.led_time, 0);

    GPIO_setdir_output(PUSH_PULL_UP, PIN_ROW_1);
    GPIO_setdir_output(PUSH_PULL_UP, PIN_ROW_2);
    GPIO_setdir_output(PUSH_PULL_UP, PIN_ROW_3);

    GPIO_setdir_input_pp(PULL_DOWN, PIN_COL_1, true);
    GPIO_setdir_input_pp(PULL_DOWN, PIN_COL_2, true);
    GPIO_setdir_input_pp(PULL_DOWN, PIN_COL_3, true);
}

void PERIPHERAL_gpio_intr_enable(void)
{
    GPIO_intr_enable(PIN_COL_1, TRIG_BY_RISING_EDGE, (void *)GPIO_button_callback, &zinc);
    GPIO_intr_enable(PIN_COL_2, TRIG_BY_RISING_EDGE, (void *)GPIO_button_callback, &zinc);
    GPIO_intr_enable(PIN_COL_3, TRIG_BY_RISING_EDGE, (void *)GPIO_button_callback, &zinc);
}

bool PERIPHERAL_is_enable_usb(void)
{
    return false;
}

void PERIPHERAL_ota_init(void)
{

}

void PERIPHERAL_init(void)
{
    timeout_init(&zinc.gpio_filter_timeo, GPIO_FILTER_INTV, (void *)GPIO_button_filter_callback, 0);
    timeout_init(&zinc.volume_adj_intv, VOLUME_ADJ_HOLD_INTV, (void *)SETTING_volume_adj_intv, 0);
    timeout_init(&zinc.setting_timeo, SETTING_TIMEOUT, (void *)SETTING_timeout_save, 0);

    zinc.voice_last_tick = (clock_t)-SETTING_TIMEOUT;
    zinc.batt_last_ts = time(NULL);

    // load settings
    if (0 != NVM_get(NVM_SETTING, sizeof(smartcuckoo), &smartcuckoo))
    {
        memset(&smartcuckoo, 0, sizeof(smartcuckoo));

        smartcuckoo.alarm_is_on = true;
        smartcuckoo.volume = 30;
    }

    smartcuckoo.volume = MIN(50, smartcuckoo.volume);
    AUDIO_set_volume_percent(smartcuckoo.volume);
    AUDIO_renderer_supress_master_value(80);

    smartcuckoo.voice_sel_id = VOICE_init(smartcuckoo.voice_sel_id, &smartcuckoo.locale);

    MQUEUE_INIT(&zinc.mqd, MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
    PERIPHERAL_gpio_intr_enable();

    if (true)
    {
        uint32_t timeout = 1000 * MQUEUE_ALIVE_INTV_SECONDS;
        ioctl(zinc.mqd, OPT_RD_TIMEO, &timeout);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstack(&attr, zinc_stack, sizeof(zinc_stack));

        pthread_t id;
        pthread_create(&id, &attr, (void *)MSG_dispatch_thread, &zinc);
        pthread_attr_destroy(&attr);
    }

    if (1)
    {
        GPIO_setdir_input_pp(PULL_DOWN, PIN_RTC_CAL_IN, true);
        clock_t ts = clock();

        while (1)
        {
            if (GPIO_peek(PIN_RTC_CAL_IN))
            {
                LOG_debug("calibration");
                VOICE_say_setting(VOICE_SETTING_DONE);
                mplayer_waitfor_idle();

                if (0 == RTC_calibration_ppb(PIN_RTC_CAL_IN))
                    VOICE_say_setting(VOICE_SETTING_DONE);
                break;
            }

            if (600 > clock() - ts)
            {
                pthread_yield();
                WDOG_feed();
            }
            else
                break;
        }

        GPIO_disable(PIN_RTC_CAL_IN);
    }

    MYNOISE_init();
    MYNOISE_power_off_tickdown_cb(MYNOISE_power_off_tickdown_callback);
}

/****************************************************************************
 *  @implements: overrides
 ****************************************************************************/
void mplayer_idle_callback(void)
{
    if (zinc.setting)
        timeout_start(&zinc.setting_timeo, &zinc);
    else
        CLOCK_schedule();
}

static void MYNOISE_power_off_tickdown_callback(uint32_t power_off_seconds_remain)
{
    (void)power_off_seconds_remain;
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct zinc_runtime_t *runtime)
{
    timeout_stop(&runtime->setting_timeo);
    enum zinc_message_t msg_button;

    GPIO_intr_disable(PIN_COL_1);
    GPIO_intr_disable(PIN_COL_2);
    GPIO_intr_disable(PIN_COL_3);

    if (PIN_COL_1 == (PIN_COL_1 & pins))
    {
        GPIO_clear(PIN_ROW_1);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_1))
        {
            msg_button = MSG_NOISE_BUTTON;
            goto gpio_send_filter;
        }

        GPIO_clear(PIN_ROW_2);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_1))
        {
            msg_button = MSG_VOLUME_UP_BUTTON;
            goto gpio_send_filter;
        }

        GPIO_clear(PIN_ROW_3);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_1))
        {
            msg_button = MSG_VOLUME_DOWN_BUTTON;
            goto gpio_send_filter;
        }
    }

    if (PIN_COL_2 == (PIN_COL_2 & pins))
    {
        GPIO_clear(PIN_ROW_1);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_2))
        {
            msg_button = MSG_SNOOZE_BUTTON;
            goto gpio_send_filter;
        }

        GPIO_clear(PIN_ROW_2);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_2))
        {
            msg_button = MSG_PREV_BUTTON;
            goto gpio_send_filter;
        }

        GPIO_clear(PIN_ROW_3);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_2))
        {
            msg_button = MSG_NEXT_BUTTON;
            goto gpio_send_filter;
        }
    }

    if (PIN_COL_3 == (PIN_COL_3 & pins))
    {
        GPIO_clear(PIN_ROW_1);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_3))
        {
            msg_button = MSG_TIMER_BUTTON;
            goto gpio_send_filter;
        }

        GPIO_clear(PIN_ROW_2);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_3))
        {
            msg_button = MSG_LAMP_BUTTON;
            goto gpio_send_filter;
        }

        GPIO_clear(PIN_ROW_3);  usleep(1);
        if (0 == GPIO_peek(PIN_COL_3))
        {
            msg_button = MSG_DIMMER_BUTTON;
            goto gpio_send_filter;
        }
    }

    if (0)
    {
    gpio_send_filter:
        timeout_start(&runtime->gpio_filter_timeo, (void *)msg_button);
    }
    else
        PERIPHERAL_gpio_intr_enable();

    GPIO_set(PIN_ROW_1);
    GPIO_set(PIN_ROW_2);
    GPIO_set(PIN_ROW_3);
}

static void GPIO_button_filter_callback(enum zinc_message_t msg_button)
{
    if (GPIO_button_is_down(msg_button))
        mqueue_postv(zinc.mqd, msg_button, 0, 0);
}

static bool GPIO_button_is_down(enum zinc_message_t msg_button)
{
    uint32_t pin_column;
    bool is_down;

    switch ((int)(msg_button >> 4))
    {
    default:
        // __BREAK_IFDBG();
        return false;

    case 1:
        pin_column = PIN_COL_1;
        break;
    case 2:
        pin_column = PIN_COL_2;
        break;
    case 3:
        pin_column = PIN_COL_3;
        break;
    }

    if (0 != GPIO_peek(pin_column))
    {
        GPIO_intr_disable(pin_column);

        switch (0x0F & msg_button)
        {
        default:
            is_down = false;
            break;

        case 1:
            GPIO_clear(PIN_ROW_1);  usleep(1);
            is_down = 0 == GPIO_peek(pin_column);
            GPIO_set(PIN_ROW_1);    usleep(1);
            break;

        case 2:
            GPIO_clear(PIN_ROW_2);  usleep(1);
            is_down = 0 == GPIO_peek(pin_column);
            GPIO_set(PIN_ROW_2);    usleep(1);
            break;

        case 3:
            GPIO_clear(PIN_ROW_3);  usleep(1);
            is_down = 0 == GPIO_peek(pin_column);
            GPIO_set(PIN_ROW_3);    usleep(1);
            break;
        }
    }
    else
        is_down = false;

    PERIPHERAL_gpio_intr_enable();
    return is_down;
}

static void SETTING_timeout_save(struct zinc_runtime_t *runtime)
{
    if (runtime->setting)
    {
        VOICE_say_setting(VOICE_SETTING_DONE);
        runtime->setting = false;
    }

    if (runtime->setting_alarm_is_modified)
        CLOCK_update_alarms();
    if (runtime->setting_is_modified)
        NVM_set(NVM_SETTING, sizeof(smartcuckoo), &smartcuckoo);
}

static void SETTING_volume_adj_intv(enum zinc_message_t msg_button)
{
    static clock_t tick = 0;
    timeout_stop(&zinc.setting_timeo);

    if (GPIO_button_is_down(msg_button))
    {
        mqueue_postv(zinc.mqd, msg_button, 0, 0);

        if (0 == tick)
            tick = clock();

        if (500 < clock() - tick)
        {
            tick = clock();
    volume_notification_2:
            char buf[16];
            SHELL_notification(buf, (unsigned)sprintf(buf, "volume: %d\n", AUDIO_get_volume_percent()));
        }
    }
    else
    {
        tick = 0;
        timeout_stop(&zinc.volume_adj_intv);

        zinc.setting_is_modified = true;
        smartcuckoo.volume = AUDIO_get_volume_percent();
        timeout_start(&zinc.setting_timeo, &zinc);

        goto volume_notification_2;
    }
}

/****************************************************************************
 *  @private: message thread & loops
 ****************************************************************************/
static void MSG_alive(struct zinc_runtime_t *runtime)
{
    /*
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
    {
        runtime->batt_last_ts = time(NULL);
        PERIPHERAL_batt_ad_sync();

        if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        {
            if (MYNOISE_is_running())
            {
                MYNOISE_stop();
                VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);
            }
        }
    }
    else
    */
    {
        if (! runtime->setting)
            CLOCK_schedule();

        if (BATT_AD_INTV_SECONDS < CLOCK_get_timestamp() - runtime->batt_last_ts)
        {
            runtime->batt_last_ts = CLOCK_get_timestamp();
            PERIPHERAL_batt_ad_start();
        }
    }
}

static void MSG_setting(struct zinc_runtime_t *runtime, uint32_t button)
{
    PMU_power_lock();
    mplayer_playlist_clear();

    if (MSG_PREV_BUTTON == button || MSG_NEXT_BUTTON == button)
    {
        if (MSG_PREV_BUTTON == button)
            runtime->setting_part = VOICE_prev_setting(runtime->setting_part);
        else
            runtime->setting_part = VOICE_next_setting(runtime->setting_part);

        struct CLOCK_moment_t *alarm0;
        alarm0 = CLOCK_get_alarm(0);

        if (VOICE_SETTING_ALARM_HOUR == runtime->setting_part ||
            VOICE_SETTING_ALARM_MIN == runtime->setting_part)
        {
            time_t ts = mtime2time(alarm0->mtime);

            localtime_r(&ts, &runtime->setting_dt);
            runtime->setting_dt.tm_sec = 0;
        }
        else
        {
            time_t ts = time(NULL);

            localtime_r(&ts, &runtime->setting_dt);
            runtime->setting_dt.tm_sec = 0;
        }

        VOICE_say_setting(runtime->setting_part);
        VOICE_say_setting_part(runtime->setting_part, &runtime->setting_dt, alarm0->ringtone_id);
    }
    else if (MSG_VOLUME_UP_BUTTON == button || MSG_VOLUME_DOWN_BUTTON == button)
    {
        int16_t old_voice_id;
        struct CLOCK_moment_t *alarm0 = CLOCK_get_alarm(0);

        switch (runtime->setting_part)
        {
        case VOICE_SETTING_EXT_LOW_BATT:
        case VOICE_SETTING_EXT_ALARM_ON:
        case VOICE_SETTING_EXT_ALARM_OFF:
            break;

        case VOICE_SETTING_LANG:
            old_voice_id = smartcuckoo.voice_sel_id;
            if (MSG_VOLUME_UP_BUTTON == button)
                smartcuckoo.voice_sel_id = VOICE_next_locale();
            else
                smartcuckoo.voice_sel_id = VOICE_prev_locale();

            if (old_voice_id != smartcuckoo.voice_sel_id)
            {
                smartcuckoo.locale.dfmt = DFMT_DEFAULT;
                smartcuckoo.locale.hfmt = HFMT_DEFAULT;
                runtime->setting_is_modified = true;
            }
            break;

        case VOICE_SETTING_VOICE:
            old_voice_id = smartcuckoo.voice_sel_id;
            if (MSG_VOLUME_UP_BUTTON == button)
                smartcuckoo.voice_sel_id = VOICE_next_voice();
            else
                smartcuckoo.voice_sel_id = VOICE_prev_voice();

            if (old_voice_id != smartcuckoo.voice_sel_id)
            {
                smartcuckoo.locale.dfmt = DFMT_DEFAULT;
                smartcuckoo.locale.hfmt = HFMT_DEFAULT;
                runtime->setting_is_modified = true;
            }
            break;

        case VOICE_SETTING_HOUR:
            if (MSG_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            else
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 23) % 24;
            goto setting_rtc_set_time;

        case VOICE_SETTING_MINUTE:
            if (MSG_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            else
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 59) % 60;
            goto setting_rtc_set_time;

        case VOICE_SETTING_YEAR:
            if (MSG_VOLUME_UP_BUTTON == button)
                TM_year_add(&runtime->setting_dt, 1);
            else
                TM_year_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MONTH:
            if (MSG_VOLUME_UP_BUTTON == button)
                TM_month_add(&runtime->setting_dt, 1);
            else
                TM_month_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MDAY:
            if (MSG_VOLUME_UP_BUTTON == button)
                TM_mday_add(&runtime->setting_dt, 1);
            else
                TM_mday_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_ALARM_HOUR:
            if (MSG_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            else
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 23) % 24;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_MIN:
            if (MSG_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            else
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 59) % 60;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_RINGTONE:
            if (MSG_VOLUME_UP_BUTTON == button)
                alarm0->ringtone_id = (uint8_t)VOICE_next_ringtone(alarm0->ringtone_id);
            else
                alarm0->ringtone_id = (uint8_t)VOICE_prev_ringtone(alarm0->ringtone_id);

            runtime->setting_alarm_is_modified = true;
            break;

        case VOICE_SETTING_COUNT:
            break;
        };

        if (false)
        {
        setting_rtc_set_time:
            runtime->setting_dt.tm_sec = 0;
            RTC_set_epoch_time(mktime(&runtime->setting_dt) - get_dst_offset(&runtime->setting_dt));

            LOG_info("%02d:%02d:%02d", runtime->setting_dt.tm_hour,
                runtime->setting_dt.tm_min,
                runtime->setting_dt.tm_sec);
        }

        if (false)
        {
            struct tm date;

        setting_rtc_set_date:
            date = runtime->setting_dt;
            date.tm_hour = 0;
            date.tm_min = 0;
            date.tm_sec = 0;
            RTC_set_epoch_time(time(NULL) % 86400 + mktime(&date));

            LOG_info("%04d/%02d/%02d", runtime->setting_dt.tm_year + 1900,
                runtime->setting_dt.tm_mon + 1,
                runtime->setting_dt.tm_mday);
        }

        if (false)
        {
            time_t ts;
            int16_t mtime;

        setting_modify_alarm:
            ts = mktime(&runtime->setting_dt);
            mtime = time2mtime(ts);

            if (! alarm0->enabled || mtime != alarm0->mtime)
            {
                alarm0->enabled = true;
                alarm0->mtime = mtime;
                alarm0->mdate = 0;
                alarm0->wdays = 0x7F;
                runtime->setting_alarm_is_modified = true;
            }
        }

        mplayer_playlist_clear();
        VOICE_say_setting_part(runtime->setting_part, &runtime->setting_dt, alarm0->ringtone_id);
    }

    PMU_power_unlock();
}

/*
static void MSG_voice_button(struct zinc_runtime_t *runtime)
{
    PMU_power_lock();
    mplayer_playlist_clear();
    CLOCK_dismiss();

    // insert say low battery
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);

    if (! runtime->setting)
    {
        struct tm const *dt = CLOCK_update_timestamp(NULL);

        if (SETTING_TIMEOUT < clock() - runtime->voice_last_tick)
        {
            runtime->voice_last_tick = clock();

            VOICE_say_time(dt);
            CLOCK_say_reminders(dt, true);
        }
        else
        {
            runtime->voice_last_tick -= SETTING_TIMEOUT;
            VOICE_say_date(dt);
        }
    }

    PMU_power_unlock();
}
*/

static void MSG_mynoise(enum zinc_message_t msg_button)
{
    int err = 0;

    switch (msg_button)
    {
    default:
        break;

    case MSG_NOISE_BUTTON:
        if (MYNOISE_is_running())
            err = MYNOISE_nextdir();
        else
            err = MYNOISE_start();
        break;

    case MSG_NEXT_BUTTON:
        if (MYNOISE_is_running())
            err = MYNOISE_next_indir();
        break;

    case MSG_PREV_BUTTON:
        if (MYNOISE_is_running())
            err = MYNOISE_prev_indir();
        break;
    }

    if (0 != err)
        LOG_error("%s", AUDIO_strerror(err));
}

static void MSG_volume(struct zinc_runtime_t *runtime, enum zinc_message_t msg_button)
{
    if (AUDIO_renderer_is_idle())
        VOICE_play_ringtone(CLOCK_get_ringtone_id());

    if (MSG_VOLUME_UP_BUTTON == msg_button)
        AUDIO_inc_volume(VOLUME_MAX_PERCENT);
    else
        AUDIO_dec_volume(VOLUME_MIN_PERCENT);

    LOG_info("volume: %d", AUDIO_get_volume_percent());
    timeout_start(&runtime->volume_adj_intv, (void *)msg_button);
}

static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zinc_runtime_t *runtime)
{
    while (true)
    {
        struct MQ_message_t *msg = mqueue_recv(runtime->mqd);
        WDOG_feed();

        if (msg)
        {
            if (runtime->setting)
            {
                switch ((enum zinc_message_t)msg->msgid)
                {
                case MSG_SNOOZE_BUTTON:
                case MSG_NOISE_BUTTON:
                case MSG_TIMER_BUTTON:
                case MSG_LAMP_BUTTON:
                case MSG_DIMMER_BUTTON:
                    break;

                case MSG_PREV_BUTTON:
                    MSG_setting(runtime, MSG_PREV_BUTTON);
                    break;

                case MSG_NEXT_BUTTON:
                    MSG_setting(runtime, MSG_NEXT_BUTTON);
                    break;

                case MSG_VOLUME_UP_BUTTON:
                    MSG_setting(runtime, MSG_VOLUME_UP_BUTTON);
                    break;

                case MSG_VOLUME_DOWN_BUTTON:
                    MSG_setting(runtime, MSG_VOLUME_DOWN_BUTTON);
                    break;
                }
            }
            else
            {
                switch ((enum zinc_message_t)msg->msgid)
                {
                case MSG_SNOOZE_BUTTON:
                    break;

                case MSG_NOISE_BUTTON:
                case MSG_PREV_BUTTON:
                case MSG_NEXT_BUTTON:
                    MSG_mynoise((enum zinc_message_t)msg->msgid);
                    break;

                case MSG_VOLUME_UP_BUTTON:
                case MSG_VOLUME_DOWN_BUTTON:
                    MSG_volume(runtime, (enum zinc_message_t)msg->msgid);
                    break;

                case MSG_TIMER_BUTTON:
                    break;

                case MSG_LAMP_BUTTON:
                    {
                        SMART_LED_next_color(&runtime->led_time);
                        time_t ts = time(NULL);
                        CLOCK_update_display_callback(localtime(&ts));
                    }
                    break;

                case MSG_DIMMER_BUTTON:
                    break;
                }
            }

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
            MSG_alive(runtime);
    }
}
