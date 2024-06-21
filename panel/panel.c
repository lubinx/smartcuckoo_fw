#include "panel_private.h"

#if defined(PANEL_B) || defined(PANEL_C)
    #include <touch_keymap.c>
#else
    #pragma GCC error "unkonwn panel"
#endif

/****************************************************************************
 *  @def
 ****************************************************************************/
#define MQUEUE_PAYLOAD_SIZE             (8)
#define MQUEUE_LENGTH                   (12)

struct PANEL_setting_group_part_t
{
    uint8_t level;
    enum PANEL_setting_part_t start;
    enum PANEL_setting_part_t end;
};

/****************************************************************************
 *  @private
 ****************************************************************************/
// var
struct PANEL_runtime_t panel = {0};
static uint32_t __stack[1024 / sizeof(uint32_t)];

// const
static struct PANEL_setting_group_part_t const __setting_gp[] =
{
    {.level = 1,    .start = SETTING_YEAR,      .end = SETTING_DAY},
    {.level = 1,    .start = SETTING_HOUR,      .end = SETTING_HOUR_FORMT},
    {.level = 1,    .start = SETTING_A_HOUR,    .end = SETTING_WDAY_START},
    {.level = 1,    .start = SETTING_A_HOUR,    .end = SETTING_WDAY_START},
    {.level = 1,    .start = SETTING_A_HOUR,    .end = SETTING_WDAY_START},
    {.level = 1,    .start = SETTING_A_HOUR,    .end = SETTING_WDAY_START},
    {.level = 0},
};

static void GPIO_button_callback(uint32_t pins, struct PANEL_runtime_t *runtime);
static void IOEXT_repeat_callback(void *arg);   // startting by MSG_ioext()
static void SCHEDULE_intv_callback(struct PANEL_runtime_t *runtime);
static void IrDA_callback(struct IrDA_nec_t *irda, bool repeat, struct PANEL_runtime_t *runtime);

static __attribute__((noreturn)) void *MSG_dispatch_thread(struct PANEL_runtime_t *runtime);
static void MSG_alive(struct PANEL_runtime_t *runtime);

static void setting_done(struct PANEL_runtime_t *runtime);
static void setting_modify_defore_save(struct PANEL_runtime_t *runtime);
static void media_stop(struct PANEL_runtime_t *runtime);

static int SHELL_dim(struct UCSH_env *env);
static int SHELL_env_sensor(struct UCSH_env *env);
#ifdef PIN_LAMP
    static int SHELL_lamp(struct UCSH_env *env);
#endif

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PERIPHERAL_gpio_init(void)
{
    GPIO_setdir_input(PIN_EXT_5V_DET);
    GPIO_setdir_input_pp(PULL_UP, PIN_IRDA, true);

    #ifdef PANEL_B
        GPIO_setdir_input_pp(PULL_UP, PIN_EXTIO, true);
        GPIO_setdir_input_pp(PULL_UP, PIN_DIAL_CWA | PIN_DIAL_CWB, true);
    #endif
    #ifdef PANEL_C
        GPIO_setdir_input(PIN_EXTIO);
        GPIO_setdir_output(PUSH_PULL_DOWN, PIN_LAMP);
    #endif
}

void PERIPHERAL_shell_init(void)
{
    UCSH_register("dim", SHELL_dim);
    UCSH_register("env", SHELL_env_sensor);

#ifdef PIN_LAMP
    UCSH_register("lamp", SHELL_lamp);
#endif
}

void PERIPHERAL_ota_init(void)
{
    NVIC_DisableIRQ(GPIO_EVEN_IRQn);
    NVIC_DisableIRQ(GPIO_ODD_IRQn);
}

