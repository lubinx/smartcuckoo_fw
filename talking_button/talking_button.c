#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (8)
#define MQUEUE_LENGTH                   (8)

enum talking_button_message_t
{
    MSG_ALARM_SW                    = 1,
    MSG_BUTTON_VOICE,
    MSG_BUTTON_SETTING,

    MSG_SETTING_TIMEOUT,
};

struct talking_button_runtime_t
{
    int mqd;
    int mp3_uartfd;

    timeout_t setting_timeo;

    unsigned click_count;
    clock_t voice_loop_stick;

    bool setting;
    bool alarm_is_on;

    bool setting_is_modified;
    bool setting_alarm_is_modified;

    enum VOICE_setting_part_t setting_part;
    struct tm setting_dt;

    clock_t batt_last_ts;
    uint8_t batt_ctrl_volume;
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
static __attribute__((noreturn)) void *MSG_dispatch_thread(struct talking_button_runtime_t *runtime);

static void GPIO_button_callback(uint32_t pins, struct talking_button_runtime_t *runtime);

static bool battery_checking(void);
static void setting_timeout_callback(void *arg);

// var
static struct talking_button_runtime_t talking_button = {0};
__THREAD_STACK static uint32_t talking_button_stack[1280 / sizeof(uint32_t)];

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PERIPHERAL_gpio_init(void)
{
    GPIO_setdir_input_pp(PULL_UP, PIN_VOICE_BUTTON, true);
    GPIO_setdir_input_pp(HIGH_Z, PIN_SETTING_BUTTON, true);
    GPIO_setdir_input_pp(HIGH_Z, PIN_ALARM_ON, true);
}

void PERIPHERAL_shell_init(void)
{
}

void PERIPHERAL_ota_init(void)
{
}

void PERIPHERAL_init(void)
{
    // load settings
    if (0 != NVM_get(NVM_SETTING, &setting, sizeof(setting)))
    {
        memset(&setting, 0, sizeof(setting));
        setting.media_volume = 100;
    }

    VOICE_init(&voice_attr, &setting.locale);
    // init locales
    setting.sel_voice_id = VOICE_init_locales(&voice_attr, setting.sel_voice_id, true);

    talking_button.alarm_is_on = 0 != GPIO_peek(PIN_ALARM_ON);
    timeout_init(&talking_button.setting_timeo, SETTING_TIMEOUT, setting_timeout_callback, 0);

    GPIO_intr_enable(PIN_VOICE_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &talking_button);
    GPIO_intr_enable(PIN_SETTING_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &talking_button);
    GPIO_intr_enable(PIN_ALARM_ON, TRIG_BY_BOTH_EDGE,
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
                mplayer_play("voice/FFFF.lc3");
                mplayer_waitfor_idle();

                if (0 == RTC_calibration_ppb(PIN_RTC_CAL_IN))
                    mplayer_play("voice/FFFF.lc3");
                break;
            }
        }

        WDOG_feed();
        GPIO_disable(PIN_RTC_CAL_IN);
    }

    talking_button.batt_last_ts = time(NULL);
}

/****************************************************************************
 *  @implements: overrides
 ****************************************************************************/
void PERIPHERAL_on_sleep(void)
{
}

bool CLOCK_alarm_switch_is_on(void)
{
    return talking_button.alarm_is_on;
}

void mplayer_idle_callback(void)
{
    talking_button.voice_loop_stick = clock();

    if (talking_button.setting)
        timeout_start(&talking_button.setting_timeo, NULL);
    else if (-1 != clock_setting.alarming_idx)
        CLOCK_peek_start_alarms(time(NULL));
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct talking_button_runtime_t *ctx)
{
    timeout_stop(&talking_button.setting_timeo);

    if (PIN_SETTING_BUTTON == (PIN_SETTING_BUTTON & pins))
        mqueue_postv(ctx->mqd, MSG_BUTTON_SETTING, 0, 0);
    if (PIN_VOICE_BUTTON == (PIN_VOICE_BUTTON & pins))
        mqueue_postv(ctx->mqd, MSG_BUTTON_VOICE, 0, 0);

    if (PIN_ALARM_ON == (PIN_ALARM_ON & pins))
    {
        if (talking_button.alarm_is_on != (0 != GPIO_peek(PIN_ALARM_ON)))
            mqueue_postv(ctx->mqd, MSG_ALARM_SW, 0, 1, ! talking_button.alarm_is_on);
    }
}

