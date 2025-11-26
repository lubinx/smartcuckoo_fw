#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define ZONE_APWR_NVM_ID                NVM_DEFINE_KEY('A', 'P', 'W', 'R')
#define MQUEUE_PAYLOAD_SIZE             (4)
#define MQUEUE_LENGTH                   (8)

#define MQUEUE_ALIVE_INTV_SECONDS       (5)

enum zone_message
{
    MSG_TOP_BUTTON              = 0,
    MSG_POWER_BUTTON,
    MSG_PREV_BUTTON,
    MSG_NEXT_BUTTON,
    MSG_VOLUME_UP_BUTTON,
    MSG_VOLUME_DOWN_BUTTON,
};

struct zone_runtime_t
{
    int mqd;
    clock_t voice_last_tick;

    timeout_t setting_timeo;
    timeout_t volume_adj_intv;

    bytebool_t power_is_down;
    bytebool_t setting;
    bytebool_t setting_is_modified;
    bytebool_t setting_alarm_is_modified;

    enum VOICE_setting_t setting_part;
    struct tm setting_dt;

    clock_t top_button_stick;
    clock_t power_button_stick;
    clock_t prev_next_button_stick;

    time_t batt_last_ts;
};

struct ZONE_auto_power_t
{
    char noise[96];
    uint32_t off_seconds;
};
static_assert(sizeof(struct ZONE_auto_power_t) < NVM_MAX_OBJECT_SIZE, "");

/****************************************************************************
 *  @private
 ****************************************************************************/
static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zone_runtime_t *runtime);

static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *runtime);
static void SETTING_volume_adj_intv(uint32_t button_pin);
static void SETTING_timeout_save(struct zone_runtime_t *runtime);

static void MYNOISE_power_off_tickdown_callback(uint32_t power_off_seconds_remain);
static void APWR_callback(void);
static int APWR_shell(struct UCSH_env *env);

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
    GPIO_disable(PIN_TOP_BUTTON);
    GPIO_disable(PIN_POWER_BUTTON);
    GPIO_disable(PIN_PREV_BUTTON);
    GPIO_disable(PIN_NEXT_BUTTON);
    GPIO_disable(PIN_VOLUME_UP_BUTTON);
    GPIO_disable(PIN_VOLUME_DOWN_BUTTON);
}

