#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (8)
#define MQUEUE_LENGTH                   (8)

enum talking_button_message_t
{
    MSG_VOICE_BUTTON            = 1,
    MSG_SETTING_BUTTON,

    MSG_ALARM_SW,
    MSG_SETTING_TIMEOUT,
};

struct talking_button_runtime_t
{
    int mqd;
    clock_t voice_last_tick;

    timeout_t setting_timeo;
    timeout_t alarm_sw_timeo;

    bool setting;
    bool setting_is_modified;
    bool setting_alarm_is_modified;

    enum VOICE_setting_t setting_part;
    struct tm setting_dt;

    time_t batt_last_ts;
};

/****************************************************************************
 *  @private
 ****************************************************************************/
static __attribute__((noreturn)) void *MSG_dispatch_thread(struct talking_button_runtime_t *runtime);
static void MSG_alive(struct talking_button_runtime_t *runtime);

static void GPIO_button_callback(uint32_t pins, struct talking_button_runtime_t *runtime);
static void setting_timeout_callback(void *arg);
static void alaramsw_timeout_callback(void *arg);

// var
static struct talking_button_runtime_t talking_button = {0};
__THREAD_STACK static uint32_t talking_button_stack[1280 / sizeof(uint32_t)];

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PERIPHERAL_gpio_init(void)
{
    GPIO_setdir_input_pp(PULL_UP, PIN_VOICE_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_SETTING_BUTTON, true);
    GPIO_setdir_input_pp(HIGH_Z, PIN_ALARM_SW, true);
}

void PERIPHERAL_ota_init(void)
{
    GPIO_disable(PIN_VOICE_BUTTON);
    GPIO_disable(PIN_SETTING_BUTTON);
    GPIO_disable(PIN_ALARM_SW);
}

void PERIPHERAL_init(void)
{
    timeout_init(&talking_button.setting_timeo, SETTING_TIMEOUT, setting_timeout_callback, 0);
    timeout_init(&talking_button.alarm_sw_timeo, 50, alaramsw_timeout_callback, 0);

    talking_button.voice_last_tick = (clock_t)-SETTING_TIMEOUT;
    talking_button.batt_last_ts = time(NULL);

    // load settings
    if (0 != NVM_get(NVM_SETTING, sizeof(setting), &setting))
    {
        memset(&setting, 0, sizeof(setting));
        setting.media_volume = 75;
    }

    setting.alarm_is_on = (0 != GPIO_peek(PIN_ALARM_SW));
    alaramsw_timeout_callback(NULL);

    setting.media_volume = MAX(50, setting.media_volume);
    AUDIO_set_volume_percent(setting.media_volume);

    setting.voice_sel_id = VOICE_init(setting.voice_sel_id, &setting.locale);

    GPIO_intr_enable(PIN_VOICE_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &talking_button);
    GPIO_intr_enable(PIN_SETTING_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &talking_button);
    GPIO_intr_enable(PIN_ALARM_SW, TRIG_BY_BOTH_EDGE,
        (void *)GPIO_button_callback, &talking_button);

    MQUEUE_INIT(&talking_button.mqd, MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
    if (true)
    {
        uint32_t timeout = 5000;
        ioctl(talking_button.mqd, OPT_RD_TIMEO, &timeout);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstack(&attr, talking_button_stack, sizeof(talking_button_stack));

        pthread_t id;
        pthread_create(&id, &attr, (void *)MSG_dispatch_thread, &talking_button);
        pthread_attr_destroy(&attr);

        MSG_alive(&talking_button);
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
                VOICE_say_setting(VOICE_SETTING_DONE);
                mplayer_waitfor_idle();

                if (0 == RTC_calibration_ppb(PIN_RTC_CAL_IN))
                    VOICE_say_setting(VOICE_SETTING_DONE);
                break;
            }
        }

        WDOG_feed();
        GPIO_disable(PIN_RTC_CAL_IN);
    }
}

/****************************************************************************
 *  @implements: overrides
 ****************************************************************************/
void mplayer_idle_callback(void)
{
    if (talking_button.setting)
        timeout_start(&talking_button.setting_timeo, NULL);
    else
        CLOCK_schedule(time(NULL));
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct talking_button_runtime_t *runtime)
{
    timeout_stop(&talking_button.setting_timeo);

    if (PIN_SETTING_BUTTON == (PIN_SETTING_BUTTON & pins))
        mqueue_postv(runtime->mqd, MSG_SETTING_BUTTON, 0, 0);
    if (PIN_VOICE_BUTTON == (PIN_VOICE_BUTTON & pins))
        mqueue_postv(runtime->mqd, MSG_VOICE_BUTTON, 0, 0);

    if (PIN_ALARM_SW == (PIN_ALARM_SW & pins))
        timeout_start(&talking_button.alarm_sw_timeo, (void *)1);
}

static void setting_timeout_callback(void *arg)
{
    ARG_UNUSED(arg);
    mqueue_postv(talking_button.mqd, MSG_SETTING_TIMEOUT, 0, 0);
}

static void alaramsw_timeout_callback(void *arg)
{
    // REVIEW: need to pull-up to stable
    GPIO_setdir_input_pp(PULL_UP, PIN_ALARM_SW, true);
    {
        usleep(100);
        setting.alarm_is_on = (0 != GPIO_peek(PIN_ALARM_SW));
    }
    GPIO_setdir_input_pp(HIGH_Z, PIN_ALARM_SW, true);

    if (NULL != arg && BATT_EMPTY_MV < PERIPHERAL_batt_volt())
    {
        if (setting.alarm_is_on)
            VOICE_say_setting(VOICE_SETTING_EXT_ALARM_ON);
        else
            VOICE_say_setting(VOICE_SETTING_EXT_ALARM_OFF);
    }
}

