#ifndef SMART_LED_FLAGS_H
#define SMART_LED_FLAGS_H               1

#include <features.h>

#include <stdint.h>
#include <stdbool.h>

/***************************************************************************
 *  smart led flags mapping
 *
 *      0 ==> MON TUE WED THU FRI SAT SUN   ==> AM PM DST   => ALM1 ALM2 ALMS TIMER
***************************************************************************/
// AM / PM
    #define SMART_LED_FLAGS_MASK_AM     (1U << 0)
    #define SMART_LED_FLAGS_MASK_PM     (1U << 1)

// DST
    #define SMART_LED_FLAGS_MASK_DST    (1U << 2)

// MON ~ SUN
    #define SMART_LED_FLAGS_SHIFT_MON   (3)
    #define SMART_LED_FLAGS_MASK_MON    (1U << (SMART_LED_FLAGS_SHIFT_MON + 0))
    #define SMART_LED_FLAGS_MASK_TUE    (1U << (SMART_LED_FLAGS_SHIFT_MON + 1))
    #define SMART_LED_FLAGS_MASK_WED    (1U << (SMART_LED_FLAGS_SHIFT_MON + 2))
    #define SMART_LED_FLAGS_MASK_THU    (1U << (SMART_LED_FLAGS_SHIFT_MON + 3))
    #define SMART_LED_FLAGS_MASK_FRI    (1U << (SMART_LED_FLAGS_SHIFT_MON + 4))
    #define SMART_LED_FLAGS_MASK_SAT    (1U << (SMART_LED_FLAGS_SHIFT_MON + 5))
    #define SMART_LED_FLAGS_MASK_SUN    (1U << (SMART_LED_FLAGS_SHIFT_MON + 6))

// ALM 1 ~ X
    #define SMART_LED_FLAGS_SHIFT_ALM1  (10)
    #define SMART_LED_FLAGS_MASK_ALM1   (1U << SMART_LED_FLAGS_SHIFT_ALM0)

__BEGIN_DECLS
    /**
     *  SMART_LED_flags_mask()
    */
extern __attribute__((nothrow))
    uint32_t SMART_LED_flags_mask(uint8_t hour, uint8_t wdays, uint8_t alarms, bool dst);

__END_DECLS
#endif
