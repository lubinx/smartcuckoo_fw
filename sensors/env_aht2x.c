#include <ultracore.h>

#include <stropts.h>
#include <i2c.h>
#include "env.h"

/***************************************************************************
 *  @def
 ***************************************************************************/
    #define AHT2X_DA                    (0x38)
    #define AHT2X_ALT_DA                (0x39)

    #define AHT2X_COMMAND_INTV          (1000)
    #define AHT2X_MEASURE_TIMEO         (100)

/***************************************************************************
 *  @internal
 ***************************************************************************/
static uint8_t const cmd_start_convert[] = {0xAC, 0x33, 0};

/***************************************************************************
 *  @export
 ***************************************************************************/
int ENV_sensor_createfd(void *const dev, uint16_t kbps)
{
    int fd = I2C_createfd(dev, AHT2X_DA, kbps, 0, 0);
    {
        uint32_t timeout = 5;

        ioctl(fd, OPT_RD_TIMEO, &timeout);
        ioctl(fd, OPT_WR_TIMEO, &timeout);
    }

    return fd;
}

int ENV_sensor_start_convert(int fd)
{
    if (sizeof(cmd_start_convert) != write(fd, cmd_start_convert, sizeof(cmd_start_convert)))
    {
        int err = errno;
        LOG_error("AHT2X: start convert error %d", err);
        return err;
    }
    else
        return 0;
}

int ENV_sensor_read(int fd, int16_t *tmpr, uint8_t *humidity)
{
    uint8_t stat;
    int retval;

    if (sizeof(stat) != read(fd, &stat, sizeof(stat)))
    {
        retval = errno;
        goto read_exit;
    }

    if (0 != (0x80 & stat))
    {
        retval = ENODATA;
        goto read_exit;
    }

    if (0 == (0x08 & stat))
    {
        // I2C_generic_recovery(fd);
        retval = EAGAIN;
        goto read_exit;
    }

    uint8_t buf[7];
    if (sizeof(buf) == read(fd, buf, sizeof(buf)))
    {
        int val;

        val = (buf[3] & 0x0F) << 16 | buf[4] << 8 | buf[5];
        // RAW => tmpr
        val = (int16_t)(val * (200 * 10) / 1048576 - 500);

        if (-200 < val && 1000 > val)   // validate tmpr range -20.0 ~ 100 °C
        {
            *tmpr = (int16_t)val;

            val = (buf[3] & 0xF0) >>  4 | buf[1] << 12 | buf[2] << 4;
            *humidity = (uint8_t)(val * 100 / 1048576);

            LOG_debug("AHT2X: tmpr %d, humidity %d", *tmpr, *humidity);
            retval = 0;
        }
        else
        {
            LOG_error("AHT2X error RAW tmpr: %d", val);
            retval = ERANGE;
        }
    }
    else
        retval = errno;

read_exit:
    if (0 != retval)
        LOG_error("AHT2X: error %d", retval);
    return retval;
}
