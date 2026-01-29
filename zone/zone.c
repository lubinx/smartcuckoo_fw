#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (0)
#define MQUEUE_LENGTH                   (8)

#define MQUEUE_ALIVE_INTV               (2500)

enum zone_message_t
{
    MSG_TOP_BUTTON              = 1,
    MSG_POWER_BUTTON,
    MSG_PREV_BUTTON,
    MSG_NEXT_BUTTON,
    MSG_VOLUME_UP_BUTTON,
    MSG_VOLUME_DOWN_BUTTON,
};

struct zone_runtime_t
{
    int mqd;

    timeout_t setting_timeo;
    timeout_t setting_volume_intv;

    bytebool_t power_is_down;
    bytebool_t setting;
    bytebool_t setting_is_modified;
    bytebool_t setting_alarm_is_modified;

    enum VOICE_setting_t setting_part;
    struct tm setting_dt;

    clock_t voice_last_tick;
    clock_t top_button_stick;
    clock_t power_button_stick;
    clock_t prev_next_button_stick;

    time_t batt_last_ts;
};

/****************************************************************************
 *  @private
 ****************************************************************************/
static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zone_runtime_t *runtime);

static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *runtime);
static void SETTING_timeout_callback(struct zone_runtime_t *runtime);
static void SETTING_volume_intv_callback(enum zone_message_t);

static void MYNOISE_power_off_tickdown_callback(uint32_t power_off_seconds_remain, bool stopping);

// var
static struct zone_runtime_t zone = {0};
__THREAD_STACK static uint32_t zone_stack[1280 / sizeof(uint32_t)];

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PERIPHERAL_gpio_init(void)
{
    GPIO_setdir_output(PUSH_PULL_UP, LED_POWER);
    GPIO_setdir_output(PUSH_PULL_UP, LED1);
    GPIO_setdir_output(PUSH_PULL_UP, LED2);
    GPIO_setdir_output(PUSH_PULL_UP, LED3);

    GPIO_setdir_input_pp(PULL_UP, PIN_TOP_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_POWER_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_PREV_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_NEXT_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_VOLUME_UP_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_VOLUME_DOWN_BUTTON, true);
}

