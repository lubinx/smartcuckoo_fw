#include "smartcuckoo.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (8)
#define MQUEUE_LENGTH                   (8)

enum zone_message
{
    MSG_POWER_BUTTON            = 1,
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

    unsigned click_count;
    clock_t voice_loop_stick;

    bool setting;
    bool setting_is_modified;
    bool setting_alarm_is_modified;

    enum VOICE_setting_part_t setting_part;
    struct tm setting_dt;

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

static void volume_adj_intv_callback(void *arg);
static void setting_timeout_callback(void *arg);

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

    GPIO_setdir_input_pp(PULL_UP, PIN_POWER_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_PREV_BUTTON, true);
    // GPIO_setdir_input_pp(PULL_UP, PIN_NEXT_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_VOLUME_UP_BUTTON, true);
    GPIO_setdir_input_pp(PULL_UP, PIN_VOLUME_DOWN_BUTTON, true);
}

void PERIPHERAL_shell_init(void)
{
}

void PERIPHERAL_ota_init(void)
{
    // GPIO_disable(PIN_VOICE_BUTTON);
    // GPIO_disable(PIN_SETTING_BUTTON);
    // GPIO_disable(PIN_ALARM_SW);
}

void PERIPHERAL_init(void)
{
    GPIO_setdir_output(PUSH_PULL_DOWN, PB14);

    timeout_init(&zone.volume_adj_intv, VOLUME_ADJ_HOLD_INTV, volume_adj_intv_callback, TIMEOUT_FLAG_REPEAT);
    timeout_init(&zone.setting_timeo, SETTING_TIMEOUT, setting_timeout_callback, 0);

    // load settings
    if (0 != NVM_get(NVM_SETTING, &setting, sizeof(setting)))
    {
        memset(&setting, 0, sizeof(setting));
        setting.media_volume = 75;
    }
    setting.media_volume = MAX(50, setting.media_volume);
    AUDIO_set_volume_percent(setting.media_volume);

    setting.sel_voice_id = VOICE_init(setting.sel_voice_id, &setting.locale);

    GPIO_intr_enable(PIN_POWER_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_PREV_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    // GPIO_intr_enable(PIN_NEXT_BUTTON, TRIG_BY_FALLING_EDGE,
    //     (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_VOLUME_UP_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);
    GPIO_intr_enable(PIN_VOLUME_DOWN_BUTTON, TRIG_BY_FALLING_EDGE,
        (void *)GPIO_button_callback, &zone);

    MQUEUE_INIT(&zone.mqd, MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
    if (true)
    {
        uint32_t timeout = 5000;
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

bool CLOCK_alarm_switch_is_on(void)
{
    return true;
}

void mplayer_idle_callback(void)
{
    zone.voice_loop_stick = clock();

    if (zone.setting)
        timeout_start(&zone.setting_timeo, NULL);
    else // if (! CLOCK_is_alarming())
        CLOCK_peek_start_alarms(time(NULL));
}

/****************************************************************************
 *  @private: buttons
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct zone_runtime_t *ctx)
{
    (void)pins;
    (void)ctx;
    timeout_stop(&zone.setting_timeo);

    if (PIN_POWER_BUTTON == (PIN_POWER_BUTTON & pins))
        mqueue_postv(ctx->mqd, MSG_POWER_BUTTON, 0, 0);

    if (PIN_PREV_BUTTON == (PIN_PREV_BUTTON & pins))
        mqueue_postv(ctx->mqd, MSG_PREV_BUTTON, 0, 0);
    // if (PIN_NEXT_BUTTON == (PIN_NEXT_BUTTON & pins))
    //     mqueue_postv(ctx->mqd, MSG_NEXT_BUTTON, 0, 0);

    if (PIN_VOLUME_UP_BUTTON == (PIN_VOLUME_UP_BUTTON & pins))
        mqueue_postv(ctx->mqd, MSG_VOLUME_UP_BUTTON, 0, 0);
    if (PIN_VOLUME_DOWN_BUTTON == (PIN_VOLUME_DOWN_BUTTON & pins))
        mqueue_postv(ctx->mqd, MSG_VOLUME_DOWN_BUTTON, 0, 0);
}

static void setting_timeout_callback(void *arg)
{
    ARG_UNUSED(arg);
}

static void volume_adj_intv_callback(void *arg)
{
    if (PIN_VOLUME_UP_BUTTON == (uintptr_t)arg)
    {
        if (0 == GPIO_peek(PIN_VOLUME_UP_BUTTON))
            mqueue_postv(zone.mqd, MSG_VOLUME_UP_BUTTON, 0, 0);
        else
            goto volume_adj_stop;
    }
    else if (PIN_VOLUME_DOWN_BUTTON ==(uintptr_t)arg)
    {
        if (0 == GPIO_peek(PIN_VOLUME_DOWN_BUTTON))
            mqueue_postv(zone.mqd, MSG_VOLUME_DOWN_BUTTON, 0, 0);
        else
            goto volume_adj_stop;
    }
    else
    {
    volume_adj_stop:
        timeout_stop(&zone.volume_adj_intv);
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
}

static void MSG_power_button(struct zone_runtime_t *runtime)
{
    (void)runtime;
    MYNOISE_start();
}

static __attribute__((noreturn)) void *MSG_dispatch_thread(struct zone_runtime_t *runtime)
{
    while (true)
    {
        struct MQ_message_t *msg = mqueue_recv(runtime->mqd);
        WDOG_feed();

        if (msg)
        {
            switch ((enum zone_message)msg->msgid)
            {
            case MSG_POWER_BUTTON:
                MSG_power_button(runtime);
                break;

            case MSG_PREV_BUTTON:
                MYNOISE_prev();
                break;
            case MSG_NEXT_BUTTON:
                MYNOISE_next();
                break;

            case MSG_VOLUME_UP_BUTTON:
                AUDIO_inc_volume(VOLUME_MAX_PERCENT);
                LOG_debug("volume: %d", AUDIO_get_volume_percent());
                timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_UP_BUTTON);
                break;

            case MSG_VOLUME_DOWN_BUTTON:
                AUDIO_dec_volume(VOLUME_MIN_PERCENT);
                LOG_debug("volume: %d", AUDIO_get_volume_percent());
                timeout_start(&runtime->volume_adj_intv, (void *)PIN_VOLUME_DOWN_BUTTON);
                break;
            }

            mqueue_release_pool(runtime->mqd, msg);
        }
        else
            MSG_alive(runtime);
    }
}
