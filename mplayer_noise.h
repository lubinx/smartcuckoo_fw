#ifndef __MPLAYER_NOISE_H
#define __MPLAYER_NOISE_H                   1

#include <features.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

extern __attribute__((nothrow))
    int NOISE_attr_init(uint16_t stored_id);

    typedef void (* NOISE_theme_callback_t)(uint16_t id, char const *theme, void *arg, bool final);
extern __attribute__((nothrow))
    void NOISE_enum_themes(NOISE_theme_callback_t callback, void *arg);

extern __attribute__((nothrow, const))
    bool NOISE_is_playing(void);
extern __attribute__((nothrow))
    void NOISE_set_stopped(void);

extern __attribute__((nothrow))
    int NOISE_toggle(void);
extern __attribute__((nothrow))
    int NOISE_play(uint16_t id);
extern __attribute__((nothrow))
    int NOISE_stop(void);

extern __attribute__((nothrow))
    int NOISE_next(void);
extern __attribute__((nothrow))
    int NOISE_prev(void);

__END_DECLS
#endif