void PERIPHERAL_gpio_intr_enable(void)
{
    GPIO_intr_enable(PIN_TOP_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_POWER_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_PREV_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_NEXT_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_VOLUME_UP_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_VOLUME_DOWN_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
}

bool PERIPHERAL_is_enable_usb(void)
{
#ifdef DEBUG
    return true;
#else
    return 0 == GPIO_peek(PIN_POWER_BUTTON);
#endif
}

void PERIPHERAL_ota_init(void)
{
    SHELL_notification_enable(false);
    mplayer_stop();
    MYNOISE_stop();

    GPIO_disable(PIN_TOP_BUTTON);
    GPIO_disable(PIN_POWER_BUTTON);
    GPIO_disable(PIN_PREV_BUTTON);
    GPIO_disable(PIN_NEXT_BUTTON);
    GPIO_disable(PIN_VOLUME_UP_BUTTON);
    GPIO_disable(PIN_VOLUME_DOWN_BUTTON);
}

void PERIPHERAL_init(void)
{
    timeout_init(&zone.setting_volume_intv, SETTING_VOLUME_ADJ_INTV, (void *)SETTING_volume_intv_callback, 0);
    timeout_init(&zone.setting_timeo, SETTING_TIMEOUT, (void *)SETTING_timeout_callback, 0);

    zone.voice_last_tick = (clock_t)-SETTING_TIMEOUT;
    zone.batt_last_ts = time(NULL);

    // load settings
    if (0 != NVM_get(NVM_SETTING, sizeof(smartcuckoo), &smartcuckoo))
    {
        memset(&smartcuckoo, 0, sizeof(smartcuckoo));

        if (0 != NVM_get(NVM_SETTING, offsetof(struct SMARTCUCKOO_t, lamp), &smartcuckoo))
        {
            smartcuckoo.alarm_is_on = true;
            smartcuckoo.volume = 30;
        }
    }

    smartcuckoo.volume = MAX(VOLUME_MIN_PERCENT, smartcuckoo.volume);
    AUDIO_renderer_set_volume_percent(smartcuckoo.volume);
    AUDIO_renderer_master_volume_balance(5, 25);

    smartcuckoo.voice_sel_id = VOICE_init(smartcuckoo.voice_sel_id, &smartcuckoo.locale);

    MQUEUE_INIT(&zone.mqd, MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
    PERIPHERAL_gpio_intr_enable();

    if (true)
    {
        uint32_t timeout = MQUEUE_ALIVE_INTV;
        ioctl(zone.mqd, OPT_RD_TIMEO, &timeout);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstack(&attr, zone_stack, sizeof(zone_stack));

        pthread_t id;
        pthread_create(&id, &attr, (void *)MSG_dispatch_thread, &zone);
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
    MYNOISE_power_off_set_tickdown_cb(MYNOISE_power_off_tickdown_callback);
}

/****************************************************************************
 *  @implements: overrides
 ****************************************************************************/
int CLOCK_alarm_switch(bool en)
{
    smartcuckoo.alarm_is_on = en;
    return NVM_set(NVM_SETTING, sizeof(smartcuckoo), &smartcuckoo);
}

void mplayer_idle_callback(void)
{
    if (zone.setting)
        timeout_start(&zone.setting_timeo, &zone);
    else
        CLOCK_schedule();
}

static void MYNOISE_power_off_tickdown_callback(uint32_t power_off_seconds_remain, bool stopping)
{
    GPIO_set(LED1);
    GPIO_set(LED2);
    GPIO_set(LED3);

    if (0 != power_off_seconds_remain)
    {
        GPIO_clear(LED1);

        if (POWER_OFF_STEP_SECONDS < power_off_seconds_remain)
            GPIO_clear(LED2);
        if (2U * POWER_OFF_STEP_SECONDS < power_off_seconds_remain)
            GPIO_clear(LED3);
    }

    if (stopping)
    {
        struct CLOCK_moment_t *alarm = CLOCK_get_current_alarm();

        if (CLOCK_get_alarm_is_app_specify(alarm))
            CLOCK_dismiss();

        GPIO_set(LED_POWER);
    }
    else
        GPIO_clear(LED_POWER);
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *runtime)
{
    (void)pins;
    timeout_stop(&runtime->setting_timeo);

    if (PIN_TOP_BUTTON == (PIN_TOP_BUTTON & pins))
    {
        runtime->top_button_stick = 0;
        mqueue_postv(runtime->mqd, MSG_TOP_BUTTON, 0, 0);
    }

    if (PIN_POWER_BUTTON == (PIN_POWER_BUTTON & pins))
    {
        runtime->power_button_stick = 0;
        mqueue_postv(runtime->mqd, MSG_POWER_BUTTON, 0, 0);
    }

    if (PIN_PREV_BUTTON == (PIN_PREV_BUTTON & pins))
    {
        runtime->prev_next_button_stick = 0;
        mqueue_postv(runtime->mqd, MSG_PREV_BUTTON, 0, 0);
    }
    if (PIN_NEXT_BUTTON == (PIN_NEXT_BUTTON & pins))
    {
        runtime->prev_next_button_stick = 0;
        mqueue_postv(runtime->mqd, MSG_NEXT_BUTTON, 0, 0);
    }

    if (PIN_VOLUME_UP_BUTTON == (PIN_VOLUME_UP_BUTTON & pins))
        mqueue_postv(runtime->mqd, MSG_VOLUME_UP_BUTTON, 0, 0);
    if (PIN_VOLUME_DOWN_BUTTON == (PIN_VOLUME_DOWN_BUTTON & pins))
        timeout_start(&runtime->setting_volume_intv, (void *)MSG_VOLUME_DOWN_BUTTON);
}

static void SETTING_timeout_callback(struct zone_runtime_t *runtime)
{
    if (runtime->setting_alarm_is_modified)
        CLOCK_update_alarms();
    if (runtime->setting_is_modified)
        NVM_set(NVM_SETTING, sizeof(smartcuckoo), &smartcuckoo);

    if (runtime->setting)   // REVIEW: exit from setting
    {
        runtime->setting = false;
        VOICE_say_setting(VOICE_SETTING_DONE);
    }
}

static void SETTING_volume_intv_callback(enum zone_message_t msg_button)
{
    static clock_t tick = 0;
    timeout_stop(&zone.setting_timeo);

    if (MSG_VOLUME_UP_BUTTON == msg_button)
    {
        if (0 == GPIO_peek(PIN_VOLUME_UP_BUTTON))
        {
            mqueue_postv(zone.mqd, MSG_VOLUME_UP_BUTTON, 0, 0);
            goto volume_notification;
        }
        else
            goto volume_adj_done;
    }
    else if (MSG_VOLUME_DOWN_BUTTON == msg_button)
    {
        if (0 == GPIO_peek(PIN_VOLUME_DOWN_BUTTON))
        {
            mqueue_postv(zone.mqd, MSG_VOLUME_DOWN_BUTTON, 0, 0);
            goto volume_notification;
        }
        else
            goto volume_adj_done;
    }
    else
    {
    volume_adj_done:
        tick = 0;
        timeout_stop(&zone.setting_volume_intv);

        zone.setting_is_modified = true;
        smartcuckoo.volume = AUDIO_get_volume_percent();
        timeout_start(&zone.setting_timeo, &zone);

        goto volume_notification_2;
    }

    if (false)
    {
    volume_notification:
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
}

/****************************************************************************
 *  @private: message thread & loops
 ****************************************************************************/
static void MSG_alive(struct zone_runtime_t *runtime)
{
    // LOG_debug("alive");
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

static void MSG_voice_button(struct zone_runtime_t *runtime)
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

static void MSG_setting(struct zone_runtime_t *runtime, enum zone_message_t msg_button)
{
    PMU_power_lock();
    mplayer_playlist_clear();

    // any button will stop alarming & reminders
    CLOCK_dismiss();

    if (MSG_POWER_BUTTON == msg_button)
    {
        if (! runtime->setting)
        {
            runtime->setting_part = VOICE_first_setting();
            runtime->setting = true;
            runtime->setting_is_modified = false;
            runtime->setting_alarm_is_modified = false;

            goto say_setting_part;
        }
        else
        {
            runtime->setting = false;
            VOICE_say_setting(VOICE_SETTING_DONE);
        }
    }
    else if (MSG_PREV_BUTTON == msg_button || MSG_NEXT_BUTTON == msg_button)
    {
        if (MSG_PREV_BUTTON == msg_button)
            runtime->setting_part = VOICE_prev_setting(runtime->setting_part);
        else
            runtime->setting_part = VOICE_next_setting(runtime->setting_part);

        struct CLOCK_moment_t *alarm0;
    say_setting_part:
        alarm0 = CLOCK_get_alarm(0);

        if (VOICE_SETTING_ALARM_HOUR == runtime->setting_part ||
            VOICE_SETTING_ALARM_MIN == runtime->setting_part ||
            VOICE_SETTING_ALARM_RINGTONE == runtime->setting_part)
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
    else if (MSG_VOLUME_UP_BUTTON == msg_button || MSG_VOLUME_DOWN_BUTTON == msg_button)
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
            if (MSG_VOLUME_UP_BUTTON == msg_button)
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
            if (MSG_VOLUME_UP_BUTTON == msg_button)
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
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            else
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 23) % 24;
            goto setting_rtc_set_time;

        case VOICE_SETTING_MINUTE:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            else
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 59) % 60;
            goto setting_rtc_set_time;

        case VOICE_SETTING_YEAR:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                TM_year_add(&runtime->setting_dt, 1);
            else
                TM_year_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MONTH:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                TM_month_add(&runtime->setting_dt, 1);
            else
                TM_month_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MDAY:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                TM_mday_add(&runtime->setting_dt, 1);
            else
                TM_mday_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_ALARM_HOUR:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            else
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 23) % 24;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_MIN:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            else
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 59) % 60;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_RINGTONE:
            if (MSG_VOLUME_UP_BUTTON == msg_button)
                alarm0->ringtone_id = (uint8_t)VOICE_next_ringtone(alarm0->ringtone_id);
            else
                alarm0->ringtone_id = (uint8_t)VOICE_prev_ringtone(alarm0->ringtone_id);
            goto setting_modify_alarm;

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
        setting_modify_alarm:
            runtime->setting_alarm_is_modified = true;
            alarm0->enabled = true;
            alarm0->mtime = time2mtime(mktime(&runtime->setting_dt));
            alarm0->mdate = 0;
            alarm0->wdays = 0x7F;
        }

        mplayer_playlist_clear();
        VOICE_say_setting_part(runtime->setting_part, &runtime->setting_dt, alarm0->ringtone_id);
    }

    PMU_power_unlock();
}

