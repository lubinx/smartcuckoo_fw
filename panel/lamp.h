#ifndef __SMARTCUCKOO_LAMP_h
#define __SMARTCUCKOO_LAMP_h            1

#include <features.h>

    struct LAMP_attr_t
    {
        bool en;

        size_t color_idx;
        int8_t weight;
    };

extern __attribute__((nothrow))
    void LAMP_attr_init(struct LAMP_attr_t *attr);

    typedef void (* LAMP_color_callback_t)(unsigned id, uint8_t R, uint8_t G, uint8_t B, void *arg, bool final);
extern __attribute__((nothrow))
    void LAMP_enum_colors(LAMP_color_callback_t callback, void *arg);

extern __attribute__((nothrow))
    void LAMP_toggle(struct LAMP_attr_t *attr);
extern __attribute__((nothrow))
    void LAMP_on(struct LAMP_attr_t *attr);
extern __attribute__((nothrow))
    void LAMP_off(struct LAMP_attr_t *attr);


extern __attribute__((nothrow))
    void LAMP_set_brightness(struct LAMP_attr_t *attr, unsigned percent);
extern __attribute__((nothrow))
    void LAMP_set_color(struct LAMP_attr_t *attr, unsigned idx);
extern __attribute__((nothrow))
    void LAMP_next_color(struct LAMP_attr_t *attr);

extern __attribute__((nothrow))
    void LAMP_inc(struct LAMP_attr_t *attr);
extern __attribute__((nothrow))
    void LAMP_dec(struct LAMP_attr_t *attr);

#endif