void PERIPHERAL_init(void)
{
    timeout_init(&zone.volume_adj_intv, VOLUME_ADJ_HOLD_INTV, (void *)SETTING_volume_adj_intv, 0);
    timeout_init(&zone.setting_timeo, SETTING_TIMEOUT, (void *)SETTING_timeout_save, 0);

    zone.voice_last_tick = (clock_t)-SETTING_TIMEOUT;
    zone.batt_last_ts = time(NULL);

    // load settings
    if (0 != NVM_get(NVM_SETTING, sizeof(smartcuckoo), &smartcuckoo))
    {
        memset(&smartcuckoo, 0, sizeof(smartcuckoo));

        smartcuckoo.alarm_is_on = true;
        smartcuckoo.media_volume = 30;
    }

    smartcuckoo.media_volume = MIN(50, smartcuckoo.media_volume);
    AUDIO_set_volume_percent(smartcuckoo.media_volume);
    AUDIO_renderer_supress_master_value(80);

    smartcuckoo.voice_sel_id = VOICE_init(smartcuckoo.voice_sel_id, &smartcuckoo.locale);

    MQUEUE_INIT(&zone.mqd, MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
    PERIPHERAL_gpio_intr_enable();

    if (true)
    {
        uint32_t timeout = 1000 * MQUEUE_ALIVE_INTV_SECONDS;
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

    CLOCK_app_specify_callback(APWR_callback);
    UCSH_REGISTER("apwr", APWR_shell);

    MYNOISE_init();
    MYNOISE_power_off_tickdown_cb(MYNOISE_power_off_tickdown_callback);
}

/****************************************************************************
 *  @implements: overrides
 ****************************************************************************/
void mplayer_idle_callback(void)
{
    if (zone.setting)
        timeout_start(&zone.setting_timeo, &zone);
    else
        CLOCK_schedule(time(NULL));
}

static void MYNOISE_power_off_tickdown_callback(uint32_t power_off_seconds_remain)
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
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *runtime)
{
    (void)pins;
    timeout_stop(&zone.setting_timeo);

    if (PIN_TOP_BUTTON == (PIN_TOP_BUTTON & pins))
    {
        runtime->top_button_stick = 0;
        mqueue_postv(runtime->mqd, MSG_TOP_BUTTON, 0, 0);
    }

    if (PIN_POWER_BUTTON == (PIN_POWER_BUTTON & pins))
    {
        runtime->power_button_stick = 0;
        mqueue_postv(runtime->mqd, MSG_POWER_BUTTON, 0, 1, 0);
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
        timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_DOWN_BUTTON);
}

static void SETTING_timeout_save(struct zone_runtime_t *runtime)
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

static void SETTING_volume_adj_intv(uint32_t button_pin)
{
    timeout_stop(&zone.setting_timeo);

    if (PIN_VOLUME_UP_BUTTON == button_pin)
    {
        if (0 == GPIO_peek(PIN_VOLUME_UP_BUTTON))
            mqueue_postv(zone.mqd, MSG_VOLUME_UP_BUTTON, 0, 0);
        else
            goto volume_adj_done;
    }
    else if (PIN_VOLUME_DOWN_BUTTON == button_pin)
    {
        if (0 == GPIO_peek(PIN_VOLUME_DOWN_BUTTON))
            mqueue_postv(zone.mqd, MSG_VOLUME_DOWN_BUTTON, 0, 0);
        else
            goto volume_adj_done;
    }
    else
    {
    volume_adj_done:
        timeout_stop(&zone.volume_adj_intv);

        zone.setting_is_modified = true;
        smartcuckoo.media_volume = AUDIO_get_volume_percent();
        timeout_start(&zone.setting_timeo, &zone);
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
        time_t now = time(NULL);

        if (BATT_AD_INTV_SECONDS < now - runtime->batt_last_ts)
        {
            runtime->batt_last_ts = now;
            PERIPHERAL_batt_ad_start();
        }

        if (! runtime->setting && -1 == CLOCK_get_alarming_idx())
            CLOCK_schedule(now);
    }
}

static void MSG_voice_button(struct zone_runtime_t *runtime)
{
    /*
    PERIPHERAL_batt_ad_sync();
    runtime->batt_last_ts = time(NULL);

    uint16_t mv = PERIPHERAL_batt_volt();
    LOG_info("batt %dmV", mv);

    if (BATT_EMPTY_MV > mv)
    {
        runtime->setting = false;
        return;
    }
    else
    {
        uint8_t percent = BATT_mv_level(mv);
        if (50 > percent)
        {
            percent = MIN(smartcuckoo.media_volume, MAX(25, percent));
            AUDIO_set_volume_percent(percent);
        }
        else
            AUDIO_set_volume_percent(smartcuckoo.media_volume);
    }
    */

    PMU_power_lock();
    mplayer_playlist_clear();
    CLOCK_dismiss();

    // insert say low battery
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);

    if (! runtime->setting)
    {
        time_t ts = time(NULL);

        if (SETTING_TIMEOUT < clock() - runtime->voice_last_tick)
        {
            runtime->voice_last_tick = clock();

            VOICE_say_time_epoch(ts);
            CLOCK_say_reminders(ts, true);
        }
        else
        {
            runtime->voice_last_tick -= SETTING_TIMEOUT;
            VOICE_say_date_epoch(ts);
        }
    }

    PMU_power_unlock();
}