static void MSG_mynoise_toggle(bool step)
{
    mplayer_stop();

    int startting = false;
    if (step)
    {
        if (MYNOISE_is_running())
        {
            uint32_t seconds = MYNOISE_get_power_off_seconds();
            char buf[26];
            int len;

            if (0 == seconds)
            {
                MYNOISE_power_off_seconds(POWER_OFF_STEP_SECONDS);
                len = sprintf(buf, "noise: off_seconds=%d\n", POWER_OFF_STEP_SECONDS);
            }
            else if (POWER_OFF_STEP_SECONDS >= seconds)
            {
                MYNOISE_power_off_seconds(2U * POWER_OFF_STEP_SECONDS);
                len = sprintf(buf, "noise: off_seconds=%d\n", 2U *POWER_OFF_STEP_SECONDS);
            }
            else if (2U * POWER_OFF_STEP_SECONDS >= seconds)
            {
                MYNOISE_power_off_seconds(3U * POWER_OFF_STEP_SECONDS);
                len = sprintf(buf, "noise: off_seconds=%d\n", 3U *POWER_OFF_STEP_SECONDS);
            }
            else
                MYNOISE_stop();

            if (0 != (seconds = MYNOISE_get_power_off_seconds()))
                SHELL_notification(buf, (unsigned)len);
        }
        else
            startting = true;
    }
    else
    {
        if (MYNOISE_is_running())
            MYNOISE_stop();
        else
            startting = true;
    }

    if (startting)
    {
        int err;

        if (BATT_HINT_MV > PERIPHERAL_batt_ad_sync())
        {
            VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);
            LOG_error("%s", strerror(EBATT));
        }
        else
        {
            GPIO_set(LED_POWER);

            if (0 != (err = MYNOISE_start()))
            {
                GPIO_clear(LED_POWER);
                LOG_error("%s", AUDIO_strerror(err));
            }
        }
    }
}

