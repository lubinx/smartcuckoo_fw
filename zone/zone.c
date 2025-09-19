#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (8)
#define MQUEUE_LENGTH                   (8)

#define MQUEUE_ALIVE_INTV_SECONDS       (5)

enum zone_message
{
    MSG_VOICE_BUTTON        = 0,
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
    timeout_t volume_adj_intv;

    bool setting;
    bool setting_is_modified;
    bool setting_alarm_is_modified;

    enum VOICE_setting_part_t setting_part;
    struct tm setting_dt;

    clock_t power_button_tick;
    clock_t voice_button_tick;

    unsigned voice_click_count;
    clock_t voice_click_stick;

    uint32_t noise_off_seconds;
    time_t batt_last_ts;
};

enum buttion_action_t
{
    BUTTON_SAY_TIME,
    BUTTON_SAY_DATE,

    BUTTON_ACTION_COUNT,
};

/****************************************************************************
 *  @private
 ****************************************************************************/
static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zone_runtime_t *runtime);

static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *runtime);

static void update_power_timeo_led(struct zone_runtime_t *runtime);
static void setting_timeout_callback(struct zone_runtime_t *runtime);
static void volume_adj_intv_callback(uint32_t button_pin);

// var
static struct zone_runtime_t zone = {0};
__THREAD_STACK static uint32_t zone_stack[1280 / sizeof(uint32_t)];

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PERIPHERAL_gpio_init(void)
{
    GPIO_setdir_output(PUSH_PULL_DOWN, LED_POWER);
    GPIO_setdir_output(PUSH_PULL_UP, LED1);
    GPIO_setdir_output(PUSH_PULL_UP, LED2);
    GPIO_setdir_output(PUSH_PULL_UP, LED3);

    GPIO_setdir_input_pp(PULL_UP, PIN_VOICE_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_POWER_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_PREV_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_NEXT_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_VOLUME_UP_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_VOLUME_DOWN_BUTTON, true);
}

bool PERIPHERAL_is_enable_usb(void)
{
    return 0 == GPIO_peek(PIN_POWER_BUTTON);
}

void PERIPHERAL_ota_init(void)
{
    GPIO_disable(PIN_VOICE_BUTTON);
    GPIO_disable(PIN_POWER_BUTTON);
    GPIO_disable(PIN_PREV_BUTTON);
    GPIO_disable(PIN_NEXT_BUTTON);
    GPIO_disable(PIN_VOLUME_UP_BUTTON);
    GPIO_disable(PIN_VOLUME_DOWN_BUTTON);
}

void PERIPHERAL_init(void)
{
    timeout_init(&zone.volume_adj_intv, VOLUME_ADJ_HOLD_INTV, (void *)volume_adj_intv_callback, 0);
    timeout_init(&zone.setting_timeo, SETTING_TIMEOUT, (void *)setting_timeout_callback, 0);

    // load settings
    if (0 != NVM_get(NVM_SETTING, &setting, sizeof(setting)))
    {
        memset(&setting, 0, sizeof(setting));
        setting.media_volume = 30;
    }

    // REVIEW: reset set alarm on
    setting.alarm_is_on = true;

    setting.media_volume = MIN(50, setting.media_volume);
    AUDIO_set_volume_percent(setting.media_volume);

    setting.sel_voice_id = VOICE_init(setting.sel_voice_id, &setting.locale);

    GPIO_intr_enable(PIN_VOICE_BUTTON, TRIG_BY_FALLING_EDGE,
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

    MQUEUE_INIT(&zone.mqd, MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
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
            if (600 < clock() - ts)
                break;
            else
                pthread_yield();

            if (GPIO_peek(PIN_RTC_CAL_IN))
            {
                LOG_debug("calibration");
                VOICE_say_setting(VOICE_SETTING_DONE, NULL);
                mplayer_waitfor_idle();

                if (0 == RTC_calibration_ppb(PIN_RTC_CAL_IN))
                    VOICE_say_setting(VOICE_SETTING_DONE, NULL);
                break;
            }
        }

        WDOG_feed();
        GPIO_disable(PIN_RTC_CAL_IN);
    }

    zone.batt_last_ts = time(NULL);
}

/****************************************************************************
 *  @implements: overrides
 ****************************************************************************/
void PERIPHERAL_on_sleep(void)
{
}

