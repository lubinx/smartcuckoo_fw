#ifndef __AHT2X_H
#define __AHT2X_H                       1

#include <features.h>
#include <stdint.h>

__BEGIN_DECLS

extern __attribute__((nothrow))
    int ENV_sensor_createfd(void *const dev, uint16_t kbps);

extern __attribute__((nothrow, nonnull))
    int ENV_sensor_start_convert(int fd);

extern __attribute__((nothrow, nonnull))
    int ENV_sensor_read(int fd, int16_t *tmpr, uint8_t *humidity);

__END_DECLS
#endif