static void MSG_alarm_toggle(struct zone_runtime_t *runtime)
{
    smartcuckoo.alarm_is_on = ! smartcuckoo.alarm_is_on;

    runtime->setting_is_modified = true;
    timeout_start(&zone.setting_timeo, &zone);

    if (smartcuckoo.alarm_is_on)
        VOICE_say_setting(VOICE_SETTING_EXT_ALARM_ON);
    else
        VOICE_say_setting(VOICE_SETTING_EXT_ALARM_OFF);

    if (smartcuckoo.alarm_is_on)
        SHELL_notification("alarm: on\n", 10);
    else
        SHELL_notification("alarm: off\n", 11);
}

static void MSG_power_button(struct zone_runtime_t *runtime, bool power_down)
{
    extern void bluetooth_go_sleep(void);
    extern void bluetooth_wakeup(void);

    if (power_down)
    {
        runtime->power_is_down = true;

        GPIO_intr_disable(PIN_TOP_BUTTON);
        GPIO_intr_disable(PIN_PREV_BUTTON);
        GPIO_intr_disable(PIN_NEXT_BUTTON);
        GPIO_intr_disable(PIN_VOLUME_UP_BUTTON);
        GPIO_intr_disable(PIN_VOLUME_DOWN_BUTTON);

        MYNOISE_stop();
        bluetooth_go_sleep();

        while (0 == GPIO_peek(PIN_POWER_BUTTON))
        {
            WDOG_feed();

            GPIO_clear(LED_POWER);
            msleep(100);
            GPIO_set(LED_POWER);
            msleep(100);
        }
        GPIO_set(LED_POWER);

    }
    else if (runtime->power_is_down)
    {
        PERIPHERAL_gpio_intr_enable();
        bluetooth_wakeup();

        runtime->power_is_down = false;
    }
}

static void MSG_volume(struct zone_runtime_t *runtime, enum zone_message_t msg_button)
{
    if (AUDIO_renderer_is_idle())
        VOICE_play_ringtone(CLOCK_get_ringtone_id());

    if (MSG_VOLUME_UP_BUTTON == msg_button)
        AUDIO_inc_volume(VOLUME_MAX_PERCENT);
    else
        AUDIO_dec_volume(VOLUME_MIN_PERCENT);

    LOG_info("volume: %d", AUDIO_get_volume_percent());
    timeout_start(&runtime->setting_volume_intv, (void *)msg_button);
}

