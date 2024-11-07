#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>

#include <i2c.h>
#include "panel_i2c_drv.h"
#include "PERIPHERAL_config.h"

#ifndef NDEBUG
    #pragma GCC optimize("O0")
#endif

/****************************************************************************
 *  @def
 ****************************************************************************/
    #define DA                          (0x73)
    #define DEF_PWM                     (5)

    #define SYSDIS                      (0x80)
    #define SYSEN                       (0x81)
    #define LEDOFF                      (0x82)
    #define LEDON                       (0x83)
    #define BLINKOFF                    (0x88)
    #define BLINK2HZ                    (0x89)
    #define BLINK1HZ                    (0x8A)
    #define BLINK0_5HZ                  (0x8B)

    #define SLAVEMODE                   (0x90)
    #define RCMODE0                     (0x98)
    #define RCMODE1                     (0x9A)
    #define EXTCLK0                     (0x9C)
    #define EXTCLK1                     (0x9E)

    #define COM8NMOS                    (0xA0)
    #define COM16NMOS                   (0xA4)
    #define COM8PMOS                    (0xA8)
    #define COM16PMOS                   (0xAC)

    #define PWM(N)                      (0xB0 | (15 < N ? 15 : N))
    #define SEG(N)                      (0 < N ? (N - 1) * 4 : N)

    #define COM_A                       (1UL << 4)
    #define COM_B                       (1UL << 5)
    #define COM_C                       (1UL << 6)
    #define COM_D                       (1UL << 7)
    #define COM_E                       (1UL << 0)
    #define COM_F                       (1UL << 1)
    #define COM_G                       (1UL << 2)

    #define COM_AA                      (1UL << 3 | COM_A)
    #define COM_BB                      (1UL << 12 | COM_B)
    #define COM_CC                      (1UL << 13 | COM_C)
    #define COM_DD                      (1UL << 14 | COM_D)
    #define COM_EE                      (1UL << 15 | COM_E)
    #define COM_FF                      (1UL << 8  | COM_F)
    #define COM_GG                      (1UL << 9  | COM_G)

    #define FLAG_IND_YEAR               (1UL << 20)
    #define FLAG_IND_MONTH              (1UL << 21)
    #define FLAG_IND_MDAY               (1UL << 22)

    #define FLAG_IND_AM                 (1UL << 4 | 1UL << 5)
    #define FLAG_IND_PM                 (1UL << 6 | 1UL << 7)
    #define FLAG_IND_SEC                (1UL << 16 | 1UL << 23)

    #define FLAG_IND_ALARM              (1UL << 0 | 1UL << 1)
    #define FLAG_IND_1                  (1UL << 2)
    #define FLAG_IND_2                  (1UL << 3)
    #define FLAG_IND_3                  (1UL << 13)
    #define FLAG_IND_4                  (1UL << 12)
    #define FLAG_INDICATES              (FLAG_IND_1 | FLAG_IND_2 | FLAG_IND_3 | FLAG_IND_4)

    #define FLAG_IND_PERCENT            (1UL << 17)
    #define FLAG_IND_HUMIDITY           (1UL << 14 | 1UL << 15)

    #define FLAG_IND_TMPR               (1UL << 18 | 1UL << 19 | 1UL << 28)
    // #define FLAG_IND_DOT                (1UL << 28)
    #define FLAG_IND_TMPR_C             (1UL << 30)
    #define FLAG_IND_TMPR_F             (1UL << 31)

/****************************************************************************
 *  @internal
 ****************************************************************************/
static int PANEL_update_wday(struct PANEL_attr_t *attr, int8_t wday);
static int PANEL_update_tmpr(struct PANEL_attr_t *attr, int16_t tmpr);
static int PANEL_update_digit(struct PANEL_attr_t *attr, uint8_t seg, int no, size_t count, bool lead_zero);

/****************************************************************************
 *  @implements: HAL
 ****************************************************************************/
