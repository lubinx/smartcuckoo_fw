#ifndef __BS81XC_H
#define __BS81XC_H                      1

#include <features.h>

__BEGIN_DECLS

extern __attribute__((nothrow))
    int IOEXT_createfd(void *i2c_dev, uint16_t kbps);

extern __attribute__((nothrow))
    int IOEXT_read_key(int devfd, uint32_t *key);

__END_DECLS
#endif
