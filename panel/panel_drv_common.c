#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <i2c.h>
#include "panel_i2c_drv.h"

/****************************************************************************
 *  attributes
 ****************************************************************************/
void PANEL_attr_init(struct PANEL_attr_t *attr, void *i2c_dev, struct SMARTCUCKOO_locale_t const *locale)
{
    memset(attr, 0, sizeof(*attr));
    memset(&attr->set, 0xFF, sizeof(attr->set));

    attr->locale = locale;
    PANEL_HAL_init(attr, i2c_dev);
}

void PANEL_attr_set_blinky(struct PANEL_attr_t *attr, uint32_t parts)
{
    attr->blinky_parts = parts;
    attr->blinky_mask = parts;
}

void PANEL_attr_set_disable(struct PANEL_attr_t *attr, uint32_t parts)
{
    attr->disable_parts = parts;
}

void PANEL_attr_set_tm(struct PANEL_attr_t *attr, struct tm *dt)
{
    attr->cache.year = (int16_t)dt->tm_year + 1900;
    attr->cache.month = (int8_t)dt->tm_mon + 1;
    attr->cache.mday = (int8_t)dt->tm_mday;
    attr->cache.wdays = (int8_t)(1 << dt->tm_wday);

    attr->cache.mtime = (int16_t)(dt->tm_hour * 100 + dt->tm_min);
}

void PANEL_attr_set_datetime(struct PANEL_attr_t *attr, time_t ts)
{
    struct tm dt;
    localtime_r(&ts, &dt);

    PANEL_attr_set_tm(attr, &dt);
}

void PANEL_attr_set_mdate(struct PANEL_attr_t *attr, int32_t mdate)
{
    time_t ts;

    if (0 > mdate)
    {
        attr->cache.year = -('-');
        attr->cache.month = -('-');
        attr->cache.mday = -('-');
        attr->cache.wdays = 0;
    }
    else
    {
        struct tm dt = {0};

        dt.tm_year = (int)(mdate / 10000 - 1900);
        dt.tm_mon = (int)((mdate % 10000) / 100 - 1);
        dt.tm_mday = (int)(mdate % 100);

        ts = mktime(&dt) + (time_t)((attr->cache.mtime / 100) * 3600 + attr->cache.mtime % 60);
        localtime_r(&ts, &dt);

        PANEL_attr_set_tm(attr, &dt);
    }
}

void PANEL_attr_set_mtime(struct PANEL_attr_t *attr, int16_t mtime)
{
    if (0 > mtime)
        attr->cache.mtime = -('-');
    else
        attr->cache.mtime = mtime;
}

void PANEL_attr_set_wdays(struct PANEL_attr_t *attr, int8_t wdays)
{
    attr->cache.wdays = wdays;
}

void PANEL_attr_set_ringtone_id(struct PANEL_attr_t *attr, int16_t id)
{
    attr->cache.tmpr = id;
}

void PANEL_attr_set_tmpr(struct PANEL_attr_t *attr, int16_t tmpr)
{
    attr->std_tmpr = tmpr;
}

void PANEL_attr_set_humidity(struct PANEL_attr_t *attr, int8_t humidity)
{
    attr->cache.humidity = humidity;
}

/****************************************************************************
 *  HAL
 ****************************************************************************/
int PANEL_HAL_write(int fd, void const *buf, size_t count)
{
    int retval = (count == (size_t)write(fd, buf, count));
    msleep(10);

    if (retval)
        return 0;
    else
        return errno;
}