void PANEL_HAL_init(struct PANEL_attr_t *attr, void *i2c_dev)
{
    attr->da0_fd = I2C_createfd(i2c_dev, DA, I2C0_BUS_SPEED, 0, 0);

    uint32_t timeo = 500;
    ioctl(attr->da0_fd, OPT_WR_TIMEO, &timeo);

    static uint8_t const __startup[8] = {SYSDIS, COM16NMOS, RCMODE1, SYSEN, PWM(DEF_PWM), LEDON};
    while (1)
    {
        if (0 == PANEL_HAL_write(attr->da0_fd, __startup, sizeof(__startup)))
            break;
        else
            msleep(1000);
    }
}

void PANEL_set_dim(struct PANEL_attr_t *attr, uint8_t percent, uint8_t dyn_percent)
{
    static uint8_t pwm_set = 0;

    uint8_t pwm = PWM((uint8_t)(percent * 5 / 100 + dyn_percent * 10 / 100));
    if (pwm_set != pwm)
    {
        pwm_set = pwm;
        PANEL_HAL_write(attr->da0_fd, &pwm, sizeof(pwm));
    }
}

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PANEL_test(struct PANEL_attr_t *attr)
{
    for (int i = 10000; i <= 19999; i += 1111)
    {
        // yyyy/mm/dd
        PANEL_update_digit(attr, SEG(0), i, 4, true);
        PANEL_update_digit(attr, SEG(5), i, 2, true);
        PANEL_update_digit(attr, SEG(7), i, 2, true);
        PANEL_update_wday(attr, 1 << (i % 7));
        // hh:nn
        PANEL_update_digit(attr, SEG(10), i, 4, true);
        // humidity / tmpr
        PANEL_update_digit(attr, SEG(14), i, 2, true);
        PANEL_update_digit(attr, SEG(16), i, 3, true);
    }
}

void PANEL_attr_set_flags(struct PANEL_attr_t *attr, uint32_t flags)
{
    if (PANEL_IND_1 & flags)
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_1;
    if (PANEL_IND_2 & flags)
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_2;
    if (PANEL_IND_3 & flags)
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_3;
    if (PANEL_IND_4 & flags)
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_4;
    if (PANEL_PERCENT & flags)
        attr->cache.flags = attr->cache.flags | FLAG_IND_PERCENT;
}

void PANEL_attr_unset_flags(struct PANEL_attr_t *attr, uint32_t flags)
{
    if (PANEL_IND_1 & flags)
        attr->cache.flags &= ~FLAG_IND_1;
    if (PANEL_IND_2 & flags)
        attr->cache.flags &= ~FLAG_IND_2;
    if (PANEL_IND_3 & flags)
        attr->cache.flags &= ~FLAG_IND_3;
    if (PANEL_IND_4 & flags)
        attr->cache.flags &= ~FLAG_IND_4;
    if (PANEL_PERCENT & flags)
        attr->cache.flags &= ~FLAG_IND_PERCENT;

    if (0 == (attr->cache.flags & FLAG_INDICATES))
        attr->cache.flags &= ~FLAG_IND_ALARM;
}

