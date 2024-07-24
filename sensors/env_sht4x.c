#include <ultracore.h>

#include <stdbool.h>
#include <stropts.h>
#include <gpio.h>
#include <i2c.h>
#include <hash/crc8.h>

#include "env.h"

/***************************************************************************
 *  @def
 ***************************************************************************/
    #define SHT4X_A_DA                  (0x44)
    #define SHT4X_B_DA                  (0x45)

    #define SHT4X_POWER_UP_TIMEO        (2)
    #define SHT4X_HIGH_PRECISION_TIMEO  (9)
    #define SHT4X_MED_PRECISION_TIMEO   (5)
    #define SHT4X_LOW_PRECISION_TIMEO   (2)

/***************************************************************************
 *  @internal
 ***************************************************************************/
static int write_command(int fd, uint8_t cmd);
static int read_reponse(int fd, uint16_t *d1, uint16_t *d2);

struct SHT4X_context
{
    clock_t tick;
    uint32_t timeout;

    int tmpr;
    int uncropped_humidity;
};
struct SHT4X_context context;

/***************************************************************************
 *  @export
 ***************************************************************************/
int ENV_sensor_createfd(void *i2c_dev, uint16_t kbps)
{
    int fd = I2C_createfd(i2c_dev, SHT4X_A_DA, kbps, 0, 0);
    {
        int timeout = 10;

        ioctl(fd, OPT_RD_TIMEO, &timeout);
        ioctl(fd, OPT_WR_TIMEO, &timeout);
    }
    return fd;
}

int ENV_sensor_start_convert(int fd)
{
    if (0 < fd)
    {
        context.timeout = SHT4X_HIGH_PRECISION_TIMEO;
        int retval = write_command(fd, 0xFD);

        // context.timeout = SHT4X_MED_PRECISION_TIMEO;
        // int retval = write_command(fd, 0xF6);

        // context.timeout = SHT4X_LOW_PRECISION_TIMEO;
        // int retval = write_command(fd, 0xE0);

        // context.timeout = 100;        // heater 0.1s
        // int retval = write_command(fd, 0x32);

        if (0 == retval)
            context.tick = clock();
        else
            NVIC_SystemReset();

        return retval;
    }
    else
        return -1;
}

int ENV_sensor_read(int fd, int16_t *tmpr, uint8_t *humidity)
{
    if (0 >= fd)
        return __set_errno_neg(ENODEV);

    uint16_t d1 = 0, d2 = 0;

    int retval = read_reponse(fd, &d1, &d2);
    context.tick = 0;

    if (0 == retval)
    {
        context.tmpr = (int16_t)(d1 * 1750 / 65535 - 450);
        context.uncropped_humidity = d2 * 1250 / 65535 - 60;

        if (context.uncropped_humidity < 0 || context.uncropped_humidity > 1000)
        {
            if (context.uncropped_humidity < 0)
                context.uncropped_humidity = 0;
            else if (context.uncropped_humidity > 1000)
                context.uncropped_humidity = 1000;
        }
    }

    *tmpr = (int16_t)context.tmpr;
    *humidity = (uint8_t)((context.uncropped_humidity + 5) / 10);

    return retval;
}

static int write_command(int fd, uint8_t cmd)
{
    if (sizeof(cmd) == write(fd, &cmd, sizeof(cmd)))
        return 0;
    else
        return errno;
}

static int read_reponse(int fd, uint16_t *d1, uint16_t *d2)
{
    uint8_t buf[6];

    if (context.tick)
    {
        uint32_t diff = clock() - context.tick;

        if (diff < context.timeout)
            msleep(context.timeout - diff);
    }

    if (sizeof(buf) != read(fd, buf, sizeof(buf)))
        return -1;

    if (d1)
    {
         if (buf[2] == CRC8_hash(&buf[0], 2))
             *d1 = (uint16_t)(buf[0] << 8 | (buf[1]));
         else
            return __set_errno_neg(EIO);
    }
    if (d2)
    {
        if (buf[5] == CRC8_hash(&buf[3], 2))
            *d2 = (uint16_t)(buf[3] << 8 | buf[4]);
        else
            return __set_errno_neg(EIO);
    }
    return 0;
}