void PERIPHERAL_init(void)
{
    CLOCK_init();

    // waitfor ext 5V is connected
    while (0 == GPIO_peek(PIN_EXT_5V_DET))
    {
        WDOG_feed();
        BURAM->RET[31].REG = BURTC->CNT;
        msleep(500);
    }
    PMU_power_acquire();

    panel.mqd = mqueue_create(MQUEUE_PAYLOAD_SIZE, MQUEUE_LENGTH);
    // init panel & display
    PANEL_attr_init(&panel.panel_attr, DISPLAY_I2C, &setting.locale);

    // load settings
    if (0 != NVM_get(NVM_SETTING, &setting, sizeof(setting)))
    {
        memset(&setting, 0, sizeof(setting));

        setting.media_volume = 70;
        setting.dim = 40;
    }

    // environment sensor
    panel.env_sensor_devfd = ENV_sensor_createfd(I2C1);
    // bootstap update display once
    MSG_alive(&panel);

    // mp3 chip
    int uart_fd = UART_createfd(USART0, 115200, UART_PARITY_NONE, UART_STOP_BITS_ONE);
    mplayer_initlaize(uart_fd, PIN_PLAY_BUSYING);
    mplayer_idle_shutdown(SETTING_TIMEOUT + 100);
    // volume
    mplayer_set_volume(setting.media_volume);

    // init voice with using alt folder
    VOICE_init(&voice_attr, &setting.locale, true);
    // startting RTC calibration if PIN is connected
    //  NOTE: need after VOICE_init() by using common voice folder
    RTC_calibration_init();

    // touch pad
    panel.ioext_fd_devfd = IOEXT_createfd(I2C1);
    #ifdef PANEL_B
        while (1)
        {
            if (0 == GPIO_peek(PIN_EXT_5V_DET))
                NVIC_SystemReset();

            uint32_t key;
            if (0 == IOEXT_read_key(panel.ioext_fd_devfd, &key) && 0 == key)
                break;

            LOG_warning("IOEXT: %04x", key);
            msleep(500);
        }
    #endif
    timeout_init(&panel.gpio_repeat_intv, 500, IOEXT_repeat_callback, TIMEOUT_FLAG_REPEAT);

    // init locales
    setting.sel_voice_id = VOICE_init_locales(&voice_attr, setting.sel_voice_id, false);

    if (true)
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstack(&attr, __stack, sizeof(__stack));

        pthread_t id;
        pthread_create(&id, &attr, (void *)MSG_dispatch_thread, &panel);
        pthread_attr_destroy(&attr);
    }

    // modules
    NOISE_attr_init(&panel.noise_attr, setting.last_noise_id);
    PANEL_light_ad_attr_init(&panel.light_sens);
    IrDA_init(&panel.irda, PIN_IRDA, (void *)IrDA_callback, &panel);
    #ifdef PIN_LAMP
        LAMP_attr_init(&panel.lamp_attr);
    #endif

    // intr
    GPIO_intr_enable(PIN_EXTIO, TRIG_BY_FALLING_EDGE, (void *)GPIO_button_callback, &panel);
    #ifdef PANEL_B
        GPIO_debounce(PIN_EXTIO, 80);
        GPIO_intr_enable(PIN_DIAL_CWA, TRIG_BY_FALLING_EDGE, (void *)GPIO_button_callback, &panel);
    #endif

    #ifdef PANEL_C
        GPIO_intr_enable(PIN_SNOOZE, TRIG_BY_FALLING_EDGE, (void *)GPIO_button_callback, &panel);
        GPIO_debounce(PIN_SNOOZE, 60);
        GPIO_intr_enable(PIN_MESSAGE, TRIG_BY_FALLING_EDGE, (void *)GPIO_button_callback, &panel);
    #endif

    // start schedule intv
    timeout_init(&panel.schedule_intv, 380, (void *)SCHEDULE_intv_callback, TIMEOUT_FLAG_REPEAT);
    timeout_start(&panel.schedule_intv, &panel);
}

void mplayer_stopping_callback(void)
{
    NOISE_set_stopped(&panel.noise_attr);

    // any mplayer_stop() will stop alarming
    CLOCK_stop_current_alarm();
    // any mplayer_stop() will snooze all current reminder
    CLOCK_snooze_reminders();
}

void mplayer_idle_callback(void)
{
     if (CLOCK_is_alarming())
     {
        int idx = CLOCK_peek_start_alarms(time(NULL));

        if (-1 == idx)
            PANEL_attr_set_blinky(&panel.panel_attr, 0);
        else
            PANEL_attr_set_blinky(&panel.panel_attr, PANEL_IND_1 << idx);
     }
}

/****************************************************************************
 *  @private: message handler
 ****************************************************************************/
static void MSG_alive(struct PANEL_runtime_t *runtime)
{
    time_t ts = time(NULL);

    if (runtime->setting.en && SETTING_TIMEOUT < clock() - runtime->setting.tick)
        setting_done(runtime);

    if (runtime->tmp_content.en && SETTING_TIMEOUT / 3 < clock() - runtime->tmp_content.tick)
    {
        runtime->tmp_content.en = false;

        PANEL_attr_set_disable(&runtime->panel_attr, 0);
        PANEL_attr_set_blinky(&runtime->panel_attr, 0);
    }

    if (runtime->setting.en || runtime->tmp_content.en)
    {
        enum PANEL_setting_group_t group;
        if (runtime->setting.en)
            group = runtime->setting.group;
        else
            group = runtime->tmp_content.group;

        if (SETTING_TIME_GROUP == group)
        {
            PANEL_attr_set_ringtone_id(&runtime->panel_attr, -1);
            PANEL_attr_set_humidity(&runtime->panel_attr, setting.locale.hfmt);
        }

        if (SETTING_group_is_alarms(group))
        {
            struct CLOCK_moment_t *moment = &clock_setting.alarms[group - SETTING_ALARM_1_GROUP];

            PANEL_attr_set_disable(&runtime->panel_attr, PANEL_TMPR | PANEL_HUMIDITY);
            PANEL_attr_set_humidity(&runtime->panel_attr, -1);

            if (moment->enabled)
            {
                if (0 == moment->mdate)
                    PANEL_attr_set_mdate(&runtime->panel_attr, -1);

                PANEL_attr_set_mtime(&runtime->panel_attr, moment->mtime);
                PANEL_attr_set_wdays(&runtime->panel_attr, moment->wdays);
            }
            else
            {
                PANEL_attr_set_mdate(&runtime->panel_attr, -1);
                PANEL_attr_set_mtime(&runtime->panel_attr, -1);
                PANEL_attr_set_wdays(&runtime->panel_attr, 0);
            }

            // tmpr in alarm is ringtone_id
            PANEL_attr_set_ringtone_id(&runtime->panel_attr, moment->ringtone_id);
        }
        else
            goto update_dt_alarms;
    }
    else
    {
    update_dt_alarms:
        PANEL_attr_set_datetime(&runtime->panel_attr, ts);
        PANEL_attr_unset_flags(&runtime->panel_attr, PANEL_INDICATES);

        for (unsigned i = 0; i < 4; i ++)
        {
            struct CLOCK_moment_t *moment = &clock_setting.alarms[i];

            if (moment->enabled)
                PANEL_attr_set_flags(&runtime->panel_attr, PANEL_IND_1 << i);
        }
    }

    if (! runtime->setting.en &&
        timeout_is_running(&panel.schedule_intv))
    {
        CLOCK_peek_start_reminders(ts);

        if (! CLOCK_is_alarming())
        {
            int idx = CLOCK_peek_start_alarms(ts);
            if (-1 != idx)
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_1 << idx);
        }
    }

    if (0 == runtime->env_sensor.last_ts || ENV_SENSOR_UPDATE_SECONDS < ts - runtime->env_sensor.last_ts)
    {
        runtime->env_sensor.last_ts = ts;
        PERIPHERAL_adv_update();

        ENV_sensor_read(runtime->env_sensor_devfd,
            &runtime->env_sensor.tmpr, &runtime->env_sensor.humidity);
    }

    if (! (PANEL_TMPR & runtime->panel_attr.disable_parts))
        PANEL_attr_set_tmpr(&runtime->panel_attr, runtime->env_sensor.tmpr);
    if (! (PANEL_HUMIDITY & runtime->panel_attr.disable_parts))
        PANEL_attr_set_humidity(&runtime->panel_attr, (int8_t)runtime->env_sensor.humidity);

    PANEL_update(&runtime->panel_attr);
}

