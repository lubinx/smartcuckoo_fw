#ifndef __BS81XC_H
#define __BS81XC_H                      1

#include <features.h>

__BEGIN_DECLS

extern __attribute__((nothrow))
    int BS81xC_createfd(void *const dev, uint16_t kbps);

extern __attribute__((nothrow))
    int BS81xC_read_key(int devfd, uint32_t *key);

__END_DECLS
#endif