static bool battery_checking(void)
{
    PERIPHERAL_batt_ad_sync();
    LOG_printf("batt %dmV", PERIPHERAL_batt_volt());

    // say low battery only
    if (BATT_EMPTY_MV > PERIPHERAL_batt_volt() || BATT_LOW_MV > PERIPHERAL_batt_volt())
    {
        // discard settings
        talking_button.click_count = 0;
        talking_button.setting = false;;

        mplayer_stop(true);
        return false;
    }
    else
        return true;
}

static void setting_timeout_callback(void *arg)
{
    ARG_UNUSED(arg);
    mqueue_postv(talking_button.mqd, MSG_SETTING_TIMEOUT, 0, 0);
}

static void MSG_alive(struct talking_button_runtime_t *runtime)
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
}

static void MSG_button_voice(struct talking_button_runtime_t *runtime)
{
    mplayer_stop(false);
    if (! battery_checking())
        return;

    if (SETTING_TIMEOUT < clock() - talking_button.voice_loop_stick)
        talking_button.click_count = 0;
    runtime->voice_loop_stick = clock();

    // any button will stop alarming
    if (true == CLOCK_stop_current_alarm())
    {
        mplayer_stop(true);
        return;
    }
    else
        mplayer_stop(false);

    PMU_power_lock();

    // any button will snooze all current reminder
    CLOCK_snooze_reminders();

    // insert say low battery
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        VOICE_say_setting(&voice_attr, VOICE_SETTING_EXT_LOW_BATT, NULL);

    if (! runtime->setting)
    {
        time_t ts = time(NULL);

        switch ((enum buttion_action_t)(runtime->click_count % BUTTON_ACTION_COUNT))
        {
        case BUTTON_ACTION_COUNT:
            break;

        case BUTTON_SAY_TIME:
            VOICE_say_time_epoch(&voice_attr, ts);
            CLOCK_say_reminders(ts, true);
            break;

        case BUTTON_SAY_DATE:
            VOICE_say_date_epoch(&voice_attr, ts);
            break;
        }

        runtime->click_count ++;
    }
    else
    {
        int16_t old_voice_id;

        switch (runtime->setting_part)
        {
        case VOICE_SETTING_EXT_LOW_BATT:
        case VOICE_SETTING_EXT_ALARM_ON:
        case VOICE_SETTING_EXT_ALARM_OFF:
            break;

        case VOICE_SETTING_LANG:
            old_voice_id = setting.sel_voice_id;
            setting.sel_voice_id = VOICE_next_locale(&voice_attr);

            if (old_voice_id != setting.sel_voice_id)
                runtime->setting_is_modified = true;
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
            clock_setting.alarms[0].ringtone_id = (uint8_t)VOICE_next_ringtone(&voice_attr,
                clock_setting.alarms[0].ringtone_id);
            break;

        case VOICE_SETTING_COUNT:
            break;
        };

        if (false)
        {
        setting_rtc_set_time:
            runtime->setting_dt.tm_sec = 0;
            RTC_set_epoch_time(mktime(&runtime->setting_dt));

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

            if (! clock_setting.alarms[0].enabled || mtime != clock_setting.alarms[0].mtime)
            {
                clock_setting.alarms[0].enabled = true;
                clock_setting.alarms[0].mtime = mtime;
                clock_setting.alarms[0].mdate = 0;
                clock_setting.alarms[0].wdays = 0x7F;
                runtime->setting_alarm_is_modified = true;
            }
        }

        VOICE_say_setting_part(&voice_attr, runtime->setting_part,
            &runtime->setting_dt,
            (void *)(uintptr_t)clock_setting.alarms[0].ringtone_id
        );
    }

    PMU_power_unlock();
}

