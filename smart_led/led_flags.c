#include "led_flags.h"

uint32_t SMART_LED_flags_mask(uint8_t hour, uint8_t wdays, uint8_t alarms, bool dst)
{
    uint32_t mask = 0;

    if (12 > hour)
        mask |= SMART_LED_FLAGS_MASK_AM;
    else
        mask |= SMART_LED_FLAGS_MASK_PM;

    if (dst)
        mask |= SMART_LED_FLAGS_MASK_DST;

    for (unsigned i = 0; i < 8; i ++)
    {
        if ((1U << i) & alarms)
            mask |= 1U << (i + SMART_LED_FLAGS_SHIFT_ALM1);
    }

    if (0x01 & wdays)
        mask |= SMART_LED_FLAGS_MASK_SUN;
    if (0x02 & wdays)
        mask |= SMART_LED_FLAGS_MASK_MON;
    if (0x04 & wdays)
        mask |= SMART_LED_FLAGS_MASK_TUE;
    if (0x08 & wdays)
        mask |= SMART_LED_FLAGS_MASK_WED;
    if (0x10 & wdays)
        mask |= SMART_LED_FLAGS_MASK_THU;
    if (0x20 & wdays)
        mask |= SMART_LED_FLAGS_MASK_FRI;
    if (0x40 & wdays)
        mask |= SMART_LED_FLAGS_MASK_SAT;

    return mask;
}