static void MSG_set_blinky(struct PANEL_runtime_t *runtime)
{
    PANEL_attr_set_disable(&runtime->panel_attr, 0);
    PANEL_attr_set_blinky(&runtime->panel_attr, 0);

    if (runtime->setting.en)
    {
        if (SETTING_group_is_alarms(runtime->setting.group))
        {
            PANEL_attr_set_disable(&runtime->panel_attr, PANEL_TMPR | PANEL_HUMIDITY);
            PANEL_attr_set_humidity(&runtime->panel_attr, -1);
        }
        else if (SETTING_TIME_GROUP == runtime->setting.group)
        {
            PANEL_attr_set_disable(&runtime->panel_attr, PANEL_TMPR | PANEL_HUMIDITY);
            PANEL_attr_set_humidity(&runtime->panel_attr, setting.locale.hfmt);
        }
        else
            PANEL_attr_set_disable(&runtime->panel_attr, 0);

        for (unsigned i = 0; i < 4; i ++)
        {
            struct CLOCK_moment_t *moment = &clock_setting.alarms[i];
            if (moment->enabled)
                PANEL_attr_set_flags(&runtime->panel_attr, PANEL_IND_1 << i);
            else
                PANEL_attr_unset_flags(&runtime->panel_attr, PANEL_IND_1 << i);
        }

        if (0 == runtime->setting.level)
        {
            switch (runtime->setting.group)
            {
            case SETTING_DATE_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_DATE);
                break;
            case SETTING_TIME_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_TIME);
                break;
            case SETTING_TMPR_UNIT_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_TMPR | PANEL_TMPR_UNIT);
                break;

            case SETTING_ALARM_1_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_1 | PANEL_TIME);
                break;
            case SETTING_ALARM_2_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_2 | PANEL_TIME);
                break;
            case SETTING_ALARM_3_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_3 | PANEL_TIME);
                break;
            case SETTING_ALARM_4_GROUP:
                PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_4 | PANEL_TIME);
                break;
            };
        }
        else if (1 == runtime->setting.level)
        {
            uint32_t blinky = 0;

            switch (runtime->setting.part)
            {
            case SETTING_NONE:
                break;

            // date
            case SETTING_YEAR:
                blinky = PANEL_YEAR;
                break;
            case SETTING_MONTH:
                blinky = PANEL_MONTH;
                break;
            case SETTING_DAY:
                blinky = PANEL_DAY;
                break;

            // time
            case SETTING_HOUR:
                blinky = PANEL_HOUR;
                break;
            case SETTING_MINUTE:
                blinky = PANEL_MINUTE;
                break;
            case SETTING_HOUR_FORMT:
                blinky = PANEL_HUMIDITY;
                break;

            // alarm
            case SETTING_A_HOUR:
                blinky = PANEL_HOUR;
                break;
            case SETTING_A_MINUTE:
                blinky = PANEL_MINUTE;
                break;
            case SETTING_A_RINGTONE:
                blinky = PANEL_TMPR;
                break;
            // alarm wdays
            case SETTING_WDAY_SUNDAY:
                blinky = PANEL_WDAY_SUNDAY;
                break;
            case SETTING_WDAY_MONDAY:
                blinky = PANEL_WDAY_MONDAY;
                break;
            case SETTING_WDAY_THESDAY:
                blinky = PANEL_WDAY_THESDAY;
                break;
            case SETTING_WDAY_WEDNESDAY:
                blinky = PANEL_WDAY_WEDNESDAY;
                break;
            case SETTING_WDAY_THURSDAY:
                blinky = PANEL_WDAY_THURSDAY;
                break;
            case SETTING_WDAY_FRIDAY:
                blinky = PANEL_WDAY_FRIDAY;
                break;
            case SETTING_WDAY_SATURDAY:
                blinky = PANEL_WDAY_SATURDAY;
                break;
            }

            switch (runtime->setting.group)
            {
            default:
                break;

            case SETTING_TIME_GROUP:
                PANEL_attr_set_ringtone_id(&runtime->panel_attr, -1);
                PANEL_attr_set_humidity(&runtime->panel_attr, setting.locale.hfmt);
                break;

            case SETTING_ALARM_1_GROUP:
                blinky |= PANEL_IND_1;
                break;
            case SETTING_ALARM_2_GROUP:
                blinky |= PANEL_IND_2;
                break;
            case SETTING_ALARM_3_GROUP:
                blinky |= PANEL_IND_3;
                break;
            case SETTING_ALARM_4_GROUP:
                blinky |= PANEL_IND_4;
                break;
            };

            PANEL_attr_set_blinky(&runtime->panel_attr, blinky);
        }
    }

    mqueue_postv(runtime->mqd, MSG_ALIVE, 0, 0);
}

