#include <errno.h>
#include <stropts.h>
#include <unistd.h>

#include <i2c.h>
#include "ioext.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
    #define DA                          (0x20)
    #define INPUT_DEFAULT               (0x1FF0U)

/****************************************************************************
 *  @implements
 ****************************************************************************/
int IOEXT_createfd(void *i2c_dev, uint16_t kbps)
{
    int fd = I2C_createfd(i2c_dev, DA, kbps, 0, 0x7);

    uint32_t timeo = 500;
    ioctl(fd, OPT_RD_TIMEO, &timeo);
    ioctl(fd, OPT_WR_TIMEO, &timeo);

    return fd;
}

int IOEXT_read_key(int devfd, uint32_t *key)
{
    uint16_t val;
    lseek(devfd, 0, SEEK_SET);

    if (sizeof(val) == read(devfd, &val, sizeof(val)))
    {
        *key = 0x1FF0U & ~val;
        return 0;
    }
    else
    {
        I2C_generic_reset(devfd);

        *key = 0;
        return errno;
    }
}