static void MSG_setting(struct zone_runtime_t *runtime, uint32_t button)
{
    PMU_power_lock();
    mplayer_playlist_clear();

    // any button will stop alarming & snooze reminders
    CLOCK_dismiss();

    if (PIN_POWER_BUTTON == button)
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
    else if (PIN_PREV_BUTTON == button || PIN_NEXT_BUTTON == button)
    {
        if (PIN_PREV_BUTTON == button)
            runtime->setting_part = VOICE_prev_setting(runtime->setting_part);
        else
            runtime->setting_part = VOICE_next_setting(runtime->setting_part);

        struct CLOCK_moment_t *alarm0;
    say_setting_part:
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
    else if (PIN_VOLUME_UP_BUTTON == button || PIN_VOLUME_DOWN_BUTTON == button)
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
            if (PIN_VOLUME_UP_BUTTON == button)
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
            if (PIN_VOLUME_UP_BUTTON == button)
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
            if (PIN_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            else
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 23) % 24;
            goto setting_rtc_set_time;

        case VOICE_SETTING_MINUTE:
            if (PIN_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            else
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 59) % 60;
            goto setting_rtc_set_time;

        case VOICE_SETTING_YEAR:
            if (PIN_VOLUME_UP_BUTTON == button)
                TM_year_add(&runtime->setting_dt, 1);
            else
                TM_year_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MONTH:
            if (PIN_VOLUME_UP_BUTTON == button)
                TM_month_add(&runtime->setting_dt, 1);
            else
                TM_month_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MDAY:
            if (PIN_VOLUME_UP_BUTTON == button)
                TM_mday_add(&runtime->setting_dt, 1);
            else
                TM_mday_add(&runtime->setting_dt, -1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_ALARM_HOUR:
            if (PIN_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            else
                runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 23) % 24;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_MIN:
            if (PIN_VOLUME_UP_BUTTON == button)
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            else
                runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 59) % 60;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_RINGTONE:
            if (PIN_VOLUME_UP_BUTTON == button)
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

static void MSG_mynoise_toggle(bool step)
{
    mplayer_stop();

    int startting = false;
    if (step)
    {
        if (MYNOISE_is_running())
        {
            uint32_t seconds = MYNOISE_get_power_off_seconds();

            if (0 == seconds)
                MYNOISE_power_off_seconds(POWER_OFF_STEP_SECONDS);
            else if (POWER_OFF_STEP_SECONDS >= seconds)
                MYNOISE_power_off_seconds(2U * POWER_OFF_STEP_SECONDS);
            else if (2U * POWER_OFF_STEP_SECONDS >= seconds)
                MYNOISE_power_off_seconds(3U * POWER_OFF_STEP_SECONDS);
            else
                MYNOISE_stop();
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
            err = EBATT;
            VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);
        }
        else
            err = MYNOISE_start();

        if (0 != err)
            LOG_error("%s", strerror(err));
    }
    else
    {
        #ifndef NDEBUG
            LOG_warning("heap avail: %u", SYSCON_get_heap_unused());
        #endif
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
}

static void MSG_power_button(struct zone_runtime_t *runtime, bool power_down)
{
    extern void bluetooth_go_sleep(void);
    extern void bluetooth_wakeup(void);

    if (power_down)
    {
        // GPIO_intr_disable(PIN_POWER_BUTTON);
        GPIO_intr_disable(PIN_TOP_BUTTON);
        GPIO_intr_disable(PIN_PREV_BUTTON);
        GPIO_intr_disable(PIN_NEXT_BUTTON);
        GPIO_intr_disable(PIN_VOLUME_UP_BUTTON);
        GPIO_intr_disable(PIN_VOLUME_DOWN_BUTTON);

        runtime->power_is_down = true;
        bluetooth_go_sleep();

        MYNOISE_stop();

        while (0 == GPIO_peek(PIN_POWER_BUTTON))
        {
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

static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zone_runtime_t *runtime)
{
    while (true)
    {
        struct MQ_message_t *msg = mqueue_recv(runtime->mqd);
        WDOG_feed();

        if (msg)
        {
            if (runtime->setting)
            {
                switch ((enum zone_message)msg->msgid)
                {
                case MSG_TOP_BUTTON:
                    break;

                case MSG_POWER_BUTTON:
                    MSG_setting(runtime, PIN_POWER_BUTTON);
                    break;

                case MSG_PREV_BUTTON:
                    MSG_setting(runtime, PIN_PREV_BUTTON);
                    break;

                case MSG_NEXT_BUTTON:
                    MSG_setting(runtime, PIN_NEXT_BUTTON);
                    break;

                case MSG_VOLUME_UP_BUTTON:
                    MSG_setting(runtime, PIN_VOLUME_UP_BUTTON);
                    break;

                case MSG_VOLUME_DOWN_BUTTON:
                    MSG_setting(runtime, PIN_VOLUME_DOWN_BUTTON);
                    break;
                }
            }
            else
            {
                switch ((enum zone_message)msg->msgid)
                {
                case MSG_TOP_BUTTON:
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
                            if (! CLOCK_dismiss())
                                MSG_voice_button(runtime);
                        }
                    }
                    else
                    {
                        if (! CLOCK_snooze())
                            MSG_mynoise_toggle(false);
                    }
                    break;

                case MSG_POWER_BUTTON:
                    MSG_power_button(runtime, false);
                    CLOCK_dismiss();

                    if (false == msg->payload.as_u32[0])
                        MSG_mynoise_toggle(true);

                    if (0 == GPIO_peek(PIN_POWER_BUTTON))
                    {
                        if (0 == runtime->power_button_stick)
                            runtime->power_button_stick = clock();

                        if (LONG_PRESS_POWER_DOWN > clock() - runtime->power_button_stick)
                        {
                            thread_yield();
                            mqueue_postv(runtime->mqd, MSG_POWER_BUTTON, 0, 1, true);
                        }
                        else
                            MSG_power_button(runtime, true);
                    }
                    break;

                case MSG_PREV_BUTTON:
                    CLOCK_dismiss();

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
                                MSG_setting(runtime, PIN_POWER_BUTTON);
                        }
                    }
                    else
                    {
                        unsigned power_off_seconds = MYNOISE_get_power_off_seconds();
                        MYNOISE_power_off_tickdown_cb(NULL);

                        int err = MYNOISE_prev();
                        MYNOISE_power_off_tickdown_cb(MYNOISE_power_off_tickdown_callback);

                        if (0 == err)
                            MYNOISE_power_off_seconds(power_off_seconds);
                        else
                            MYNOISE_power_off_seconds(0);
                    }
                    break;

                case MSG_NEXT_BUTTON:
                    CLOCK_dismiss();

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
                                MSG_setting(runtime, PIN_POWER_BUTTON);
                        }
                    }
                    else
                    {
                        unsigned power_off_seconds = MYNOISE_get_power_off_seconds();
                        MYNOISE_power_off_tickdown_cb(NULL);

                        int err = MYNOISE_next();
                        MYNOISE_power_off_tickdown_cb(MYNOISE_power_off_tickdown_callback);

                        if (0 == err)
                            MYNOISE_power_off_seconds(power_off_seconds);
                        else
                            MYNOISE_power_off_seconds(0);
                    }
                    break;

                case MSG_VOLUME_UP_BUTTON:
                    if (AUDIO_renderer_is_idle())
                        VOICE_play_ringtone(CLOCK_get_ringtone_id());

                    AUDIO_inc_volume(VOLUME_MAX_PERCENT);
                    LOG_info("volume: %d", AUDIO_get_volume_percent());
                    timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_UP_BUTTON);
                    break;

                case MSG_VOLUME_DOWN_BUTTON:
                    if (AUDIO_renderer_is_idle())
                        VOICE_play_ringtone(CLOCK_get_ringtone_id());

                    AUDIO_dec_volume(VOLUME_MIN_PERCENT);
                    LOG_info("volume: %d", AUDIO_get_volume_percent());
                    timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_DOWN_BUTTON);
                    break;
                }
            }

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
        {
            if (! runtime->power_is_down)
                MSG_alive(runtime);
        }
    }
}