static void MSG_light_sensitive(struct PANEL_runtime_t *runtime, int percent)
{
    PANEL_set_dim(&runtime->panel_attr, setting.dim, (uint8_t)percent);
}

static void MSG_irda(struct IrDA_t *irda)
{
    printf("0x%08lX\n", irda->value);
}

static void MSG_ioext(struct PANEL_runtime_t *runtime)
{
    timeout_stop(&panel.gpio_repeat_intv);

    uint32_t key;
    IOEXT_read_key(runtime->ioext_fd_devfd, &key);

    if (0 != key)
    {
        LOG_info("touch key: 0x%08lx", key);

        struct TOUCH_keymap_t const *mapping = &__touch_keymap[0];
        while (0 != mapping->key)
        {
            if (key & mapping->key)
            {
                mqueue_postv(runtime->mqd, mapping->msg_key, 0, 0);

                if (MSG_is_repeatable(mapping->msg_key))
                    timeout_start(&panel.gpio_repeat_intv, (void *)(int)mapping->key);
                break;
            }
            mapping ++;
        }
    }
}

static void MSG_button_snooze(struct PANEL_runtime_t *runtime)
{
    static clock_t snooze_activity = 0;
    static unsigned click_count = 0;

    if (runtime->tmp_content.en)
    {
        mqueue_postv(runtime->mqd, MSG_BUTTON_SETTING, 0, 0);
        mqueue_postv(runtime->mqd, MSG_BUTTON_SNOOZE, 0, 0);
    }
    else
    {
        if (CLOCK_is_alarming())
        {
            CLOCK_stop_current_alarm();
            PANEL_attr_set_blinky(&runtime->panel_attr, 0);
        }
        media_stop(runtime);

        if (5000 < clock() - snooze_activity)
            click_count = 0;
        snooze_activity = clock();

        if (0 == click_count ++ % 2)
            VOICE_say_time_epoch(&voice_attr, time(NULL));
        else
            VOICE_say_date_epoch(&voice_attr, time(NULL));
    }
    LOG_verbose("MSG_button_snooze");
}

static void MSG_button_message(struct PANEL_runtime_t *runtime)
{
    (void)runtime;
    LOG_verbose("MSG_button_message");
}

static void MSG_function_key(struct PANEL_runtime_t *runtime, enum PANEL_message_t msgid)
{
    switch (msgid)
    {
    default:
        break;

    case MSG_BUTTON_SETTING:
        runtime->setting.tick = clock();

        if ( ! runtime->setting.en)
        {
            runtime->setting.en = true;
            runtime->setting.level = 0;

            runtime->setting.group = runtime->tmp_content.en ? runtime->tmp_content.group : 0;
            runtime->setting.part = 0;
            runtime->tmp_content.en = false;    // copied into setting

            media_stop(runtime);
            VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
        }
        else
        {
            if (0 == runtime->setting.level)
                setting_done(runtime);
            else
                runtime->setting.level --;
        }
        mqueue_postv(runtime->mqd, MSG_SET_BLINKY, 0, 0);
        break;

    case MSG_BUTTON_NOISE:
        if (! runtime->setting.en)
            NOISE_toggle(&runtime->noise_attr);
        break;

    case MSG_BUTTON_RECORD:
        break;
    }
}

