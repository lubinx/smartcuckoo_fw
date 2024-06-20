#include "PERIPHERAL_config.h"
#include "panel_light_sens.h"
#include "panel_private.h"

static void LIGHT_ad_callback(int volt, int raw, struct PANEL_light_ad_attr_t *attr)
{
    (void)volt;
    attr->cumul += raw;
    attr->cumul_count ++;

    if (LIGHT_SENS_CUMUL_INTV < clock() - attr->activity)
    {
        raw = attr->cumul / attr->cumul_count;

        if (60000 < raw)
            attr->percent = 0U;
        else if (10000 > raw)
            attr->percent = 100U;
        else
            attr->percent = (uint8_t)((60000 - raw) * 100 / 50000);

        mqueue_postv(panel.mqd, MSG_LIGHT_SENS, 0, 1, attr->percent);

        attr->cumul = 0;
        attr->cumul_count = 0;
        attr->activity = clock();
    }
}

void PANEL_light_ad_attr_init(struct PANEL_light_ad_attr_t *attr)
{
    attr->percent = 50;

    ADC_attr_init(&attr->attr, 3, (void *)LIGHT_ad_callback);
    ADC_attr_positive_input(&attr->attr, PIN_LIGHT_SENS_AD);

    ADC_start_convert(&attr->attr, attr);
}