int PANEL_update(struct PANEL_attr_t *attr)
{
    int retval = 0;

    // update date
    if (0 == retval)
    {
        attr->blinky_mask ^= PANEL_DATE & attr->blinky_parts;
        attr->cache.flags = attr->cache.flags | FLAG_IND_YEAR | FLAG_IND_MONTH | FLAG_IND_MDAY;

        if (PANEL_YEAR & attr->blinky_mask)
        {
            attr->cache.year = -1;
            attr->cache.flags &= ~FLAG_IND_YEAR;
        }
        if (PANEL_MONTH & attr->blinky_mask)
        {
            attr->cache.month = -1;
            attr->cache.flags &= ~FLAG_IND_MONTH;
        }
        if (PANEL_DAY & attr->blinky_mask)
        {
            attr->cache.mday = -1;
            attr->cache.flags &= ~FLAG_IND_MDAY;
        }

        if (0 == retval && attr->cache.year != attr->set.year)
        {
            if (0 == (retval = PANEL_update_digit(attr, SEG(0), attr->cache.year, 4, false)))
                attr->set.year = attr->cache.year;
        }
        if (0 == retval && attr->set.month != attr->cache.month)
        {
            if (0 == (retval = PANEL_update_digit(attr, SEG(5), attr->cache.month, 2, false)))
                attr->set.month = attr->cache.month;
        }
        if (0 == retval && attr->set.mday != attr->cache.mday)
        {
            if (0 == (retval = PANEL_update_digit(attr, SEG(7), attr->cache.mday, 2, false)))
                attr->set.mday = attr->cache.mday;
        }
    }

    // update wdays
    if (0 == retval)
    {
        attr->blinky_mask ^= PANEL_WDAYS & attr->blinky_parts;

        // if (attr->cache.wdays != attr->set.wdays)
        {
            attr->set.wdays = attr->cache.wdays;
            retval = PANEL_update_wday(attr, attr->cache.wdays);
        }
    }

    // update time
    if (0 == retval)
    {
        enum LOCALE_hfmt_t hfmt = attr->locale->hfmt;
        if (HFMT_DEFAULT == hfmt)
            hfmt = LOCALE_hfmt();

        attr->blinky_mask ^= PANEL_TIME & attr->blinky_parts;
        attr->cache.flags = (attr->cache.flags & ~(FLAG_IND_AM | FLAG_IND_PM)) | FLAG_IND_SEC;

        int16_t mtime = attr->cache.mtime;
        int8_t hour;
        int8_t min;

        if (0 <= mtime)
        {
            hour = (int8_t)(mtime / 100);
            min = (int8_t)(mtime % 100);

            if (1200 > mtime)
                attr->cache.flags |= FLAG_IND_AM;
            else
                attr->cache.flags |= FLAG_IND_PM;

            if (HFMT_12 == hfmt)
            {
                hour %= 12;
                hour = 0 == hour ? 12 : hour;
                mtime = (int16_t)(100 * hour + min);
            }
        }
        else
        {
            hour = -('-');
            min = -('-');
        }

        if (PANEL_HOUR & attr->blinky_parts)
        {
            if (PANEL_HOUR & attr->blinky_mask)
            {
                hour = -1;
                mtime = mtime % 60;
            }
            retval = PANEL_update_digit(attr, SEG(10), hour, 2, HFMT_24 == hfmt);
        }

        if (PANEL_MINUTE & attr->blinky_mask)
        {
            min = -1;
            mtime = (int16_t)((mtime / 100) * 100 + 99);
        }

        if (0 == retval && attr->set.mtime != mtime)
        {
            if ((int16_t)hour * 100 + min != mtime || mtime < 100)
            {
                PANEL_update_digit(attr, SEG(10), hour, 2, HFMT_24 == hfmt);
                retval = PANEL_update_digit(attr, SEG(12), min, 2, true);
            }
            else
                retval = PANEL_update_digit(attr, SEG(10), mtime, 4, HFMT_24 == hfmt);

            if (0 == retval)
                attr->set.mtime = mtime;
        }
    }

    // update humidity
    if (0 == retval)
    {
        if (0 == (PANEL_HUMIDITY & attr->disable_parts))
        {
            attr->cache.flags = attr->cache.flags | FLAG_IND_HUMIDITY | FLAG_IND_PERCENT;

            if (attr->cache.humidity != attr->set.humidity)
            {
                attr->set.humidity = attr->cache.humidity;

                if (100 <= attr->set.humidity) attr->set.humidity = 99;
                retval = PANEL_update_digit(attr, SEG(14), attr->set.humidity, 2, false);
            }
        }
        else
        {
            attr->cache.flags &= ~FLAG_IND_HUMIDITY;

            if (100 <= attr->set.humidity) attr->set.humidity = 99;
            retval = PANEL_update_digit(attr, SEG(14), attr->set.humidity, 2, false);
        }

        if (PANEL_HUMIDITY & attr->blinky_parts)
        {
            attr->blinky_mask ^= PANEL_HUMIDITY & attr->blinky_parts;

            if ((PANEL_HUMIDITY & attr->blinky_mask) && -1 != attr->set.humidity)
            {
                attr->set.humidity = -1;
                retval = PANEL_update_digit(attr, SEG(14), attr->set.humidity, 2, false);
            }
        }
    }

    // update tmpr
    if (0 == retval)
    {
        if (0 == (PANEL_TMPR & attr->disable_parts))
        {
            attr->cache.flags = (attr->cache.flags & ~(FLAG_IND_TMPR_F | FLAG_IND_TMPR_C)) | FLAG_IND_TMPR;

            int16_t tmpr;
            if (FAHRENHEIT == attr->locale->tmpr_unit)
            {
                attr->cache.flags |= FLAG_IND_TMPR_F;
                tmpr = TMPR_fahrenheit(attr->cache.tmpr);
            }
            else
            {
                attr->cache.flags |= FLAG_IND_TMPR_C;
                tmpr = attr->cache.tmpr;
            }

            if (tmpr != attr->set.tmpr)
                retval = PANEL_update_tmpr(attr, tmpr);
        }
        else
        {
            attr->cache.flags &= ~(FLAG_IND_TMPR_F | FLAG_IND_TMPR_C | FLAG_IND_TMPR);
            retval = PANEL_update_digit(attr, SEG(16), attr->set.tmpr, 3, true);
        }

        if (PANEL_TMPR & attr->blinky_parts)
        {
            attr->blinky_mask ^= PANEL_TMPR & attr->blinky_parts;

            if ((PANEL_TMPR & attr->blinky_mask) && -1 != attr->set.tmpr)
            {
                attr->set.tmpr = -1;
                retval = PANEL_update_digit(attr, SEG(16), attr->set.tmpr, 3, true);
            }
        }

        // blinky tmpr unit
        if (PANEL_TMPR_UNIT & attr->blinky_parts)
        {
            attr->blinky_mask ^= PANEL_TMPR_UNIT & attr->blinky_parts;

            if (! (PANEL_TMPR_UNIT & attr->blinky_mask))
            {
                if (FAHRENHEIT == attr->locale->tmpr_unit)
                    attr->cache.flags |= FLAG_IND_TMPR_F;
                else
                    attr->cache.flags |= FLAG_IND_TMPR_C;
            }
            else
                attr->cache.flags = attr->cache.flags & ~(FLAG_IND_TMPR_C | FLAG_IND_TMPR_F);
        }
    }

    // blinky alarm 1~4
    /*
    if (PANEL_INDICATES & attr->blinky_parts)
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_INDICATES;
    */

    if (PANEL_IND_1 & attr->blinky_parts)
    {
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_1;
        attr->blinky_mask ^= PANEL_IND_1 & attr->blinky_parts;

        if (PANEL_IND_1 & attr->blinky_mask)
            attr->cache.flags &= ~(FLAG_IND_ALARM | FLAG_IND_1);
    }
    if (PANEL_IND_2 & attr->blinky_parts)
    {
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_2;
        attr->blinky_mask ^= PANEL_IND_2 & attr->blinky_parts;

        if (PANEL_IND_2 & attr->blinky_mask)
            attr->cache.flags &= ~(FLAG_IND_ALARM | FLAG_IND_2);
    }
    if (PANEL_IND_3 & attr->blinky_parts)
    {
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_3;
        attr->blinky_mask ^= PANEL_IND_3 & attr->blinky_parts;

        if (PANEL_IND_3 & attr->blinky_mask)
            attr->cache.flags &= ~(FLAG_IND_ALARM | FLAG_IND_3);
    }
    if (PANEL_IND_4 & attr->blinky_parts)
    {
        attr->cache.flags = attr->cache.flags | FLAG_IND_ALARM | FLAG_IND_4;
        attr->blinky_mask ^= PANEL_IND_4 & attr->blinky_parts;

        if (PANEL_IND_4 & attr->blinky_mask)
            attr->cache.flags &= ~(FLAG_IND_ALARM | FLAG_IND_4);
    }

    if (PANEL_INDICATES == (PANEL_INDICATES & attr->disable_parts))
    {
        attr->cache.flags = attr->cache.flags & ~(
            FLAG_IND_ALARM |
            FLAG_INDICATES
        );
    }
    else
    {
        if (PANEL_IND_1 & attr->disable_parts)
            attr->cache.flags &= ~FLAG_IND_1;
        if (PANEL_IND_2 & attr->disable_parts)
            attr->cache.flags &= ~FLAG_IND_2;
        if (PANEL_IND_3 & attr->disable_parts)
            attr->cache.flags &= ~FLAG_IND_3;
        if (PANEL_IND_4 & attr->disable_parts)
            attr->cache.flags &= ~FLAG_IND_4;
    }

    if (0 == retval && attr->set.flags != attr->cache.flags)
    {
        attr->set.flags = attr->cache.flags;

        uint8_t buf[1 + sizeof(uint32_t)] = {0};
        buf[0] = SEG(19);
        memcpy(&buf[1], &attr->set.flags, sizeof(attr->set.flags));

        retval = PANEL_HAL_write(attr->da0_fd, &buf, sizeof(buf));
    }
    return retval;
}