static void MSG_common_key_setting(struct PANEL_runtime_t *runtime, enum PANEL_message_t msgid)
{
    runtime->setting.tick = clock();
    int value;

    switch (msgid)
    {
    default:
        break;

    case MSG_BUTTON_SNOOZE:
        if (SETTING_ALARM_1_GROUP <= runtime->setting.group &&
            SETTING_ALARM_4_GROUP >= runtime->setting.group)
        {
            struct CLOCK_moment_t *moment
                = &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];

            if (0 == moment->mdate && 0 >= moment->wdays)
                moment->wdays = 0x3E;

            moment->enabled = ! moment->enabled;
            runtime->setting.part = __setting_gp[runtime->setting.group].start;

            if (moment->enabled)
                runtime->setting.level = 1;
            else
                runtime->setting.level = 0;

            goto post_set_blinky;
        }
        else
            goto msg_button_ok;
        break;

    msg_button_ok:
    case MSG_BUTTON_OK:
        if (runtime->setting.level < __setting_gp[runtime->setting.group].level)
        {
            if (1 == ++ runtime->setting.level)
               runtime->setting.part = __setting_gp[runtime->setting.group].start;

            if (SETTING_group_is_alarms(runtime->setting.group))
            {
                struct CLOCK_moment_t *moment
                    = &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];

                if (! moment->enabled)
                {
                    moment->enabled = true;

                    if (0 == moment->mdate && 0 >= moment->wdays)
                        moment->wdays = 0x3E;
                }
            }
            goto post_set_blinky;
        }
        else if (SETTING_TMPR_UNIT_GROUP == runtime->setting.group)
        {
            if (CELSIUS == setting.locale.tmpr_unit)
                setting.locale.tmpr_unit = FAHRENHEIT;
            else
                setting.locale.tmpr_unit = CELSIUS;

            runtime->setting.is_modified = true;
        }
        else if (SETTING_ALARM_1_GROUP <= runtime->setting.group &&
            SETTING_ALARM_4_GROUP >= runtime->setting.group)
        {
            struct CLOCK_moment_t *moment
                = &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];

            if (SETTING_part_is_wdays(runtime->setting.part))
            {
                int8_t wday;

                switch (runtime->setting.part)
                {
                default:
                    wday = 0;
                    break;

                case SETTING_WDAY_SUNDAY:
                    wday = 1 << 0;
                    break;
                case SETTING_WDAY_MONDAY:
                    wday = 1 << 1;
                    break;
                case SETTING_WDAY_THESDAY:
                    wday = 1 << 2;
                    break;
                case SETTING_WDAY_WEDNESDAY:
                    wday = 1 << 3;
                    break;
                case SETTING_WDAY_THURSDAY:
                    wday = 1 << 4;
                    break;
                case SETTING_WDAY_FRIDAY:
                    wday = 1 << 5;
                    break;
                case SETTING_WDAY_SATURDAY:
                    wday = 1 << 6;
                    break;
                }

                if (wday & moment->wdays)
                    moment->wdays &= (int8_t)~wday;
                else
                    moment->wdays |= wday;
            }

            runtime->setting.alarm_is_modified = true;
            goto post_set_blinky;
        }
        else if (SETTING_HOUR_FORMT == runtime->setting.part)
        {
            switch (setting.locale.hfmt)
            {
            case HFMT_DEFAULT:
            case HFMT_24:
                setting.locale.hfmt = HFMT_12;
                break;

            case HFMT_12:
                setting.locale.hfmt = HFMT_24;
                break;
            }
            runtime->setting.is_modified = true;

            goto post_set_blinky;
        }
        else
        {
            if (runtime->setting.level)
            {
                runtime->setting.level --;
                goto post_set_blinky;
            }
        }

        break;

    case MSG_BUTTON_UP:
        if (0 == runtime->setting.level)
        {
        loop_group_down:
            if (0 < runtime->setting.group)
                runtime->setting.group --;
            else
                runtime->setting.group = SETTING_GROUP_MAX;
            goto post_set_blinky;
        }
        else
        {
            if (SETTING_part_is_wdays(runtime->setting.part))
            {
                runtime->setting.part --;

                if (! SETTING_part_is_wdays(runtime->setting.part))
                    runtime->setting.part = SETTING_WDAY_END;

                goto post_set_blinky;
            }
            else
            {
                value = 1;

                if (SETTING_A_RINGTONE == runtime->setting.part)
                    goto ringtone_inc_dec;
                else
                    goto datetime_inc_dec;
            }
        }
        break;

    case MSG_BUTTON_DOWN:
        if (0 == runtime->setting.level)
        {
        loop_group_up:
            if (SETTING_GROUP_MAX > runtime->setting.group)
                runtime->setting.group ++;
            else
                runtime->setting.group = 0;
            goto post_set_blinky;
        }
        else
        {
            if (SETTING_part_is_wdays(runtime->setting.part))
            {
                runtime->setting.part ++;

                if (! SETTING_part_is_wdays(runtime->setting.part))
                    runtime->setting.part = SETTING_WDAY_START;

                goto post_set_blinky;
            }
            else
            {
                value = -1;

                if (SETTING_A_RINGTONE == runtime->setting.part)
                    goto ringtone_inc_dec;
                else
                    goto datetime_inc_dec;
            }
        }
        break;

    case MSG_BUTTON_LEFT:
        if (0 == runtime->setting.level)
            goto loop_group_down;

        if (runtime->setting.part > __setting_gp[runtime->setting.group].start)
            runtime->setting.part --;
        else
            runtime->setting.part = __setting_gp[runtime->setting.group].end;

        if (SETTING_A_RINGTONE == runtime->setting.part)
        {
            struct CLOCK_moment_t *moment =
                &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];

            mplayer_stop();
            VOICE_play_ringtone(&voice_attr, moment->ringtone_id);
        }
        goto post_set_blinky;

    case MSG_BUTTON_RIGHT:
        if (0 == runtime->setting.level)
            goto loop_group_up;

        if (runtime->setting.part < __setting_gp[runtime->setting.group].end)
            runtime->setting.part ++;
        else
            runtime->setting.part = __setting_gp[runtime->setting.group].start;

        if (SETTING_A_RINGTONE == runtime->setting.part)
        {
            struct CLOCK_moment_t *moment =
                &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];

            mplayer_stop();
            VOICE_play_ringtone(&voice_attr, moment->ringtone_id);
        }
        goto post_set_blinky;
    }

    if (false)
    {
    post_set_blinky:
        mqueue_postv(runtime->mqd, MSG_SET_BLINKY, 0, 0);
    }

    if (false)
    {
        struct CLOCK_moment_t *moment;

    ringtone_inc_dec:
        media_stop(runtime);

        moment = &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];
        if (0 < value)
            moment->ringtone_id = (uint8_t)VOICE_next_ringtone(&voice_attr, moment->ringtone_id);
        else
            moment->ringtone_id = (uint8_t)VOICE_prev_ringtone(&voice_attr, moment->ringtone_id);

        VOICE_play_ringtone(&voice_attr, moment->ringtone_id);
        mqueue_postv(runtime->mqd, MSG_ALIVE, 0, 0);
    }

    if (false)
    {
        struct CLOCK_moment_t *moment;

    datetime_inc_dec:
        if (SETTING_ALARM_1_GROUP <= runtime->setting.group &&
            SETTING_ALARM_4_GROUP >= runtime->setting.group)
        {
            moment = &clock_setting.alarms[runtime->setting.group - SETTING_ALARM_1_GROUP];
        }
        else
            moment = NULL;

        switch (runtime->setting.part)
        {
        default:
            moment = NULL;
            break;

        case SETTING_YEAR:
            if (NULL == moment)
                RTC_year_add(value);
            else
                CLOCK_year_add(moment, value);
            break;
        case SETTING_MONTH:
            if (NULL == moment)
                RTC_month_add(value);
            else
                CLOCK_month_add(moment, value);
            break;
        case SETTING_DAY:
            if (NULL == moment)
                RTC_mday_add(value);
            else
                CLOCK_day_add(moment, value);
            break;
        case SETTING_HOUR:
            RTC_hour_add(value);
            break;
        case SETTING_MINUTE:
            RTC_minute_add(value);
            break;
        case SETTING_HOUR_FORMT:
            if (HFMT_12 == setting.locale.hfmt)
                setting.locale.hfmt = HFMT_24;
            else
                setting.locale.hfmt = HFMT_12;
            break;

        case SETTING_A_HOUR:
            CLOCK_hour_add(moment, value);
            break;
        case SETTING_A_MINUTE:
            CLOCK_minute_add(moment, value);
            break;
        }

        if (NULL != moment)
            runtime->setting.alarm_is_modified = true;

        mqueue_postv(runtime->mqd, MSG_ALIVE, 0, 0);
    }
}

