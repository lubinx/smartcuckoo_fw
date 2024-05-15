#include <unistd.h>
#include <pthread.h>
#include <sys/errno.h>

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stropts.h>

#include <i2c.h>
#include "panel.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
    char const *abb_month_names_en[12] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sept", "Oct", "Nov", "Dec"
    };
    char const **abb_month_names = (char const **)&abb_month_names_en;

    char const *weekday_names_en[7] =
    {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    char const **weekday_names = (char const **)&weekday_names_en;
    /*
    char const *abb_month_names_fr[12] =
    {
        "janv", "fevr", "mars", "avril", "mai", "juin", "juil", "aout", "sept", "oct", "nov", "dec"
    };
    char const *abb_month_names_sp[12] =
    {
        "enero", "feb", "marzo", "abr", "mayo", "jun", "jul", "agosto", "sept", "oct", "nov", "dic"
    };
    */

    #define I2C_DEV                     I2C0
    #define DA0                         (0x72)
    #define DA1                         (0x73)
    #define DEF_PWM                     (3)

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
    #define SEG(N)                      (((N) - 1) * 4)

    #define COM_1                       (1UL << 4)
    #define COM_2                       (1UL << 5)
    #define COM_3                       (1UL << 6)
    #define COM_4                       (1UL << 7)
    #define COM_5                       (1UL << 0)
    #define COM_6                       (1UL << 1)
    #define COM_7                       (1UL << 2)
    #define COM_8                       (1UL << 3)
    #define COM_9                       (1UL << 12)
    #define COM_10                      (1UL << 13)
    #define COM_11                      (1UL << 14)
    #define COM_12                      (1UL << 15)
    #define COM_13                      (1UL << 8)
    #define COM_14                      (1UL << 9)
    #define COM_15                      (1UL << 10)
    #define COM_16                      (1UL << 11)

    #define COM_A                       COM_1
    #define COM_B                       COM_2
    #define COM_C                       COM_3
    #define COM_D                       COM_4
    #define COM_E                       COM_5
    #define COM_F                       COM_6
    #define COM_G                       COM_7
    #define COM_H                       COM_8
    #define COM_I                       COM_9
    #define COM_J                       COM_10
    #define COM_K                       COM_11
    #define COM_L                       COM_12
    #define COM_M                       COM_13
    #define COM_N                       COM_14
    #define COM_O                       COM_15
    #define COM_P                       COM_16
    #define COM_AA                      (COM_8  | COM_A)
    #define COM_BB                      (COM_9  | COM_B)
    #define COM_CC                      (COM_10 | COM_C)
    #define COM_DD                      (COM_11 | COM_D)
    #define COM_EE                      (COM_12 | COM_E)
    #define COM_FF                      (COM_13 | COM_F)
    #define COM_GG                      (COM_14 | COM_G)

    #define FLAG_IND_YEAR               COM_1
    #define FLAG_IND_MONTH              (1UL << 21)
    #define FLAG_IND_MDAY               (1UL << 22)

    #define COM_T1                      (COM_1 << 16)
    #define COM_T2                      (COM_2 << 16)
    #define COM_T3                      (COM_3 << 16)
    #define COM_T4                      (COM_4 << 16)
    #define COM_T5                      (COM_5 << 16)
    #define COM_T6                      (COM_6 << 16)
    #define COM_T7                      (COM_7 << 16)
    #define COM_T8                      (COM_8 << 16)
    #define COM_T9                      (COM_9 << 16)
    #define COM_T10                     (COM_10 << 16)
    #define COM_T11                     (COM_11 << 16)
    #define COM_T12                     (COM_12 << 16)
    #define FLAG_IND_AM                 (COM_T7 | COM_T8)
    #define FLAG_IND_PM                 (COM_T9 | COM_T10)
    #define FLAG_IND_SEC                (COM_T11 | COM_T12)

    #define FLAG_IND_ALARM              (COM_5 | COM_6)
    #define FLAG_IND_1            (COM_1 | FLAG_IND_ALARM)
    #define FLAG_IND_2            (COM_2 | FLAG_IND_ALARM)
    #define FLAG_IND_3            (COM_3 | FLAG_IND_ALARM)
    #define FLAG_IND_4            (COM_4 | FLAG_IND_ALARM)

    #define FLAG_IND_HUMIDITY           (COM_8 | COM_9 | COM_7)
    #define FLAG_IND_LINE               (COM_10 | COM_11 | COM_12)

    #define FLAG_IND_TMPR               (COM_T3 | COM_T4 | COM_T5)
    #define FLAG_IND_TMPR_C             (COM_T2)
    #define FLAG_IND_TMPR_F             (COM_T1)

    enum SEG_digit_t
    {
        SEG_HOUR                    = (SEG(1)  | 0x00),
        SEG_TMPR                    = (SEG(4)  | 0x00),
        SEG_HUMIDITY                = (SEG(7)  | 0x00),
        SEG_MINUTE                  = (SEG(15) | 0x80),
    };

/****************************************************************************
 *  @def
 ****************************************************************************/
struct PANEL_context
{
    pthread_mutex_t lock;

    int da0_fd;
    int da1_fd;
    uint8_t pwm;            // 0~15

    uint8_t weekday;
    uint8_t mday;
    uint8_t month;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;

    uint32_t flags;
};

/****************************************************************************
 *  @internal
 ****************************************************************************/
static void *PANEL_clock_thread(pthread_mutex_t *update_lock) __attribute__((noreturn));

static void PANEL_update_date(uint16_t year, uint8_t month, uint8_t mday);
static void PANEL_update_wday(uint8_t wday);
static void PANEL_update_time(uint8_t hour, uint8_t minute);
static void PANEL_update_flags(void);

static int PANEL_update_digit(enum SEG_digit_t seg, int value);
static int PANEL_update_char(uint8_t offset, char const *buf, uint8_t count);
static int PANEL_HAL_write(int fd, void const *buf, size_t count);

// var
static struct PANEL_context PANEL_context = {0};
static uint32_t PANEL_clock_thread_stack[2048 / sizeof(uint32_t)];

/****************************************************************************
 *  @implements
 ****************************************************************************/
void PANEL_attr_init()
{
    PANEL_context.da0_fd = I2C_createfd(I2C_DEV, DA0, 200, 0, 0);
    PANEL_context.da1_fd = I2C_createfd(I2C_DEV, DA1, 200, 0, 0);

    if (0 < PANEL_context.da0_fd)
    {
        uint32_t timeo = 500;
        ioctl(PANEL_context.da0_fd, OPT_WR_TIMEO, &timeo);
    }

    PANEL_context.pwm = DEF_PWM;
    PANEL_context.flags = 0;
    PANEL_context.mday = (uint8_t)-1;
    PANEL_context.hour = (uint8_t)-1;
    PANEL_context.minute = (uint8_t)-1;

    if (true)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&PANEL_context.lock, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    if (true)
    {
        pthread_t id;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setstack(&attr, PANEL_clock_thread_stack, sizeof(PANEL_clock_thread_stack));
        pthread_create(&id, &attr, (void *)PANEL_clock_thread, &PANEL_context.lock);
        pthread_attr_destroy(&attr);
    }
}

int PANEL_pwm(uint8_t val)
{
    if (15 < val)
        return EINVAL;

    if (val != PANEL_context.pwm)
    {
        uint8_t pwm =  PWM(val);
        PANEL_HAL_write(PANEL_context.da0_fd, &pwm, sizeof(pwm));
        PANEL_HAL_write(PANEL_context.da1_fd, &pwm, sizeof(pwm));
    }
    return 0;
}

void PANEL_test(void)
{
    char const str[] = "cdefghijklmnopqrstuvwxyz"; // "*+-/01234567890abcdefghijklmnopqrstuvwxyz";
    PANEL_update_char(0, (char *)str, sizeof(str) - 1);

    for (int i = 10000; i <= 19999; i += 1111)
    {
        PANEL_update_digit(SEG_HOUR, i);
        PANEL_update_digit(SEG_MINUTE, i);
        PANEL_update_digit(SEG_TMPR, i);
        PANEL_update_digit(SEG_HUMIDITY, i);

        msleep(100);
    }
}

void PANEL_update_tmpr(int tmpr, enum TMPR_unit_t deg)
{
    PANEL_context.flags |= FLAG_IND_TMPR;

    if (CELSIUS == deg)
        PANEL_context.flags = (PANEL_context.flags & ~FLAG_IND_TMPR_F) | FLAG_IND_TMPR_C;
    else
        PANEL_context.flags = (PANEL_context.flags & ~FLAG_IND_TMPR_C) | FLAG_IND_TMPR_F;

    PANEL_update_digit(SEG_TMPR, tmpr);
    PANEL_update_flags();
}

void PANEL_update_humidity(int humidity)
{
    PANEL_context.flags |= FLAG_IND_HUMIDITY;

    PANEL_update_digit(SEG_HUMIDITY, humidity);
    PANEL_update_flags();
}

/****************************************************************************
 *  @internal
 ****************************************************************************/
static void *PANEL_clock_thread(pthread_mutex_t *lock)
{
    struct tm tv = {0};
    static uint8_t const __startup[64] = {SYSDIS, COM16PMOS, RCMODE1, SYSEN, PWM(DEF_PWM), LEDON};

    while (1)
    {
        if (0 == PANEL_HAL_write(PANEL_context.da0_fd, __startup, sizeof(__startup)) &&
            0 == PANEL_HAL_write(PANEL_context.da1_fd, __startup, sizeof(__startup)))
        {
            break;
        }
        else
            msleep(1000);
    }
    PANEL_update_flags();

    PANEL_context.flags |= FLAG_IND_ALARM | FLAG_IND_LINE;
    PANEL_update_tmpr(253, CELSIUS);
    PANEL_update_humidity(58);

    while (1)
    {
        time_t ts = time(NULL);
        localtime_r(&ts, &tv);

        /*
        printf("%04d/%02d/%02d %02d:%02d:%02d\n",
            1900 + tv.tm_year, 1 + tv.tm_mon, tv.tm_mday,
            tv.tm_hour, tv.tm_min, tv.tm_sec
        );
        fflush(stdout);
        */

        pthread_mutex_lock(lock);
        {
            PANEL_update_date((uint16_t)tv.tm_year + 1900, (uint8_t)tv.tm_mon + 1, (uint8_t)tv.tm_mday);
            PANEL_update_wday((uint8_t)tv.tm_wday);
            PANEL_update_time((uint8_t)tv.tm_hour, (uint8_t)tv.tm_min);

            PANEL_update_flags();
        }
        pthread_mutex_unlock(lock);

        msleep(1000);
    }
}

static void PANEL_update_date(uint16_t year, uint8_t month, uint8_t mday)
{
    PANEL_context.flags = PANEL_context.flags | FLAG_IND_YEAR | FLAG_IND_MONTH | FLAG_IND_MDAY;

    if (PANEL_context.year != year || PANEL_context.month != month || PANEL_context.mday != mday)
    {
        char buf[18];
        sprintf(buf, "%6s %02u %04u", abb_month_names[month - 1], mday, year);

        if (0 == PANEL_update_char(10, buf, 14))
        {
            PANEL_context.year = year;
            PANEL_context.month = month;
            PANEL_context.mday = mday;
        }
    }
}

static void PANEL_update_wday(uint8_t wday)
{
    if (PANEL_context.weekday != wday)
    {
        if (7 > wday)
        {
            char const *weekday = weekday_names[wday];
            uint8_t count = (uint8_t)(strlen(weekday) % 11);

            if (0 == PANEL_update_char(0, weekday, count))
                PANEL_context.weekday = wday;
        }
    }
}


static void PANEL_update_time(uint8_t hour, uint8_t minute)
{
    int retval = 0;
    PANEL_context.flags |= FLAG_IND_SEC;

    if (PANEL_context.hour != hour)
    {
        if (0 == retval && (0 == (retval = PANEL_update_digit(SEG_HOUR, hour))))
            PANEL_context.hour = hour;

        if (0 == retval)
        {
            if (hour >= 12)
                PANEL_context.flags = (PANEL_context.flags & (uint32_t)~(FLAG_IND_AM)) | FLAG_IND_PM;
            else
                PANEL_context.flags = (PANEL_context.flags & (uint32_t)~(FLAG_IND_PM)) | FLAG_IND_AM;
        }
    }

    if (PANEL_context.minute != minute)
    {
        if (0 == retval && (0 == (retval = PANEL_update_digit(SEG_MINUTE, minute))))
            PANEL_context.minute = minute;
    }
}

static void PANEL_update_flags(void)
{
    static uint32_t flags1 = 0;
    static uint32_t flags2 = 0;

    if (flags1 != (uint16_t)PANEL_context.flags)
    {
        uint8_t buf[1 + sizeof(uint16_t)] = {0};
        uint16_t flags = (uint16_t)PANEL_context.flags;
        buf[0] = SEG(9);

        memcpy(&buf[1], &flags, sizeof(flags));

        if (0 == PANEL_HAL_write(PANEL_context.da0_fd, &buf, sizeof(buf)))
            flags1 = flags;
    }
    if (flags2 != (uint16_t)(PANEL_context.flags >> 16))
    {
        uint8_t buf[1 + sizeof(uint16_t)] = {0};
        uint16_t flags = (uint16_t)(PANEL_context.flags >> 16);
        buf[0] = SEG(3);

        memcpy(&buf[1], &flags, sizeof(flags));

        if (0 == PANEL_HAL_write(PANEL_context.da0_fd, &buf, sizeof(buf)))
            flags2 = flags;
    }
}

static int PANEL_update_digit(enum SEG_digit_t seg, int value)
{
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

    int fd = seg & 0x80 ? PANEL_context.da1_fd : PANEL_context.da0_fd;

    uint8_t buf[1 + 4 * sizeof(uint16_t)] = {0};
    buf[0] = 0x7F & seg;
    int count;

    if (SEG_HOUR == seg || SEG_MINUTE == seg)
    {
        count = 4;

        if (SEG_MINUTE == seg)
        {
            // fill with "00"
            for (int i = count - 1; i >= 0; i -= 2)
            {
                uint16_t xlat = __xlat_digit_b[0];
                memcpy(&buf[count - i], &xlat, sizeof(xlat));
            }
        }

        if (0 < value)
        {
            for (int i = count - 1; i >= 0; i -= 2)
            {
                uint16_t xlat = __xlat_digit_b[value % 10];
                memcpy(&buf[count - i], &xlat, sizeof(xlat));

                value /= 10;
                if (SEG_HOUR == seg && 0 == value) break;
            }
        }
    }
    else if (SEG_TMPR == seg)
    {
        count = 6;

        if (0 < value)
        {
            for (int i = count - 1; i >= 0; i -= 2)
            {
                buf[i] = __xlat_digit[value % 10];

                value /= 10;
                if (0 == value) break;
            }
        }
    }
    else if (SEG_HUMIDITY == seg)
    {
        count = 4;

        if (0 < value)
        {
            for (int i = count - 1; i >= 0; i -= 2)
            {
                buf[count - i] = __xlat_digit[value % 10];

                value /= 10;
                if (0 == value) break;
            }
        }
    }
    else
        return EINVAL;

    return PANEL_HAL_write(fd, &buf, (unsigned)count + 1);
}

static int PANEL_update_char(uint8_t offset, char const *buf, uint8_t count)
{
    static uint16_t const ascii_xlat[] =
    {
        [0] = 0,
        ['!' - ' '] = 0,
        ['"' - ' '] = 0,
        ['#' - ' '] = 0,
        ['$' - ' '] = 0,
        ['%' - ' '] = 0,
        ['&' - ' '] = 0,
        ['\'' - ' '] = COM_K,
        ['(' - ' '] = 0,
        [')' - ' '] = 0,
        ['*' - ' '] = COM_I | COM_K | COM_N | COM_P,
        ['+' - ' '] = COM_J | COM_L | COM_M | COM_O,
        [',' - ' '] = 0,
        ['-' - ' '] = COM_L | COM_M,
        ['.' - ' '] = 0,
        ['/' - ' '] = COM_K | COM_N,
        ['0' - ' '] = COM_A | COM_B | COM_H | COM_C | COM_G | COM_D | COM_F | COM_E,
        ['1' - ' '] = COM_J | COM_O,
        ['2' - ' '] = COM_A | COM_B | COM_C | COM_L | COM_M | COM_G | COM_F | COM_E,
        ['3' - ' '] = COM_A | COM_B | COM_C | COM_L | COM_M | COM_D | COM_F | COM_E,
        ['4' - ' '] = COM_H | COM_J | COM_L | COM_M | COM_O,
        ['5' - ' '] = COM_A | COM_B | COM_H | COM_L | COM_M | COM_D | COM_F | COM_E,
        ['6' - ' '] = COM_A | COM_B | COM_H | COM_L | COM_M | COM_G | COM_D | COM_F | COM_E,
        ['7' - ' '] = COM_A | COM_B | COM_K | COM_N,
        ['8' - ' '] = COM_A | COM_B | COM_H | COM_C | COM_L | COM_M | COM_G | COM_D | COM_F | COM_E,
        ['9' - ' '] = COM_A | COM_B | COM_H | COM_C | COM_L | COM_M | COM_D | COM_F | COM_E,
        [':' - ' '] = 0,
        [';' - ' '] = 0,
        ['<' - ' '] = 0,
        ['=' - ' '] = 0,
        ['>' - ' '] = 0,
        ['?' - ' '] = 0,
        ['A' - ' '] = COM_A | COM_B | COM_H | COM_C | COM_L | COM_M | COM_G | COM_D,
        ['B' - ' '] = COM_A | COM_B | COM_J | COM_C | COM_M | COM_O | COM_D | COM_F | COM_E,
        ['C' - ' '] = COM_A | COM_B | COM_H | COM_G | COM_F | COM_E,
        ['D' - ' '] = COM_A | COM_B | COM_J | COM_C | COM_O | COM_D | COM_F | COM_E,
        ['E' - ' '] = COM_A | COM_B | COM_H | COM_L | COM_M | COM_G | COM_F | COM_E,
        ['F' - ' '] = COM_A | COM_B | COM_H | COM_L | COM_M | COM_G,
        ['G' - ' '] = COM_A | COM_B | COM_H | COM_M | COM_G | COM_D | COM_F | COM_E,
        ['H' - ' '] = COM_H | COM_C | COM_L | COM_M | COM_G | COM_D,
        ['I' - ' '] = COM_A | COM_B | COM_J | COM_O | COM_F | COM_E,
        ['J' - ' '] = COM_J | COM_O | COM_F | COM_G,
        ['K' - ' '] = COM_H | COM_G | COM_L | COM_K | COM_P,
        ['L' - ' '] = COM_J | COM_O | COM_E,
        ['M' - ' '] = COM_H | COM_I | COM_K | COM_C | COM_G | COM_O | COM_D,
        ['N' - ' '] = COM_H | COM_I | COM_C | COM_G | COM_P | COM_D,
        ['O' - ' '] = COM_A | COM_B | COM_H | COM_C | COM_G | COM_D | COM_F | COM_E,
        ['P' - ' '] = COM_A | COM_B | COM_H | COM_K | COM_L | COM_G,
        ['Q' - ' '] = COM_A | COM_B | COM_I | COM_C | COM_M | COM_D,
        ['R' - ' '] = COM_A | COM_B | COM_H | COM_C | COM_L | COM_M | COM_G | COM_P,
        ['S' - ' '] = COM_A | COM_B | COM_H | COM_L | COM_P | COM_F | COM_E,
        ['T' - ' '] = COM_A | COM_B | COM_J | COM_O,
        ['U' - ' '] = COM_H | COM_C | COM_G | COM_D | COM_F | COM_E,
        ['V' - ' '] = COM_H | COM_K | COM_G | COM_N,
        ['W' - ' '] = COM_H | COM_J | COM_C | COM_G | COM_N | COM_P | COM_D,
        ['X' - ' '] = COM_I | COM_K | COM_N | COM_P,
        ['Y' - ' '] = COM_H | COM_C | COM_L | COM_M | COM_D | COM_F | COM_E,
        ['Z' - ' '] = COM_A | COM_B | COM_K | COM_N | COM_F | COM_E,
    };

    int retval = 0;
    count = count > (10 + 14) ? 24 : count;

    // "\' * + - /
    uint8_t idx = 0;

    if (10 > offset)
    {
        char mem[1 + 10 * sizeof(uint16_t)] = {0};
        mem[0] = 0x7F & SEG(19 + 1 - (offset + (count > 10 ? 10 : count)));
        int memsize = sizeof(uint16_t) * (count > 10 ? 10 : count);
        offset = 10;

        for (int i = 1; i < memsize; i += 2)
        {
            uint16_t const *xlat;
            char ch = (char)toupper(buf[idx ++]);

            if (ch >= ' ' && ch < lengthof(ascii_xlat) + ' ')
                xlat = &ascii_xlat[ch - ' '];
            else
                xlat = NULL;

            if (xlat)
                memcpy(&mem[memsize - i], xlat, sizeof(*xlat));
        }

        if(0 != (retval = PANEL_HAL_write(PANEL_context.da0_fd, &mem, (unsigned)memsize + 1)))
            idx = count;
    }

    if (idx < count)
    {
        char mem[1 + 14 * sizeof(uint16_t)] = {0};
        mem[0] = 0x7F & SEG(offset + 1 - 10);
        unsigned memsize = sizeof(uint16_t) * (count - idx);

        for (unsigned i = 1; i < memsize; i += 2)
        {
            uint16_t const *xlat;
            char ch = (char)toupper(buf[idx ++]);

            if (ch >= ' ' && ch < lengthof(ascii_xlat) + ' ')
                xlat = &ascii_xlat[ch - ' '];
            else
                xlat = NULL;

            if (xlat)
                memcpy(&mem[i], xlat, sizeof(*xlat));
        }

        retval = PANEL_HAL_write(PANEL_context.da1_fd, &mem, (unsigned)memsize + 1);
    }

    return retval;
}

static int PANEL_HAL_write(int fd, void const *buf, size_t count)
{
    int retval = (count == (size_t)write(fd, buf, count));

    if (retval)
        return 0;
    else
        return errno;
}