/****************************************************************************
 *  @internal
 ****************************************************************************/
static int PANEL_update_wday(struct PANEL_attr_t *attr, int8_t wdays)
{
    static uint16_t const __xlat_wday[] =
    {
        [0] = 1 <<  8 | 1 <<  9,
        [1] = 1 <<  4 | 1 <<  5,
        [2] = 1 <<  6 | 1 <<  7,
        [3] = 1 <<  0 | 1 <<  1,
        [4] = 1 <<  2 | 1 <<  3,
        [5] = 1 << 12 | 1 << 13,
        [6] = 1 << 14 | 1 << 15,
    };
    static uint16_t const __xlat_wday_blinky[] =
    {
        [0] = 1 <<  8,
        [1] = 1 <<  4,
        [2] = 1 <<  6,
        [3] = 1 <<  0,
        [4] = 1 <<  2,
        [5] = 1 << 12,
        [6] = 1 << 14,
    };

    int retval = 0;
    uint8_t buf[1 + sizeof(uint16_t)] = {0};

    if (0 <= wdays)
    {
        uint16_t xlat = 0;
        for (unsigned w = 0; w < 7; w ++)
        {
            if (wdays & (1 << w))
            {
                xlat |= __xlat_wday[w];

                if (((PANEL_WDAY_SUNDAY << w) & attr->blinky_parts) &&
                    (PANEL_WDAY_SUNDAY << w) & attr->blinky_mask)
                {
                    xlat &= (uint16_t)~__xlat_wday_blinky[w];
                }
            }
            else if ((PANEL_WDAY_SUNDAY << w) & attr->blinky_parts)
            {
                if ((PANEL_WDAY_SUNDAY << w) & attr->blinky_mask)
                    xlat &= (uint16_t)~__xlat_wday[w];
                else
                    xlat |= __xlat_wday[w];
            }
        }
        memcpy(&buf[1], &xlat, sizeof(xlat));

        if (0 == retval)
        {
            buf[0] = SEG(9);
            retval = PANEL_HAL_write(attr->da0_fd, &buf, sizeof(buf));
        }

        if (0 == retval)
        {
            buf[0] = SEG(21);
            retval = PANEL_HAL_write(attr->da0_fd, &buf, sizeof(buf));
        }
    }
    return retval;
}

