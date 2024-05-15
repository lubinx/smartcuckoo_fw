#ifndef __MPLAYER_NOISE_H
#define __MPLAYER_NOISE_H                   1

#include <features.h>
#include <stdbool.h>

    struct NOISE_mapping_t
    {
        char const *id;
        char const *name;
    };

    struct NOISE_attr_t
    {
        bool playing;
        struct NOISE_mapping_t *curr;
    };

__BEGIN_DECLS

extern __attribute__((nothrow))
    int NOISE_attr_init(struct NOISE_attr_t *attr);

extern __attribute__((nothrow, const))
    bool NOISE_is_playing(struct NOISE_attr_t *attr);
extern __attribute__((nothrow))
    void NOISE_set_stopped(struct NOISE_attr_t *attr);

extern __attribute__((nothrow))
    int NOISE_toggle(struct NOISE_attr_t *attr);
extern __attribute__((nothrow))
    int NOISE_stop(struct NOISE_attr_t *attr);

extern __attribute__((nothrow))
    int NOISE_next(struct NOISE_attr_t *attr);
extern __attribute__((nothrow))
    int NOISE_prev(struct NOISE_attr_t *attr);

__END_DECLS
#endif