static void MSG_common_key(struct PANEL_runtime_t *runtime, enum PANEL_message_t msgid)
{
    switch (msgid)
    {
    default:
        break;

    case MSG_BUTTON_OK:
        if (runtime->tmp_content.en)
        {
            mqueue_postv(runtime->mqd, MSG_BUTTON_SETTING, 0, 0);
            mqueue_postv(runtime->mqd, MSG_BUTTON_OK, 0, 0);
        }
        break;

    case MSG_BUTTON_LEFT:
        if (NOISE_is_playing(&runtime->noise_attr))
        {
            NOISE_prev(&runtime->noise_attr);
            setting.last_noise_id = (int16_t)runtime->noise_attr.curr_id;
            setting_modify_defore_save(runtime);
        }
        break;
    case MSG_BUTTON_RIGHT:
        if (NOISE_is_playing(&runtime->noise_attr))
        {
            NOISE_next(&runtime->noise_attr);
            setting.last_noise_id = (int16_t)runtime->noise_attr.curr_id;
            setting_modify_defore_save(runtime);
        }
        break;

#ifdef PIN_LAMP
    case MSG_BUTTON_UP:
        LAMP_inc(&runtime->lamp_attr);
        break;
    case MSG_BUTTON_DOWN:
        LAMP_dec(&runtime->lamp_attr);
        break;
#endif
    }
}

static void MSG_volume_key(struct PANEL_runtime_t *runtime, enum PANEL_message_t msgid)
{
    bool is_playing = MPLAYER_PLAYING != mplayer_stat();
    bool modified;
    uint8_t volume;

    switch (msgid)
    {
    default:
        break;

    case MSG_VOLUME_INC:
        volume = mplayer_volume_inc();
        if (volume != setting.media_volume)
        {
            setting.media_volume = volume;
            modified = true;
        }
        if (! is_playing)
            VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);

        setting_modify_defore_save(runtime);
        break;

    case MSG_VOLUME_DEC:
        volume = mplayer_volume_dec();
        if (volume != setting.media_volume)
        {
            setting.media_volume = volume;
            modified = true;
        }
        if (! is_playing)
            VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
        break;
    }

    if (modified)
        setting_modify_defore_save(runtime);
}