void mplayer_idle_callback(void)
{
    zone.voice_click_stick = clock();

    if (zone.setting)
        timeout_start(&zone.setting_timeo, &zone);
    else // if (! CLOCK_is_alarming())
        CLOCK_peek_start_alarms(time(NULL));
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *runtime)
{
    (void)pins;
    timeout_stop(&zone.setting_timeo);

    if (PIN_VOICE_BUTTON == (PIN_VOICE_BUTTON & pins))
    {
        runtime->voice_button_tick = 0;
        mqueue_postv(runtime->mqd, MSG_VOICE_BUTTON, 0, 0);
    }

    if (PIN_POWER_BUTTON == (PIN_POWER_BUTTON & pins))
    {
        runtime->power_button_tick = 0;
        mqueue_postv(runtime->mqd, MSG_POWER_BUTTON, 0, 0);
    }

    if (PIN_PREV_BUTTON == (PIN_PREV_BUTTON & pins))
        mqueue_postv(runtime->mqd, MSG_PREV_BUTTON, 0, 0);
    if (PIN_NEXT_BUTTON == (PIN_NEXT_BUTTON & pins))
        mqueue_postv(runtime->mqd, MSG_NEXT_BUTTON, 0, 0);

    if (PIN_VOLUME_UP_BUTTON == (PIN_VOLUME_UP_BUTTON & pins))
        mqueue_postv(runtime->mqd, MSG_VOLUME_UP_BUTTON, 0, 0);
    if (PIN_VOLUME_DOWN_BUTTON == (PIN_VOLUME_DOWN_BUTTON & pins))
        timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_DOWN_BUTTON);
}

static void update_power_timeo_led(struct zone_runtime_t *runtime)
{
    GPIO_set(LED1);
    GPIO_set(LED2);
    GPIO_set(LED3);

    if (0 != runtime->noise_off_seconds)
    {
        GPIO_clear(LED1);

        if (POWER_OFF_STEP_SECONDS < runtime->noise_off_seconds)
            GPIO_clear(LED2);
        if (2U * POWER_OFF_STEP_SECONDS < runtime->noise_off_seconds)
            GPIO_clear(LED3);
    }
}

static void setting_timeout_callback(struct zone_runtime_t *runtime)
{
    if (runtime->setting)
    {
        VOICE_say_setting(VOICE_SETTING_DONE, NULL);
        runtime->setting = false;
    }

    if (runtime->setting_alarm_is_modified)
        CLOCK_update_alarms();
    if (runtime->setting_is_modified)
        NVM_set(NVM_SETTING, &setting, sizeof(setting));
}

static void volume_adj_intv_callback(uint32_t button_pin)
{
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
        setting.media_volume = AUDIO_get_volume_percent();
        timeout_start(&zone.setting_timeo, &zone);
    }
}

static void MSG_alive(struct zone_runtime_t *runtime)
{
    LOG_debug("alive");
    static time_t last_time = 0;

    if (BATT_EMPTY_MV > PERIPHERAL_batt_volt())
    {
        // trigger ad batery everytime, no other actions
        runtime->batt_last_ts = time(NULL);
        PERIPHERAL_batt_ad_start();
    }
    else
    {
        time_t now = time(NULL);

        if (BATT_AD_INTV_SECONDS < now - runtime->batt_last_ts)
        {
            runtime->batt_last_ts = now;
            PERIPHERAL_batt_ad_start();
        }

        if (now != last_time)
        {
            last_time = now;

            if (! runtime->setting)
            {
                CLOCK_peek_start_reminders(now);

                if (! CLOCK_is_alarming())
                    CLOCK_peek_start_alarms(now);
            }
        }
    }

    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        MYNOISE_stop();

    if (0 != runtime->noise_off_seconds)
    {
        if (MQUEUE_ALIVE_INTV_SECONDS >= runtime->noise_off_seconds)
        {
            MYNOISE_stop();
            runtime->noise_off_seconds = 0;
        }
        else
            runtime->noise_off_seconds -= MQUEUE_ALIVE_INTV_SECONDS;
    }
    update_power_timeo_led(runtime);
}

