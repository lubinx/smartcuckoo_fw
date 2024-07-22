#include <ultracore.h>

#include <stropts.h>
#include <i2c.h>

// #include <hash/crc8.h>
#include "AHT2X.h"

/***************************************************************************
 *  @internal
 ***************************************************************************/
static int AHT2X_reset_reg(int fd, uint8_t reg);

/***************************************************************************
 *  @export
 ***************************************************************************/
int AHT2X_open(void *const dev, uint16_t da, uint16_t kbps, uint16_t powerup_timeout)
{
    if (powerup_timeout)
        msleep(powerup_timeout);

    int fd = I2C_createfd(dev, da, kbps, 0, 0);
    {
        uint32_t timeout = 5;

        ioctl(fd, OPT_RD_TIMEO, &timeout);
        ioctl(fd, OPT_WR_TIMEO, &timeout);
    }

    if (0 < fd)
    {
        uint8_t stat;
        if (sizeof(stat) == read(fd, &stat, sizeof(stat)))
        {
            if (0x18 != (0x18 & stat))
            {
                if (0 != AHT2X_reset_reg(fd, 0x1B))
                    goto aht2x_init_failure;
                if (0 != AHT2X_reset_reg(fd, 0x1C))
                    goto aht2x_init_failure;
                if (0 != AHT2X_reset_reg(fd, 0x1E))
                    goto aht2x_init_failure;
            }
        }
        else
        {
aht2x_init_failure:
            close(fd);
            fd = -1;
        }
    }
    return fd;
}

int AHT2X_start_convert(int fd)
{
    uint8_t buf[] = {0xAC, 0x33, 0};

    if (3 == write(fd, buf, 3))
        return 0;
    else
        return errno;
}

int AHT2X_read(int fd, int16_t *tmpr, uint8_t *humidity)
{
    uint8_t stat;

    if (sizeof(stat) != read(fd, &stat, sizeof(stat)))
        return errno;

    if (0 == (0x80 & stat))
    {
        uint8_t buf[7];

        if (sizeof(buf) == read(fd, buf, sizeof(buf)))
        {
            int val;

            val = (buf[3] & 0xF0) >>  4 | buf[1] << 12 | buf[2] << 4;
            *humidity = (uint8_t)(val * 100 / 1048576);

            val = (buf[3] & 0x0F) << 16 | buf[4] << 8 | buf[5];
            *tmpr = (int16_t)(val * (200 * 10) / 1048576 - 500);

            return 0;
        }
        else
            return errno;
    }
    else
        return EAGAIN;
}

/***************************************************************************
 *  @internal
 ***************************************************************************/
static int AHT2X_reset_reg(int fd, uint8_t reg)
{
    uint8_t buf[3];
    {
        buf[0] = reg;
        buf[1] = 0;
        buf[2] = 0;

        if (sizeof(buf) != write(fd, &buf, sizeof(buf)))
            goto aht2x_reset_failure;
    }
    msleep(10);

    {
        if (sizeof(buf) != read(fd, &buf, sizeof(buf)))
            goto aht2x_reset_failure;
    }
    msleep(10);

    {
        buf[0] = 0xB0 | reg;

        if (sizeof(buf) != write(fd, &buf, sizeof(buf)))
            goto aht2x_reset_failure;
    }
    msleep(10);

    if (false)
    {
aht2x_reset_failure:
        return errno;
    }
    else
        return 0;
}
