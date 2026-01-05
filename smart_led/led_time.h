#ifndef SMART_LED_TIME_H
#define SMART_LED_TIME_H                1

#include <features.h>

#include <stdbool.h>
#include <time.h>
#include <hw/smart_led.h>

/***************************************************************************
 *  smart led 7 segment mapping:
 *      ==> 0
 *        +---+
 *      5 | 6 | 1
 *        +-+-+
 *      4 |   | 2
 *        +---+
 *          3 ==>
 ***************************************************************************/
    #define SMART_LED_TIME_MASK_HOUR    0b000000000000000011111111111111UL
    #define SMART_LED_TIME_MASK_IND     0B000000000000001100000000000000UL
    #define SMART_LED_TIME_MASK_MINUTE  0b111111111111110000000000000000UL

__BEGIN_DECLS

    /**
     *  SMART_LED_time_mask_digit()
    */
extern __attribute__((nothrow, nonnull))
    uint64_t SMART_LED_time_mask_digit(int digit, bool leading_zero);

    /**
     *  SMART_LED_time_mask()
    */
extern __attribute__((nothrow, nonnull))
    uint64_t SMART_LED_time_mask(time_t const ts);

    /**
     * SMART_LED_time_mask_tm()
    */
extern __attribute__((nothrow, nonnull))
    uint64_t SMART_LED_time_mask_tm(struct tm const *dt);

__END_DECLS
#endif
