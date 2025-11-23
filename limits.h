#ifndef __SMARTCUCKOO_LIMIT_H
#define __SMARTCUCKOO_LIMIT_H           1

#include_next <limits.h>

    #define COMPILE_YEAR                (\
        (__DATE__[7] - '0') * 1000 +    \
        (__DATE__[8] - '0') *  100 +    \
        (__DATE__[9] - '0') *   10 +    \
        (__DATE__[10] - '0')   \
    )

    #define YEAR_ROUND_LO               COMPILE_YEAR
    #define YEAR_ROUND_HI               (YEAR_ROUND_LO + 10)

#endif