static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zone_runtime_t *runtime)
{
    while (true)
    {
        struct MQ_message_t *msg = mqueue_recv(runtime->mqd);
        WDOG_feed();

        if (msg)
        {
            if (! runtime->setting)
            {
                switch ((enum zone_message_t)msg->msgid)
                {
                case MSG_TOP_BUTTON:
                    if (! CLOCK_snooze())
                    {
                        if (0 == GPIO_peek(PIN_TOP_BUTTON))
                        {
                            if (0 == runtime->top_button_stick)
                                runtime->top_button_stick = clock();

                            if (LONG_PRESS_VOICE > clock() - runtime->top_button_stick)
                            {
                                thread_yield();
                                mqueue_postv(runtime->mqd, MSG_TOP_BUTTON, 0, 0);
                            }
                            else
                            {
                                CLOCK_dismiss();
                                MSG_voice_button(runtime);
                            }
                        }
                        else
                        {
                            MSG_mynoise_toggle(false);
                            mqueue_flush(runtime->mqd);
                        }
                    }
                    break;

                case MSG_POWER_BUTTON:
                    MSG_power_button(runtime, false);

                    if (! CLOCK_dismiss())
                    {
                        if (0 == GPIO_peek(PIN_POWER_BUTTON))
                        {
                            if (0 == runtime->power_button_stick)
                                runtime->power_button_stick = clock();

                            if (LONG_PRESS_POWER_DOWN > clock() - runtime->power_button_stick)
                            {
                                thread_yield();
                                mqueue_postv(runtime->mqd, MSG_POWER_BUTTON, 0, 0);
                            }
                            else
                                MSG_power_button(runtime, true);
                        }
                        else
                        {
                            MSG_mynoise_toggle(true);
                            mqueue_flush(runtime->mqd);
                        }
                    }
                    else
                        mqueue_flush(runtime->mqd);
                    break;

                case MSG_PREV_BUTTON:
                    if (! CLOCK_dismiss())
                    {
                        if (0 == GPIO_peek(PIN_PREV_BUTTON))
                        {
                            if (0 == runtime->prev_next_button_stick)
                                runtime->prev_next_button_stick = clock();

                            if (LONG_PRESS_SETTING > clock() - runtime->prev_next_button_stick && 0 != GPIO_peek(PIN_NEXT_BUTTON))
                            {
                                thread_yield();
                                mqueue_postv(runtime->mqd, msg->msgid, 0, 0);
                            }
                            else
                            {
                                MYNOISE_stop();

                                if (0 == GPIO_peek(PIN_PREV_BUTTON | PIN_NEXT_BUTTON))
                                {
                                    mqueue_flush(runtime->mqd);
                                    MSG_alarm_toggle(runtime);
                                }
                                else
                                    MSG_setting(runtime, MSG_POWER_BUTTON);
                            }
                        }
                        else if (MYNOISE_is_running())
                        {
                            GPIO_set(LED_POWER);

                            unsigned power_off_seconds = MYNOISE_get_power_off_seconds();
                            MYNOISE_power_off_set_tickdown_cb(NULL);

                            int err = MYNOISE_prev();
                            MYNOISE_power_off_set_tickdown_cb(MYNOISE_power_off_tickdown_callback);

                            if (0 == err)
                                MYNOISE_power_off_seconds(power_off_seconds);

                            mqueue_flush(runtime->mqd);
                        }
                    }
                    else
                        mqueue_flush(runtime->mqd);
                    break;

                case MSG_NEXT_BUTTON:
                    if (! CLOCK_dismiss())
                    {
                        if (0 == GPIO_peek(PIN_NEXT_BUTTON))
                        {
                            if (0 == runtime->prev_next_button_stick)
                                runtime->prev_next_button_stick = clock();

                            if (LONG_PRESS_SETTING > clock() - runtime->prev_next_button_stick && 0 != GPIO_peek(PIN_PREV_BUTTON))
                            {
                                thread_yield();
                                mqueue_postv(runtime->mqd, msg->msgid, 0, 0);
                            }
                            else
                            {
                                MYNOISE_stop();

                                if (0 == GPIO_peek(PIN_PREV_BUTTON | PIN_NEXT_BUTTON))
                                {
                                    mqueue_flush(runtime->mqd);
                                    MSG_alarm_toggle(runtime);
                                }
                                else
                                    MSG_setting(runtime, MSG_POWER_BUTTON);
                            }
                        }
                        else if (MYNOISE_is_running())
                        {
                            GPIO_set(LED_POWER);

                            unsigned power_off_seconds = MYNOISE_get_power_off_seconds();
                            MYNOISE_power_off_set_tickdown_cb(NULL);

                            int err = MYNOISE_next();
                            MYNOISE_power_off_set_tickdown_cb(MYNOISE_power_off_tickdown_callback);

                            if (0 == err)
                                MYNOISE_power_off_seconds(power_off_seconds);

                            mqueue_flush(runtime->mqd);
                        }
                    }
                    else
                        mqueue_flush(runtime->mqd);
                    break;

                case MSG_VOLUME_UP_BUTTON:
                case MSG_VOLUME_DOWN_BUTTON:
                    MSG_volume(runtime, (enum zone_message_t)msg->msgid);
                    break;
                }
            }
            else
                MSG_setting(runtime, (enum zone_message_t)msg->msgid);

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
        {
            if (! runtime->power_is_down)
                MSG_alive(runtime);
        }
    }
}