static void *MSG_dispatch_thread(struct PANEL_runtime_t *runtime)
{
    while (true)
    {
        struct MQ_message_t *msg = mqueue_timedrecv(runtime->mqd, 500);
        BURAM->RET[31].REG = BURTC->CNT;    // RTC

        if (0 == GPIO_peek(PIN_EXT_5V_DET))
            NVIC_SystemReset();

        if (NULL != msg)
        {
            WDOG_feed();

            switch ((enum PANEL_message_t)msg->msgid)
            {
            case MSG_ALIVE:
                MSG_alive(runtime);             // => update display
                break;

            case MSG_SET_BLINKY:
                MSG_set_blinky(runtime);
                break;

            case MSG_LIGHT_SENS:
                MSG_light_sensitive(runtime, msg->payload.as_i32[0]);
                break;
            case MSG_IRDA:
                MSG_irda((void *)&msg->payload);
                break;

            case MSG_IOEXT:
                MSG_ioext(runtime);         // => MSG_BUTTON_xxx
                break;
            case MSG_BUTTON_SNOOZE:
                if (runtime->setting.en)
                    MSG_common_key_setting(runtime, msg->msgid);
                else
                    MSG_button_snooze(runtime);
                break;
            case MSG_BUTTON_MESSAGE:
                MSG_button_message(runtime);
                break;

            case MSG_BUTTON_SETTING:
                MSG_function_key(runtime, msg->msgid);
                break;

            case MSG_BUTTON_OK:
            case MSG_BUTTON_UP:
            case MSG_BUTTON_DOWN:
            case MSG_BUTTON_LEFT:
            case MSG_BUTTON_RIGHT:
                if (runtime->setting.en)
                    MSG_common_key_setting(runtime, msg->msgid);
                else
                    MSG_common_key(runtime, msg->msgid);
                break;

            case MSG_BUTTON_NOISE:
            case MSG_BUTTON_RECORD:
                MSG_function_key(runtime, msg->msgid);
                break;

            case MSG_VOLUME_INC:
            #if defined(PANEL_B)        // volume inc => up
                if (runtime->setting.en)
                    MSG_common_key_setting(runtime, MSG_BUTTON_DOWN);
                else
            #endif
                MSG_volume_key(runtime, msg->msgid);
                break;

            case MSG_VOLUME_DEC:
            #if defined(PANEL_B)        // volume dec => down
                if (runtime->setting.en)
                    MSG_common_key_setting(runtime, MSG_BUTTON_UP);
                else
            #endif
                MSG_volume_key(runtime, msg->msgid);
                break;

        #ifdef PANEL_B  // PANEL B only
            case MSG_BUTTON_ALM1:
                if (runtime->setting.en)
                {
                    runtime->setting.level = __setting_gp[SETTING_ALARM_1_GROUP].level;
                    runtime->setting.group = SETTING_ALARM_1_GROUP;
                    MSG_set_blinky(runtime);
                }
                else
                {
                    runtime->tmp_content.en = true;
                    runtime->tmp_content.tick = clock();
                    runtime->tmp_content.group = SETTING_ALARM_1_GROUP;

                    PANEL_attr_unset_flags(&runtime->panel_attr, PANEL_INDICATES);
                    PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_1);
                    MSG_alive(runtime);
                }
                break;
            case MSG_BUTTON_ALM2:
                if (runtime->setting.en)
                {
                    runtime->setting.level = __setting_gp[SETTING_ALARM_2_GROUP].level;
                    runtime->setting.group = SETTING_ALARM_2_GROUP;
                    MSG_set_blinky(runtime);
                }
                else
                {
                    runtime->tmp_content.en = true;
                    runtime->tmp_content.tick = clock();
                    runtime->tmp_content.group = SETTING_ALARM_2_GROUP;

                    PANEL_attr_unset_flags(&runtime->panel_attr, PANEL_INDICATES);
                    PANEL_attr_set_blinky(&runtime->panel_attr, PANEL_IND_2);
                    MSG_alive(runtime);
                }
                break;

            case MSG_BUTTON_MEDIA:
                if (NOISE_is_playing(&runtime->noise_attr))
                    NOISE_stop(&runtime->noise_attr);
                else
                    NOISE_toggle(&runtime->noise_attr);
                break;
        #endif

        #ifdef PANEL_C  // PANEL C only
            case MSG_BUTTON_LAMP_EN:
                LAMP_toggle(&runtime->lamp_attr);
                break;
            case MSG_BUTTON_LAMP:
                LAMP_next_color(&runtime->lamp_attr);
                break;
        #endif

            default:
                LOG_debug("unhandled message %x", msg->msgid);
                break;
            }
            mqueue_release_pool(runtime->mqd, msg);
        }
        else
        {
        }
    }
}

/****************************************************************************
 *  @private
 ****************************************************************************/
static void GPIO_button_callback(uint32_t pins, struct PANEL_runtime_t *runtime)
{
    if (PIN_EXTIO == (PIN_EXTIO & pins))
        mqueue_postv(runtime->mqd, MSG_IOEXT, 0, 0);

    #ifdef PANEL_B
        if (PIN_DIAL_CWA == (PIN_DIAL_CWA & pins))
        {
            if (GPIO_peek(PIN_DIAL_CWB))
                mqueue_postv(runtime->mqd, MSG_VOLUME_INC, 0, 0);
            else
                mqueue_postv(runtime->mqd, MSG_VOLUME_DEC, 0, 0);
        }
    #endif

    #ifdef PANEL_C
        if (PIN_SNOOZE == (PIN_SNOOZE & pins))
        {
            mqueue_remove_id(runtime->mqd, MSG_BUTTON_SNOOZE);
            mqueue_postv(runtime->mqd, MSG_BUTTON_SNOOZE, 0, 0);
        }
        if (PIN_MESSAGE == (PIN_MESSAGE & pins))
        {
            mqueue_remove_id(runtime->mqd, MSG_BUTTON_MESSAGE);
            mqueue_postv(runtime->mqd, MSG_BUTTON_MESSAGE, 0, 0);
        }
    #endif
}

static void SCHEDULE_intv_callback(struct PANEL_runtime_t *runtime)
{
    mqueue_postv(runtime->mqd, MSG_ALIVE, 0, 0);
}

static void IOEXT_repeat_callback(void *arg)
{
    uint32_t key;
    IOEXT_read_key(panel.ioext_fd_devfd, &key);

    if (key == (uint32_t)arg)
        MSG_ioext(&panel);
    else
        timeout_stop(&panel.gpio_repeat_intv);
}

