#include <errno.h>
#include <stropts.h>
#include <unistd.h>

#include <i2c.h>
#include "xl9535.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
    #define DA                          (0x20)
    #define INPUT_DEFAULT               (0x1FF0U)

/****************************************************************************
 *  @implements
 ****************************************************************************/
int XL9535_createfd(void *const dev, uint16_t kbps)
{
    int fd = I2C_createfd(dev, DA, kbps, 0, 0x7);

    uint32_t timeo = 500;
    ioctl(fd, OPT_RD_TIMEO, &timeo);
    ioctl(fd, OPT_WR_TIMEO, &timeo);

    /*
    uint8_t buf[2] = {0};
    lseek(fd, 6, SEEK_SET);
    read(fd, buf, sizeof(buf));
    */

    return fd;
}

int XL9535_read_key(int devfd, uint32_t *key)
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
        *key = 0;
        return errno;
    }
}
