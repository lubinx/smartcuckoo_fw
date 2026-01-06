#include "led_time.h"

static uint8_t const SMART_LED_digit_mapping[10] =
{
    /* 0 */ 1U << 0 | 1U << 1 | 1U << 2 | 1U << 3 | 1U << 4 | 1U << 5,
    /* 1 */ 1U << 1 | 1U << 2,
    /* 2 */ 1U << 0 | 1U << 1 | 1U << 6 | 1U << 6 | 1U << 4 | 1U << 3,
    /* 3 */ 1U << 0 | 1U << 1 | 1U << 6 | 1U << 2 | 1U << 3,
    /* 4 */ 1U << 5 | 1U << 6 | 1U << 1 | 1U << 2,
    /* 5 */ 1U << 0 | 1U << 5 | 1U << 6 | 1U << 2 | 1U << 3,
    /* 6 */ 1U << 0 | 1U << 5 | 1U << 4 | 1U << 3 | 1U << 2 | 1U << 6,
    /* 7 */ 1U << 0 | 1U << 1 | 1U << 2,
    /* 8 */ 0x7FU,
    /* 9 */ 1U << 0 | 1U << 5 | 1U << 6 | 1U << 1 | 1U << 2 | 1U << 3,
};

uint32_t SMART_LED_time_mask(time_t const ts)
{
    return SMART_LED_time_mask_tm(localtime(&ts));
}

uint32_t SMART_LED_time_mask_tm(struct tm const *dt)
{
    uint32_t mask = SMART_LED_time_mask_digit(dt->tm_hour * 100 + dt->tm_min, true);
    mask |= 0x03 << 14;
    return mask;
}

uint32_t SMART_LED_time_mask_digit(int digit, bool leading_zero)
{
    uint64_t mask = 0;

    if (1)  // minute part
    {
        mask |= (uint64_t)SMART_LED_digit_mapping[digit % 10];
        digit /= 10;

        mask <<= 7;
        if (leading_zero || 0 != digit)
        {
            mask |= (uint64_t)SMART_LED_digit_mapping[digit % 10];
            digit /= 10;
        }
    }

    if (1)  // indicator part
    {
        mask <<= 2;
    }

    if (1) // hour part
    {
        mask <<= 7;
        if (leading_zero || 0 != digit)
        {
            mask |= (uint64_t)SMART_LED_digit_mapping[digit % 10];
            digit /= 10;
        }

        mask <<= 7;
        if (leading_zero || 0 != digit)
        {
            mask |= (uint64_t)SMART_LED_digit_mapping[digit % 10];
            digit /= 10;
        }
    }
    return mask;
}