static void IrDA_callback(struct IrDA_nec_t *irda, bool repeat, struct PANEL_runtime_t *runtime)
{
    static clock_t repeat_tick = 0;

    if (0xFF == (irda->addr + irda->addr_inv) && 0xFF == (uint8_t)(irda->cmd + irda->cmd_inv))
    {
        uint16_t msgid = 0;

        switch (irda->cmd)
        {
        case 0xFA:
            msgid = MSG_BUTTON_SNOOZE;
            break;

        // function key
        case 0xDC:
            msgid = MSG_BUTTON_SETTING;
            break;
        case 0x82:
            msgid = MSG_BUTTON_NOISE;
            break;
        case 0x80:
            msgid = MSG_VOLUME_INC;
            break;
        case 0x81:
            msgid = MSG_VOLUME_DEC;
            break;
        case 0x8D:
            msgid = MSG_BUTTON_RECORD;
            break;
        case 0xD0:
            msgid = MSG_BUTTON_LAMP_EN;
            break;
        case 0x95:
            msgid = MSG_BUTTON_LAMP;
            break;

        // common key
        case 0xCA:
            msgid = MSG_BUTTON_UP;
            break;
        case 0xD2:
            msgid = MSG_BUTTON_DOWN;
            break;
        case 0x99:
            msgid = MSG_BUTTON_LEFT;
            break;
        case 0xC1:
            msgid = MSG_BUTTON_RIGHT;
            break;
        case 0xCE:
            msgid = MSG_BUTTON_OK;
            break;
        }

        if (0 != msgid)
        {
            if (repeat)
            {
                if (MSG_is_repeatable(msgid) && COMMON_KEY_REPEAT_INTV < clock() - repeat_tick)
                {
                    repeat_tick = clock();
                    mqueue_postv(runtime->mqd, msgid, 0, 0);
                }
            }
            else
                mqueue_postv(runtime->mqd, msgid, 0, 0);
        }

        if (! repeat)
            repeat_tick = clock();

        /*
        ((struct IrDA_t *)irda)->value = irda->cmd;
        mqueue_post_buf(runtime->mqd, MSG_IRDA, 0, irda, sizeof(*irda));
        */
    }
}

static int SHELL_dim(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        unsigned dim = strtoul(env->argv[1], NULL, 10);
        dim = dim > 100 ? 100 : dim;

        if (setting.dim != dim)
        {
            setting.dim = (uint8_t)dim;
            PANEL_set_dim(&panel.panel_attr, setting.dim, panel.light_sens.percent);
        }
    }

    UCSH_printf(env, "dim=%d\n\n", setting.dim);
    return 0;
}

static int SHELL_env_sensor(struct UCSH_env *env)
{
    UCSH_printf(env, "tmpr=%d.%d\n", panel.env_sensor.tmpr / 10, abs(panel.env_sensor.tmpr) % 10);
    UCSH_printf(env, "humidity=%d\n\n", panel.env_sensor.humidity);
    return 0;
}

#ifdef PIN_LAMP
    static void lamp_color_enum_callback(unsigned id, uint8_t R, uint8_t G, uint8_t B, void *arg, bool final)
    {
        UCSH_printf((struct UCSH_env *)arg, "\t{\"id\":%d, ", id);
        UCSH_printf((struct UCSH_env *)arg, "\"R\":%d, \"G\":%d, \"B\":%d}", R, G, B);

        if (final)
            UCSH_puts((struct UCSH_env *)arg, "\n");
        else
            UCSH_puts((struct UCSH_env *)arg, ",\n");
    }

    static int SHELL_lamp(struct UCSH_env *env)
    {
        if (1 == env->argc)
        {
            UCSH_puts(env, "{\"colors\": [\n");
            LAMP_enum_colors(&panel.lamp_attr, lamp_color_enum_callback, env);
            UCSH_puts(env, "]}\n\n");

            return 0;
        }
        else if (2 == env->argc)
        {
            if (0 == strcmp("on", env->argv[1]))
                LAMP_on(&panel.lamp_attr);
            else
                LAMP_off(&panel.lamp_attr);

            UCSH_puts(env, "\n");
            return 0;
        }
        else if (3 == env->argc)
        {
            unsigned idx = strtoul(env->argv[1], NULL, 10);
            LAMP_set_color(&panel.lamp_attr, idx);

            unsigned percent = strtoul(env->argv[2], NULL, 10);
            if (0 != percent)
            {
                LAMP_set_brightness(&panel.lamp_attr, percent);
                LAMP_on(&panel.lamp_attr);
            }
            else
                LAMP_off(&panel.lamp_attr);

            UCSH_puts(env, "\n");
            return 0;
        }
        else
            return EINVAL;
    }
#endif

static void setting_done(struct PANEL_runtime_t *runtime)
{
    runtime->setting.en = false;

    if (runtime->setting.is_modified)
    {
        runtime->setting.is_modified = false;
        NVM_set(NVM_SETTING, &setting, sizeof(setting));
    }
    if (runtime->setting.alarm_is_modified)
    {
        runtime->setting.alarm_is_modified = false;
        NVM_set(NVM_ALARM, &clock_setting.alarms, sizeof(clock_setting.alarms));
    }

    mplayer_stop();
    VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);

    PANEL_attr_set_blinky(&runtime->panel_attr, 0);
    PANEL_attr_set_disable(&runtime->panel_attr, 0);
}

static void setting_defore_store_cb(struct PANEL_runtime_t *runtime)
{
    if (runtime->setting.is_modified)
    {
        NVM_set(NVM_SETTING, &setting, sizeof(setting));
        runtime->setting.is_modified = false;
    }
}

static void setting_modify_defore_save(struct PANEL_runtime_t *runtime)
{
    static timeout_t timeo = TIMEOUT_INITIALIZER(360000, (void *)setting_defore_store_cb);

    runtime->setting.is_modified = true;
    timeout_start(&timeo, runtime);
}

static void media_stop(struct PANEL_runtime_t *runtime)
{
    if (NOISE_is_playing(&runtime->noise_attr))
        NOISE_stop(&runtime->noise_attr);
    else
        mplayer_stop();
}