static void MSG_button_setting(struct talking_button_runtime_t *runtime)
{
    if (! battery_checking())
        return;

    runtime->voice_loop_stick = clock();
    mplayer_stop(false);

    // any button will stop alarming
    CLOCK_stop_current_alarm();
    // any button will snooze all current reminder
    CLOCK_snooze_reminders();

    PERIPHERAL_batt_ad_sync();
    if (BATT_EMPTY_MV > PERIPHERAL_batt_volt())
    {
        mplayer_stop(true);
        return;
    }
    // say low battery only
    if (BATT_HINT_MV > PERIPHERAL_batt_volt())
        VOICE_say_setting(&voice_attr, VOICE_SETTING_EXT_LOW_BATT, NULL);

    if (! runtime->setting)
    {
        runtime->click_count = 0;
        runtime->setting = true;
        runtime->setting_is_modified = false;
        runtime->setting_alarm_is_modified = false;
    }
    runtime->setting_part =
        (enum VOICE_setting_part_t)(runtime->click_count % VOICE_SETTING_COUNT);

    if (VOICE_SETTING_LANG == runtime->setting_part &&
        1 >= voice_attr.voice_count)
    {
        // skip language setting since is only 1 language
        runtime->click_count ++;
    }
    runtime->setting_part =
        (enum VOICE_setting_part_t)(runtime->click_count % VOICE_SETTING_COUNT);

    if (VOICE_SETTING_ALARM_HOUR == runtime->setting_part ||
        VOICE_SETTING_ALARM_MIN == runtime->setting_part)
    {
        time_t ts = mtime_to_time(clock_setting.alarms[0].mtime);

        localtime_r(&ts, &runtime->setting_dt);
        runtime->setting_dt.tm_sec = 0;
    }
    else
    {
        time_t ts = time(NULL);
        localtime_r(&ts, &runtime->setting_dt);
        runtime->setting_dt.tm_sec = 0;
    }

    runtime->setting_part =
        (enum VOICE_setting_part_t)(runtime->click_count % VOICE_SETTING_COUNT);
    runtime->click_count ++;

    VOICE_say_setting(&voice_attr, runtime->setting_part,
        (void *)(uintptr_t)clock_setting.alarms[0].ringtone_id);

    switch (runtime->setting_part)
    {
    default:
        VOICE_say_setting_part(&voice_attr, runtime->setting_part, &runtime->setting_dt, NULL);
        break;

    case VOICE_SETTING_LANG:
    case VOICE_SETTING_ALARM_RINGTONE:
        break;
    }
}

static void MSG_setting_timeout(struct talking_button_runtime_t *runtime)
{
    runtime->click_count = 0;

    if (runtime->setting)
    {
        runtime->setting = false;

        if (runtime->setting_is_modified)
            NVM_set(NVM_SETTING, &setting, sizeof(setting));
        if (runtime->setting_alarm_is_modified)
            NVM_set(NVM_ALARM, &clock_setting.alarms, sizeof(clock_setting.alarms));

        if (battery_checking())
            VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
    }
}

static void MSG_alarm_sw(struct talking_button_runtime_t *runtime, bool en)
{
    if (BATT_EMPTY_MV < PERIPHERAL_batt_volt())
    {
        runtime->alarm_is_on = en;
        mplayer_stop(false);

        if (en)
            VOICE_say_setting(&voice_attr, VOICE_SETTING_EXT_ALARM_ON, NULL);
        else
            VOICE_say_setting(&voice_attr, VOICE_SETTING_EXT_ALARM_OFF, NULL);
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
            case MSG_BUTTON_VOICE:
                MSG_button_voice(runtime);
                break;

            case MSG_BUTTON_SETTING:
                MSG_button_setting(runtime);
                break;

            case MSG_SETTING_TIMEOUT:
                MSG_setting_timeout(runtime);
                break;

            case MSG_ALARM_SW:
                MSG_alarm_sw(runtime, msg->payload.as_u32[0]);
                break;
            }

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
            MSG_alive(runtime);
    }
}
