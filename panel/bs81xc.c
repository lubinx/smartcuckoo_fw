#include <errno.h>
#include <stropts.h>
#include <unistd.h>

#include <i2c.h>
#include "bs81xc.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
    #define DA                          (0x50)

/****************************************************************************
 *  @implements
 ****************************************************************************/
int BS81xC_createfd(void *const dev, uint16_t kbps)
{
    int fd = I2C_createfd(dev, DA, kbps, 0, 0xFF);

    uint32_t timeo = 500;
    ioctl(fd, OPT_RD_TIMEO, &timeo);
    ioctl(fd, OPT_WR_TIMEO, &timeo);

    char buf[22] = {0};
    lseek(fd, 0xB0, SEEK_SET);
    read(fd, buf, sizeof(buf));

    // 设置非省电模式
    buf[0] |= 0x02;
    // 设置高感度
    buf[3] |= 0x01;
    // 设置每个键触发门坎值
    for (int i = 5; i < 19; i ++) buf[i] = 0x08;
    // 求和校检
    buf[21] = 0; for (int i = 0; i < 21; i ++) buf[21] += buf[i];

    lseek(fd, 0xB0, SEEK_SET);
    write(fd, buf, sizeof(buf));

    return fd;
}

int BS81xC_read_key(int devfd, uint32_t *key)
{
    uint16_t val;
    lseek(devfd, 0x08, SEEK_SET);

    if (sizeof(val) == read(devfd, &val, sizeof(val)))
    {
        *key = val;
        return 0;
    }
    else
    {
        *key = 0;
        return errno;
    }
}
