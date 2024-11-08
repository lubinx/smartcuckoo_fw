#ifndef __MPLAYER_NOISE_H
#define __MPLAYER_NOISE_H                   1

#include <features.h>
#include <stdint.h>
__BEGIN_DECLS

extern __attribute__((nothrow))
    int NOISE_attr_init(uint16_t stored_id);

extern __attribute__((nothrow, const))
    bool NOISE_is_playing(void);

extern __attribute__((nothrow))
    int NOISE_toggle(void);
extern __attribute__((nothrow))
    int NOISE_play(uint16_t id);
extern __attribute__((nothrow))
    int NOISE_stop(void);

extern __attribute__((nothrow))
    uint16_t NOISE_next(void);
extern __attribute__((nothrow))
    uint16_t NOISE_prev(void);

__END_DECLS
#endif
