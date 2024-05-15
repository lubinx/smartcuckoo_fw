#include <pthread.h>

#include <cmu.h>
#include <pmu.h>
#include <rtc.h>
#include <timer.h>

#include "smartcuckoo.h"

/***************************************************************************
 *  @private
 ***************************************************************************/
static void rtc_calibration(void)
{
    PMU_power_acquire();
    LOG_info("RTC calibration start...");

    uint32_t rtc_cal[RTC_CALIBRATION_SECONDS] = {0};
    uint32_t hf_cal[RTC_CALIBRATION_SECONDS] = {0};

    TIMER_free_running_configure(HW_TIMER0, TIMER_CLK_FREQ);
    TIMER_start(HW_TIMER0);

    __disable_irq();
    if (true)
    {
        while (0 != GPIO_peek(PIN_RTC_CAL_IN));
        while (0 == GPIO_peek(PIN_RTC_CAL_IN));

        uint32_t tick = TIMER_tick(HW_TIMER0);
        uint16_t rtc_pre_cnt = (uint16_t)BURTC->PRECNT;

        for (unsigned i = 0; i < lengthof(rtc_cal); i ++)
        {
            while (rtc_pre_cnt == (uint16_t)BURTC->PRECNT);
            uint32_t rtc_tick = TIMER_tick(HW_TIMER0);

            uint32_t rtc_cnt = BURTC->CNT;
            rtc_pre_cnt = (uint16_t)BURTC->PRECNT;

            while (true)
            {
                if (0 == hf_cal[i])
                {
                    if (0 == GPIO_peek(PIN_RTC_CAL_IN))
                        hf_cal[i] = TIMER_tick(HW_TIMER0) - tick;
                }

                if (rtc_cnt != BURTC->CNT && rtc_pre_cnt == (uint16_t)BURTC->PRECNT)
                {
                    rtc_cal[i] = TIMER_tick(HW_TIMER0) - rtc_tick;

                    while (0 == GPIO_peek(PIN_RTC_CAL_IN));

                    tick = TIMER_tick(HW_TIMER0);
                    rtc_cnt = BURTC->CNT;
                    rtc_pre_cnt = (uint16_t)BURTC->PRECNT;
                    break;
                }
            }
        }
    }
    __enable_irq();

    int hf_tick = 0;
    int rtc_tick = 0;
    for (unsigned i = 0; i < lengthof(rtc_cal); i ++)
    {
        hf_tick += (int)hf_cal[i];
        rtc_tick += (int)rtc_cal[i];
    }

    hf_tick = hf_tick * 2 / 3;      // hf_cal[] is half second
    rtc_tick = rtc_tick / 3;        // rtc_cal[] is full second
    int ppb = (int)((int64_t)(hf_tick - rtc_tick) * 1000000000 / hf_tick);
    RTC_set_calibration_ppb(ppb);

    LOG_info("RTC calibration end, ppb %d, hf ticks: %d", ppb, hf_tick);
    PMU_power_release();
}

/***************************************************************************
 *  @implements
 ***************************************************************************/
void RTC_calibration_init(void)
{
    // start calibration if INPUT has signal
    GPIO_setdir_input(PIN_RTC_CAL_IN);
    bool cal_lvl = GPIO_peek(PIN_RTC_CAL_IN);
    for (int i = 0; i < 55; i ++)
    {
        msleep(10);
        if (GPIO_peek(PIN_RTC_CAL_IN) != cal_lvl)
        {
            VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
            while (MPLAYER_STOPPED != mplayer_stat())
                pthread_yield();

            rtc_calibration();
            VOICE_say_setting(&voice_attr, VOICE_SETTING_DONE, NULL);
            break;
        }
    }
    GPIO_disable(PIN_RTC_CAL_IN);
}