static void MSG_alive(struct talking_button_runtime_t *runtime)
{
    // LOG_debug("alive");
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

        if (! runtime->setting && -1 == CLOCK_get_alarming_idx())
            CLOCK_schedule(now);
    }
}

static void MSG_voice_button(struct talking_button_runtime_t *runtime)
{
    time_t now = time(NULL);

    PERIPHERAL_batt_ad_sync();
    runtime->batt_last_ts = now;

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
            percent = MIN(setting.media_volume, MAX(25, percent));
            AUDIO_set_volume_percent(percent);
        }
        else
            AUDIO_set_volume_percent(setting.media_volume);
    }

    PMU_power_lock();
    mplayer_playlist_clear();

    // any button will stop alarming & snooze reminders
    CLOCK_dismiss();

    // insert say low battery
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);

    if (! runtime->setting)
    {
        if (SETTING_TIMEOUT < clock() - runtime->voice_last_tick)
        {
            runtime->voice_last_tick = clock();

            VOICE_say_time_epoch(now);
            CLOCK_say_reminders(now, true);
        }
        else
        {
            runtime->voice_last_tick -= SETTING_TIMEOUT;
            VOICE_say_date_epoch(now);
        }
    }
    else
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
            old_voice_id = setting.voice_sel_id;
            setting.voice_sel_id = VOICE_next_locale();

            if (old_voice_id != setting.voice_sel_id)
            {
                setting.locale.dfmt = DFMT_DEFAULT;
                setting.locale.hfmt = HFMT_DEFAULT;
                runtime->setting_is_modified = true;
            }
            break;

        case VOICE_SETTING_VOICE:
            old_voice_id = setting.voice_sel_id;
            setting.voice_sel_id = VOICE_next_voice();

            if (old_voice_id != setting.voice_sel_id)
            {
                setting.locale.dfmt = DFMT_DEFAULT;
                setting.locale.hfmt = HFMT_DEFAULT;
                runtime->setting_is_modified = true;
            }
            break;

        case VOICE_SETTING_HOUR:
            runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            goto setting_rtc_set_time;

        case VOICE_SETTING_MINUTE:
            runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            goto setting_rtc_set_time;

        case VOICE_SETTING_YEAR:
            TM_year_add(&runtime->setting_dt, 1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MONTH:
            TM_month_add(&runtime->setting_dt, 1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_MDAY:
            TM_mday_add(&runtime->setting_dt, 1);
            goto setting_rtc_set_date;

        case VOICE_SETTING_ALARM_HOUR:
            runtime->setting_dt.tm_hour = (runtime->setting_dt.tm_hour + 1) % 24;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_MIN:
            runtime->setting_dt.tm_min = (runtime->setting_dt.tm_min + 1) % 60;
            goto setting_modify_alarm;

        case VOICE_SETTING_ALARM_RINGTONE:
            alarm0->ringtone_id = (uint8_t)VOICE_next_ringtone(alarm0->ringtone_id);
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

static void MSG_setting_button(struct talking_button_runtime_t *runtime)
{
    PMU_power_lock();
    mplayer_playlist_clear();

    // any button will stop alarming & snooze reminders
    CLOCK_dismiss();

    // say low battery only
    if (1)
    {
        uint16_t batt = PERIPHERAL_batt_volt();
        if (BATT_EMPTY_MV > batt)
            return;
        if (BATT_HINT_MV > batt)
            VOICE_say_setting(VOICE_SETTING_EXT_LOW_BATT);
    }

    if (! runtime->setting)
    {
        runtime->setting_part = VOICE_first_setting();

        runtime->setting = true;
        runtime->setting_is_modified = false;
        runtime->setting_alarm_is_modified = false;
    }
    else
        runtime->setting_part = VOICE_next_setting(runtime->setting_part);

    struct CLOCK_moment_t *alarm0 = CLOCK_get_alarm(0);

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

    PMU_power_unlock();
}

static void MSG_setting_timeout(struct talking_button_runtime_t *runtime)
{
    if (runtime->setting)
    {
        runtime->setting = false;

        if (runtime->setting_alarm_is_modified)
            CLOCK_update_alarms();
        if (runtime->setting_is_modified)
            NVM_set(NVM_SETTING, sizeof(setting), &setting);

        VOICE_say_setting(VOICE_SETTING_DONE);
    }
}

static __attribute__((noreturn)) void *MSG_dispatch_thread(struct talking_button_runtime_t *runtime)
{
    while (true)
    {
        struct MQ_message_t *msg = mqueue_recv(runtime->mqd);
        WDOG_feed();

        if (msg)
        {
            switch ((enum talking_button_message_t)msg->msgid)
            {
            case MSG_VOICE_BUTTON:
                MSG_voice_button(runtime);
                break;

            case MSG_SETTING_BUTTON:
                MSG_setting_button(runtime);
                break;

            case MSG_SETTING_TIMEOUT:
                LOG_warning("talking button: setting timeout");
                MSG_setting_timeout(runtime);
                break;

            case MSG_ALARM_SW:
                // alaramsw_timeout_callback((void *)1);
                timeout_start(&runtime->alarm_sw_timeo, (void *)1);
                break;
            }

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
            MSG_alive(runtime);
    }
}
