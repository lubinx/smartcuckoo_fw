#ifndef __SHT4X_H
#define __SHT4X_H                       1

#include <features.h>

/// device address
    #define SHT4X_A_DA                  (0x44)
    #define SHT4X_B_DA                  (0x45)

/// other parameters
    #define SHT4X_POWER_UP_TIMEO        (2)
    #define SHT4X_HIGH_PRECISION_TIMEO  (9)
    #define SHT4X_MED_PRECISION_TIMEO   (5)
    #define SHT4X_LOW_PRECISION_TIMEO   (2)

__BEGIN_DECLS

extern __attribute__((nothrow))
    int SHT4X_open(void *const dev, uint16_t da, uint16_t kbps);

extern __attribute((nothrow))
    int SHT4X_start_convert(int fd);

extern __attribute((nothrow))
    int SHT4X_read(int fd, int16_t *tmpr, uint8_t *humidity);

__END_DECLS
#endif