static uint8_t const __xlat_digit[] =
{
    [0] = COM_A | COM_B | COM_C | COM_D | COM_E | COM_F,
    [1] = COM_B | COM_C,
    [2] = COM_A | COM_B | COM_G | COM_E | COM_D,
    [3] = COM_A | COM_B | COM_C | COM_D | COM_G,
    [4] = COM_F | COM_G | COM_B | COM_C,
    [5] = COM_A | COM_F | COM_G | COM_C | COM_D,
    [6] = COM_A | COM_F | COM_G | COM_E | COM_C | COM_D,
    [7] = COM_A | COM_B | COM_C,
    [8] = COM_A | COM_B | COM_C | COM_D | COM_E | COM_F | COM_G,
    [9] = COM_A | COM_B | COM_C | COM_D | COM_F | COM_G,
};
static uint16_t const __xlat_digit_b[] =
{
    [0] = COM_AA | COM_BB | COM_CC | COM_DD | COM_EE | COM_FF,
    [1] = COM_BB | COM_CC,
    [2] = COM_AA | COM_BB | COM_GG | COM_EE | COM_DD,
    [3] = COM_AA | COM_BB | COM_CC | COM_DD | COM_GG,
    [4] = COM_FF | COM_GG | COM_BB | COM_CC,
    [5] = COM_AA | COM_FF | COM_GG | COM_CC | COM_DD,
    [6] = COM_AA | COM_FF | COM_GG | COM_EE | COM_CC | COM_DD,
    [7] = COM_AA | COM_BB | COM_CC,
    [8] = COM_AA | COM_BB | COM_CC | COM_DD | COM_EE | COM_FF | COM_GG,
    [9] = COM_AA | COM_BB | COM_CC | COM_DD | COM_FF | COM_GG,
};

