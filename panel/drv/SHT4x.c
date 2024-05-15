#include <ultracore.h>

#include <stdbool.h>
#include <stropts.h>
#include <i2c.h>
#include <hash/crc8.h>

#include "SHT4x.h"

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
struct SHT4X_context SHT4X_context;

/***************************************************************************
 *  @export
 ***************************************************************************/
int SHT4X_open(void *const dev, uint16_t da, uint16_t kbps)
{
    msleep(SHT4X_POWER_UP_TIMEO);
    int fd = I2C_createfd(dev, da, kbps, 0, 0);
    int timeout = 10;

    ioctl(fd, OPT_RD_TIMEO, &timeout);
    ioctl(fd, OPT_WR_TIMEO, &timeout);

    if (0 < fd)
    {
        if (0 != write_command(fd, 0x94))       // soft reset
            goto sht4x_init_failure;

        msleep(SHT4X_POWER_UP_TIMEO);

        /*
        write_command(fd, 0x89);
        msleep(SHT4X_POWER_UP_TIMEO);

        uint16_t d1, d2;
        if (-1 == read_reponse(fd, &d1, &d2))
            goto sht4x_init_failure;
        */

        if (false)
        {
sht4x_init_failure:
            close(fd);
            fd = -1;
        }
    }
    return fd;
}

/***************************************************************************
 *  @internal
 ***************************************************************************/
int SHT4X_start_convert(int fd)
{
    if (0 < fd)
    {
        SHT4X_context.timeout = SHT4X_HIGH_PRECISION_TIMEO;
        int retval = write_command(fd, 0xE0);

        if (0 == retval)
            SHT4X_context.tick = clock();

        return retval;
    }
    else
        return -1;
}

int SHT4X_read(int fd, int16_t *tmpr, uint8_t *humidity)
{
    if (0 >= fd)
        return __set_errno_neg(ENODEV);

    if (0 == SHT4X_context.tick)
        SHT4X_start_convert(fd);

    uint16_t d1 = 0, d2 = 0;

    int retval = read_reponse(fd, &d1, &d2);
    SHT4X_context.tick = 0;

    if (0 == retval)
    {
        SHT4X_context.tmpr = (int16_t)(d1 * 1750 / 65535 - 450);
        SHT4X_context.uncropped_humidity = d2 * 1250 / 65535 - 60;
    }
    if (SHT4X_context.uncropped_humidity < 0 || SHT4X_context.uncropped_humidity > 1000)
    {
        write_command(fd, 0x94);

        if (SHT4X_context.uncropped_humidity < 0)
            SHT4X_context.uncropped_humidity = 0;
        else if (SHT4X_context.uncropped_humidity > 1000)
            SHT4X_context.uncropped_humidity = 1000;
    }

    *tmpr = (int16_t)SHT4X_context.tmpr;
    *humidity = (uint8_t)((SHT4X_context.uncropped_humidity + 5) / 10);

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

    if (SHT4X_context.tick)
    {
        uint32_t diff = clock() - SHT4X_context.tick;

        if (diff < SHT4X_context.timeout)
            msleep(SHT4X_context.timeout - diff);
    }

    if (sizeof(buf) != read(fd, buf, sizeof(buf)))
    {
        int err = errno;
        write_command(fd, 0x94);

        return __set_errno_neg(err);
    }

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