static void MSG_voice_button(struct zone_runtime_t *runtime)
{
    if (0 == (0x1 & zone.voice_click_count))
    {
        zone.batt_last_ts = time(NULL);
        PERIPHERAL_batt_ad_sync();

        uint16_t mv = PERIPHERAL_batt_volt();
        LOG_info("batt %dmV", mv);

        if (BATT_EMPTY_MV > mv)
        {
            // discard settings
            zone.voice_click_count = 0;
            zone.setting = false;
            return;
        }
        /*  REVIEW: remove volume ctrl
        else
        {
            uint8_t percent = BATT_mv_level(mv);
            if (50 > percent)
            {
                percent = MIN(setting.media_volume, MAX(25, percent));
                AUDIO_set_volume_percent(percent);
            }
            else
                AUDIO_set_volume_percent(setting.media_volume);
        }
        */
    }

    PMU_power_lock();
    mplayer_playlist_clear();

    if (SETTING_TIMEOUT < clock() - zone.voice_click_stick)
        zone.voice_click_count = 0;
    runtime->voice_click_stick = clock();

    // any button will stop alarming
    if (true == CLOCK_stop_current_alarm())
        runtime->voice_click_count = 0;

    // any button will snooze all current reminder
    CLOCK_snooze_reminders();

    // insert say low battery
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT, NULL);

    if (! runtime->setting)
    {
        time_t ts = time(NULL);

        switch ((enum buttion_action_t)(runtime->voice_click_count % BUTTON_ACTION_COUNT))
        {
        case BUTTON_ACTION_COUNT:
            break;

        case BUTTON_SAY_TIME:
            VOICE_say_time_epoch(ts);
            CLOCK_say_reminders(ts, true);
            break;

        case BUTTON_SAY_DATE:
            VOICE_say_date_epoch(ts);
            break;
        }

        runtime->voice_click_count ++;
    }

    PMU_power_unlock();
}

static void MSG_setting(struct zone_runtime_t *runtime, uint32_t button)
{
    PMU_power_lock();
    mplayer_playlist_clear();
    MYNOISE_stop();

    runtime->voice_click_stick = clock();

    // any button will stop alarming
    CLOCK_stop_current_alarm();
    // any button will snooze all current reminder
    CLOCK_snooze_reminders();

    // say low battery only
    if (1)
    {
        uint16_t batt = PERIPHERAL_batt_volt();
        if (BATT_EMPTY_MV > batt)
            return;
        if (BATT_HINT_MV > batt)
            VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT, NULL);
    }

    if (PIN_POWER_BUTTON == button)
    {
        if (! runtime->setting)
        {
            runtime->voice_click_count = 0;
            runtime->setting = true;
            runtime->setting_is_modified = false;
            runtime->setting_alarm_is_modified = false;

            goto say_setting_part;
        }
        else
        {
            runtime->setting = false;
            VOICE_say_setting(VOICE_SETTING_DONE, NULL);
        }
    }
    else if (PIN_PREV_BUTTON == button || PIN_NEXT_BUTTON == button)
    {
        if (PIN_PREV_BUTTON == button)
            runtime->voice_click_count = (runtime->voice_click_count + VOICE_SETTING_COUNT - 1) % VOICE_SETTING_COUNT;
        else
            runtime->voice_click_count = (runtime->voice_click_count + 1) % VOICE_SETTING_COUNT;

    say_setting_part:
        runtime->setting_part =
            (enum VOICE_setting_part_t)(runtime->voice_click_count % VOICE_SETTING_COUNT);
        struct CLOCK_moment_t *alarm0 = CLOCK_get_alarm(0);

        if (VOICE_SETTING_LANG == runtime->setting_part && 1 >= VOICE_get_count())
        {
            // skip language setting since is only 1 language
            runtime->voice_click_count ++;
        }
        runtime->setting_part =
            (enum VOICE_setting_part_t)(runtime->voice_click_count % VOICE_SETTING_COUNT);

        if (VOICE_SETTING_ALARM_HOUR == runtime->setting_part ||
            VOICE_SETTING_ALARM_MIN == runtime->setting_part)
        {
            time_t ts = mtime_to_time(alarm0->mtime);

            localtime_r(&ts, &runtime->setting_dt);
            runtime->setting_dt.tm_sec = 0;
        }
        else
        {
            time_t ts = time(NULL);
            localtime_r(&ts, &runtime->setting_dt);
            runtime->setting_dt.tm_sec = 0;
        }

        runtime->setting_part = (enum VOICE_setting_part_t)runtime->voice_click_count;

        VOICE_say_setting(runtime->setting_part,
            (void *)(uintptr_t)alarm0->ringtone_id);

        switch (runtime->setting_part)
        {
        default:
            VOICE_say_setting_part(runtime->setting_part, &runtime->setting_dt, NULL);
            break;

        case VOICE_SETTING_LANG:
        case VOICE_SETTING_ALARM_RINGTONE:
            break;
        }
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
            old_voice_id = setting.sel_voice_id;
            setting.sel_voice_id = VOICE_next_locale();

            if (old_voice_id != setting.sel_voice_id)
            {
                setting.locale.dfmt = DFMT_DEFAULT;
                setting.locale.hfmt = HFMT_DEFAULT;
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
            runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
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
            ts = mktime(&runtime->setting_dt) % 86400;
            mtime = time_to_mtime(ts);

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
        VOICE_say_setting_part(runtime->setting_part,
            &runtime->setting_dt,
            (void *)(uintptr_t)alarm0->ringtone_id
        );
    }

    PMU_power_unlock();
}