static int PANEL_update_tmpr(struct PANEL_attr_t *attr, int16_t tmpr)
{
    // max display is -9.9
    if (-99 > tmpr)
        tmpr = attr->set.tmpr = attr->cache.tmpr = -99;
    else if (999 < tmpr)
        tmpr = attr->set.tmpr = attr->cache.tmpr = 999;

    attr->set.tmpr = tmpr;

    uint8_t buf[8] = {0};
    unsigned idx = 1;

    buf[0] = SEG(16);

    if (0 > tmpr)
    {
        buf[idx] = COM_G;
        idx += 2;

        tmpr = (int16_t)abs(tmpr);
    }

    if (99 < tmpr)
    {
        buf[idx] = __xlat_digit[tmpr / 100];
        idx += 2;

        buf[idx] = __xlat_digit[tmpr % 100 / 10];
        idx += 2;
    }
    else
    {
        idx = 3;
        buf[idx] = __xlat_digit[tmpr / 10];
        idx += 2;
    }
    buf[idx] = __xlat_digit[tmpr % 10];

    return PANEL_HAL_write(attr->da0_fd, &buf, 7);
}

static int PANEL_update_digit(struct PANEL_attr_t *attr, uint8_t seg, int no, size_t count, bool lead_zero)
{
    uint8_t buf[1 + count * sizeof(uint16_t)];
    memset(buf, 0, 1 + count * sizeof(uint16_t));

    buf[0] = seg;
    count *= 2;

    if (0 <= no)
    {
        if (SEG(10) <= seg && SEG(13) >= seg)
        {
            for (int i = (int)count - 1; i >= 0; i -= 2)
            {
                uint16_t xlat = __xlat_digit_b[no % 10];
                memcpy(&buf[i], &xlat, sizeof(xlat));

                no /= 10;
                if (! lead_zero && 0 == no) break;
            }
        }
        else
        {
            for (int i = (int)count - 1; i >= 0; i -= 2)
            {
                buf[i] = __xlat_digit[no % 10];

                no /= 10;
                if (! lead_zero && 0 == no) break;
            }
        }
    }
    else if ('-' == abs(no))
    {
        uint16_t xlat;

        if (SEG(10) <= seg && SEG(13) >= seg)
        {
            xlat = COM_GG;

            for (int i = (int)count - 1; i >= 0; i -= 2)
            {
                memcpy(&buf[i], &xlat, sizeof(xlat));

                no /= 10;
                if (! lead_zero && 0 == no) break;
            }
        }
        else
        {
            for (int i = (int)count - 1; i >= 0; i -= 2)
                buf[i] = COM_G;
        }
    }

    return PANEL_HAL_write(attr->da0_fd, &buf, count + 1);
}