/****************************************************************************
 *  @private: auto power & shell
 ****************************************************************************/
static void APWR_callback(void)
{
    static __VOLATILE_DATA char scenario[80];

    struct ZONE_auto_power_t *nvm_ptr = NVM_get_ptr(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr));
    if (NULL != nvm_ptr)
    {
        char *theme = NULL;
        if (1)
        {
            char *ptr = nvm_ptr->noise;
            while (*ptr && '.' != *ptr) ptr ++;

            if ('.' == *ptr)
            {
                memset(scenario, 0, sizeof(scenario));

                strncpy(scenario, nvm_ptr->noise,  MIN(sizeof(scenario), (unsigned)(ptr -nvm_ptr->noise)));
                theme = ptr + 1;
            }
            else
                scenario[0] = '\0';
        }

        int err;
        if (NULL == theme)
            err = MYNOISE_start();
        else
            err = MYNOISE_start_scenario(scenario, theme);

        if (0 == err && 0 != nvm_ptr->off_seconds)
            MYNOISE_power_off_seconds(nvm_ptr->off_seconds);
    }
}

static int APWR_shell(struct UCSH_env *env)
{
    struct ZONE_auto_power_t *nvm_ptr = NVM_get_ptr(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr));
    if (1 == env->argc)
    {
        int pos = 0;
        pos += sprintf(env->buf + pos, "{");

        struct CLOCK_moment_t const *moment = CLOCK_get_app_specify_moment();
        if (1)
        {
            pos += sprintf(env->buf + pos, "\n\t\"enabled\": %s", moment->enabled ? "true" : "false");
            pos += sprintf(env->buf + pos, ",\n\t\"mtime\" :%d", moment->mtime);
            pos += sprintf(env->buf + pos, ",\n\t\"wdays\": %d", moment->wdays);
            pos += sprintf(env->buf + pos, ",\n\t\"mdate\": %" PRIu32, moment->mdate);
            pos += sprintf(env->buf + pos, ",\n\t\"noise\": \"%s\"", NULL == nvm_ptr ? "" : nvm_ptr->noise);
            pos += sprintf(env->buf + pos, ",\n\t\"off_seconds\": %" PRIu32, NULL == nvm_ptr ? 0 : nvm_ptr->off_seconds);
        }

        pos += sprintf(env->buf + pos, "\n}\n");
        writebuf(env->fd, env->buf, (unsigned)pos);

        return 0;
    }

    nvm_ptr = malloc(sizeof(*nvm_ptr));
    if (NULL == nvm_ptr)
        return ENOMEM;

    memset(nvm_ptr, 0, sizeof(*nvm_ptr));
    NVM_get(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr), nvm_ptr);

    struct CLOCK_moment_t moment = {0};
    int err = 0;
    /*
        apwr enable / disable
    */
    if (2 == env->argc)
    {
        if (0 == strcasecmp("off", env->argv[1]))
            moment.enabled = false;
        else if (0 == strcasecmp("on", env->argv[1]))
            moment.enabled = true;
        else
            err = EINVAL;
    }
    /*
        apwr "noise" mtime [wdays=<mask> / mdate=<mdate>] [off_seconds=<seconds>]
    */
    else
    {
        strncpy(nvm_ptr->noise, env->argv[1], sizeof(nvm_ptr->noise));
        moment.enabled = true;

        if (1)
        {
            int mtime = strtol(env->argv[2], NULL, 10);
            if (60 <= mtime % 100 || 24 <= mtime / 100)     // 0000 ~ 2359
                err = EINVAL;
            else
                moment.mtime = (int16_t)mtime;
        }

        if (0 == err)
        {
            char *wday_str = CMD_paramvalue_byname("wdays", env->argc, env->argv);
            if (wday_str)
            {
                int wdays = strtol(wday_str, NULL, 10);
                if (0 == wdays)
                    wdays = strtol(wday_str, NULL, 16);
                if (0x7F < wdays)
                    err = EINVAL;

                moment.wdays = (int8_t)wdays;
            }
            else
                moment.wdays = 0x7F;
        }
        if (0 == err)
        {
            char *mdate_str = CMD_paramvalue_byname("off_seconds", env->argc, env->argv);
            if (mdate_str)
                nvm_ptr->off_seconds = strtoul(mdate_str, NULL, 10);
        }

        if (0 == err)
        {
            char *mdate_str = CMD_paramvalue_byname("mdate", env->argc, env->argv);
            if (mdate_str)
            {
                moment.mdate = strtol(mdate_str, NULL, 10);
                moment.wdays = 0;
            }
            else
                moment.mdate = 0;
        }
        // REVIEW: least one of alarm date or week days masks must set
        if (0 == moment.wdays && 0 == moment.wdays)
            err = EINVAL;

        if (0 == strlen(nvm_ptr->noise))
            err = EINVAL;
    }

    if (0 == err)
    {
        if (0 == (err = NVM_set(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr), nvm_ptr)))
            CLOCK_store_app_specify_moment(&moment);
    }

    free(nvm_ptr);
    return err;
}
