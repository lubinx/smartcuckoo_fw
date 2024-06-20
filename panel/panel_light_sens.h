#ifndef __SMARTCUCKOO_PANEL_LIGHT_AD_H
#define __SMARTCUCKOO_PANEL_LIGHT_AD_H  1

#include <adc.h>
#include <time.h>

    struct PANEL_light_ad_attr_t
    {
        struct ADC_attr_t attr;
        clock_t activity;
        uint8_t percent;

        int cumul;
        int cumul_count;
    };

extern __attribute__((nothrow))
    void PANEL_light_ad_attr_init(struct PANEL_light_ad_attr_t *attr);

#endif
