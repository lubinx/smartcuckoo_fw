#ifndef __SMARTCUCKOO_LOCALE_H
#define __SMARTCUCKOO_LOCALE_H          1

#include <stdint.h>

    enum __attribute__((packed)) LOCALE_dfmt_t
    {
        DFMT_DEFAULT,
        DFMT_DDMMYY,
        DFMT_YYMMDD,
        DFMT_MMDDYY,
    };

    enum __attribute__((packed)) LOCALE_hfmt_t
    {
        HFMT_DEFAULT,
        HFMT_12                     = 12,
        HFMT_24                     = 24,
    };

    enum __attribute__((packed)) TMPR_unit_t
    {
        CELSIUS,
        FAHRENHEIT
    };

    struct SMARTCUCKOO_locale_t
    {
        uint8_t rsv;

        enum LOCALE_dfmt_t dfmt;
        enum LOCALE_hfmt_t hfmt;
        enum TMPR_unit_t tmpr_unit;
    };

__BEGIN_DECLS

extern __attribute__((nothrow, const))
    enum LOCALE_dfmt_t LOCALE_dfmt(void);

extern __attribute__((nothrow, const))
    enum LOCALE_hfmt_t LOCALE_hfmt(void);

static inline
    int16_t TMPR_fahrenheit(int16_t celsius_tmpr)
    {
        return (int16_t)((celsius_tmpr * 9 / 5) + 320);
    }

__END_DECLS
#endif
