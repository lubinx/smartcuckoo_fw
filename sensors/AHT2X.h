#ifndef __AHT2X_H
#define __AHT2X_H                       1

#include <features.h>
#include "ultracore/types.h"

/// device address
    #define AHT2X_DA                    (0x38)
    #define AHT2X_ALT_DA                (0x39)

/// other parameters
    #define AHT2X_POWER_UP_TIMEOUT      (500)

#ifndef AHT2X_TRIG_TIMEOUT
    #define AHT2X_TRIG_TIMEOUT          (100)
#endif

__BEGIN_DECLS

extern __attribute__((nothrow))
    int AHT2X_open(void *const dev, uint16_t da, uint16_t kbps, uint16_t powerup_timeout);

    /**
     *  AHT2X_start_convert() :
     *      trig AHT2X to start measuring data, otherwise the read will always got last data
     */
extern __attribute__((nothrow, nonnull))
    int AHT2X_start_convert(int fd);

    /**
     *  AHT2X_read() :
     *      read out data
     *  NOTE:
     *      call AHT2X_start_convert() to start convert
     *      *wait* least 80ms and call AHT2X_read() to get convert result
     */
extern __attribute__((nothrow, nonnull))
    int AHT2X_read(int fd, int16_t *tmpr, uint8_t *humidity);

__END_DECLS
#endif
