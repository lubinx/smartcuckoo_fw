#ifndef __XL9535_H
#define __XL9535_H                      1

#include <features.h>

__BEGIN_DECLS

extern __attribute__((nothrow))
    int XL9535_createfd(void *const dev, uint16_t kbps);

extern __attribute__((nothrow))
    int XL9535_read_key(int devfd, uint32_t *key);

__END_DECLS
#endif
