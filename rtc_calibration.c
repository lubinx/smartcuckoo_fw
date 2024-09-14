#include <pthread.h>

#include <cmu.h>
#include <gpio.h>
#include <pmu.h>
#include <rtc.h>
#include <timer.h>

#include "smartcuckoo.h"
// 0 1 2 3 4 5 6 7

// #pragma GCC optimize("O3")
/***************************************************************************
 *  @private
 ***************************************************************************/
static void rtc_calibration(void)
{
    PMU_power_acquire();
    LOG_info("RTC calibration start...");

    uint32_t rtc_ticks[RTC_CALIBRATION_SECONDS] = {0};
    uint32_t hf_ticks[RTC_CALIBRATION_SECONDS] = {0};

    TIMER_free_running_configure(HW_TIMER0, TIMER_CLK_FREQ);
    TIMER_start(HW_TIMER0);

    __disable_irq();
    if (true)
    {
        while (0 != GPIO_PEEK_DIN_NP(PIN_RTC_CAL_IN));
        while (0 == GPIO_PEEK_DIN_NP(PIN_RTC_CAL_IN));
        // raising edgge of RTC cal input
        uint32_t hf_tick = TIMER_tick(HW_TIMER0);

        for (unsigned i = 0; i < lengthof(rtc_ticks); i ++)
        {
            uint16_t rtc_pre_cnt = (uint16_t)BURTC->PRECNT;
            while (rtc_pre_cnt == (uint16_t)BURTC->PRECNT);
            uint32_t rtc_tick = TIMER_tick(HW_TIMER0);

            rtc_pre_cnt = (uint16_t)BURTC->PRECNT;
            for (unsigned delay = 0; delay < TIMER_CLK_FREQ / 32768; delay ++) __NOP();

            while (true)
            {
                if (rtc_pre_cnt == (uint16_t)BURTC->PRECNT)
                {
                    rtc_ticks[i] = TIMER_tick(HW_TIMER0) - rtc_tick;
                    break;
                }

                if (0 == GPIO_PEEK_DIN_NP(PIN_RTC_CAL_IN) && 0 == hf_ticks[i])
                    hf_ticks[i] = TIMER_tick(HW_TIMER0) - hf_tick;
            }

            // raising edgge of RTC cal input
            while (0 == GPIO_PEEK_DIN_NP(PIN_RTC_CAL_IN));
            hf_tick = TIMER_tick(HW_TIMER0);
        }
    }
    __enable_irq();
    TIMER_deconfigure(HW_TIMER0);

    int hf_tick = 0;
    int rtc_tick = 0;
    for (unsigned i = 0; i < lengthof(rtc_ticks); i ++)
    {
        hf_tick += (int)hf_ticks[i] * 2;    // hf is half second
        rtc_tick += (int)rtc_ticks[i];
    }

    int ppb = (int)((int64_t)(hf_tick - rtc_tick) * 1000000000 / hf_tick);
    RTC_set_calibration_ppb(ppb);

    LOG_info("RTC calibration end, ppb %d, hf ticks: %d", ppb, hf_tick / RTC_CALIBRATION_SECONDS);
    PMU_power_release();
}

/***************************************************************************
 *  @implements
 ***************************************************************************/
#define NVM_OLD_RTC_KEY                 NVM_DEFINE_KEY('C', 'R', 'T', 'C')

void RTC_calibration_init(void)
{
    GPIO_setdir_input(PIN_RTC_CAL_IN);
    bool cal_lvl = GPIO_PEEK_DIN_NP(PIN_RTC_CAL_IN);

    if (1)
    {
        int ppb;
        if (0 == NVM_get(NVM_OLD_RTC_KEY, &ppb, sizeof(ppb)))
        {
            NVM_delete(NVM_OLD_RTC_KEY);
            RTC_set_calibration_ppb(ppb + 2000);    // assume 2 ppm error
        }
    }

    for (int i = 0; i < 55; i ++)
    {
        msleep(10);
        if (GPIO_PEEK_DIN_NP(PIN_RTC_CAL_IN) != cal_lvl)
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

    if (0 == RTC_calibration_ppb())
    {
        int ppb = RTC_calibration_by_hf(0);
        if (0 != ppb)
            RTC_set_calibration_ppb(ppb);
    }
}