static void MSG_mynoise_toggle(struct zone_runtime_t *runtime)
{
    if (! MYNOISE_is_idle())
    {
        if (0 == runtime->noise_off_seconds)
            runtime->noise_off_seconds = POWER_OFF_STEP_SECONDS;
        else if (POWER_OFF_STEP_SECONDS >= runtime->noise_off_seconds)
            runtime->noise_off_seconds = 2U * POWER_OFF_STEP_SECONDS;
        else if (2U * POWER_OFF_STEP_SECONDS >= runtime->noise_off_seconds)
            runtime->noise_off_seconds = 3U * POWER_OFF_STEP_SECONDS;
        else
        {
            MYNOISE_stop();
            runtime->noise_off_seconds = 0;
        }

        update_power_timeo_led(runtime);
    }
    else
        MYNOISE_start();
}

static void MSG_alarm_toggle(struct zone_runtime_t *runtime)
{
    setting.alarm_is_on = ! setting.alarm_is_on;

    runtime->setting_is_modified = true;
    timeout_start(&zone.setting_timeo, &zone);

    if (setting.alarm_is_on)
        VOICE_say_setting(VOICE_SETTING_EXT_ALARM_ON, NULL);
    else
        VOICE_say_setting(VOICE_SETTING_EXT_ALARM_OFF, NULL);
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
                case MSG_VOICE_BUTTON:
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
                case MSG_VOICE_BUTTON:
                    if (0 == GPIO_peek(PIN_VOICE_BUTTON))
                    {
                        if (0 == runtime->voice_button_tick)
                            runtime->voice_button_tick = clock();

                        if (NOISE_LONG_PRESS_TIMEO > clock() - runtime->voice_button_tick)
                        {
                            thread_yield();
                            mqueue_postv(runtime->mqd, MSG_VOICE_BUTTON, 0, 0);
                        }
                        else
                            MYNOISE_toggle();
                    }
                    else
                        MSG_voice_button(runtime);
                    break;

                case MSG_POWER_BUTTON:
                    if (0 == GPIO_peek(PIN_POWER_BUTTON))
                    {
                        if (0 == runtime->power_button_tick)
                            runtime->power_button_tick = clock();

                        if (SETTING_LONG_PRESS_TIMEO > clock() - runtime->power_button_tick)
                        {
                            thread_yield();
                            mqueue_postv(runtime->mqd, MSG_POWER_BUTTON, 0, 0);
                        }
                        else
                            MSG_setting(runtime, PIN_POWER_BUTTON);
                    }
                    else
                        MSG_mynoise_toggle(runtime);
                    break;

                case MSG_PREV_BUTTON:
                    if (0 == GPIO_peek(PIN_PREV_BUTTON))
                    {
                        if (0 != GPIO_peek(PIN_PREV_BUTTON | PIN_NEXT_BUTTON))
                        {
                            thread_yield();
                            mqueue_postv(runtime->mqd, msg->msgid, 0, 0);
                        }
                        else
                            MSG_alarm_toggle(runtime);
                    }
                    else
                        MYNOISE_prev();
                    break;

                case MSG_NEXT_BUTTON:
                    if (0 == GPIO_peek(PIN_NEXT_BUTTON))
                    {
                        if (0 != GPIO_peek(PIN_PREV_BUTTON | PIN_NEXT_BUTTON))
                        {
                            thread_yield();
                            mqueue_postv(runtime->mqd, msg->msgid, 0, 0);
                        }
                        else
                            MSG_alarm_toggle(runtime);
                    }
                    else
                        MYNOISE_next();
                    break;

                case MSG_VOLUME_UP_BUTTON:
                    if (AUDIO_renderer_is_idle())
                        VOICE_play_ringtone(CLOCK_get_next_alarm_ringtone_id());

                    AUDIO_inc_volume(VOLUME_MAX_PERCENT);
                    LOG_info("volume: %d", AUDIO_get_volume_percent());
                    timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_UP_BUTTON);
                    break;

                case MSG_VOLUME_DOWN_BUTTON:
                    if (AUDIO_renderer_is_idle())
                        VOICE_play_ringtone(CLOCK_get_next_alarm_ringtone_id());

                    AUDIO_dec_volume(VOLUME_MIN_PERCENT);
                    LOG_info("volume: %d", AUDIO_get_volume_percent());
                    timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_DOWN_BUTTON);
                    break;
                }
            }

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
            MSG_alive(runtime);
    }
}
