#ifndef __MPLAYER_NOISE_H
#define __MPLAYER_NOISE_H                   1

#include <features.h>
#include <stdint.h>
#include <stdbool.h>

    struct NOISE_mapping_t
    {
        char const *id;
        char const *name;
    };

    struct NOISE_attr_t
    {
        bool playing;
        uint16_t curr_id;
    };

__BEGIN_DECLS

extern __attribute__((nothrow))
    int NOISE_attr_init(struct NOISE_attr_t *attr, uint16_t stored_id);

    typedef void (* NOISE_theme_callback_t)(uint16_t id, char const *theme, void *arg, bool final);
extern __attribute__((nothrow))
    void NOISE_enum_themes(NOISE_theme_callback_t callback, void *arg);

extern __attribute__((nothrow, const))
    bool NOISE_is_playing(struct NOISE_attr_t *attr);
extern __attribute__((nothrow))
    void NOISE_set_stopped(struct NOISE_attr_t *attr);

extern __attribute__((nothrow))
    int NOISE_toggle(struct NOISE_attr_t *attr);
extern __attribute__((nothrow))
    int NOISE_play(struct NOISE_attr_t *attr, uint16_t id);
extern __attribute__((nothrow))
    int NOISE_stop(struct NOISE_attr_t *attr);

extern __attribute__((nothrow))
    int NOISE_next(struct NOISE_attr_t *attr);
extern __attribute__((nothrow))
    int NOISE_prev(struct NOISE_attr_t *attr);

__END_DECLS
#endif
