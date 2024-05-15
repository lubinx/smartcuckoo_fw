#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <stropts.h>

#include <gpio.h>
#include <wdt.h>
#include <ultracore/log.h>
#include <ultracore/sema.h>
#include <ultracore/mq.h>

#include "mplayer.h"

#ifdef DEBUG
    #pragma GCC warn "DEBUG mode using USB not SD"
    #pragma GCC optimize("O0")

    #define MEDIA                       "udisk0"
#else
    #define MEDIA                       "sd0"
#endif

static char line_buffer[64];

/***************************************************************************
 * @def
 ***************************************************************************/
#define MPLAYER_WAIT_FOREVER            ((uint32_t)-1)
#define DEF_IDLE_SHUTDOWN_TIMEO         (250)
#define TASK_QUEUE_SIZE                 (10)

// struct WT2605

enum mtask_id_t
{
    /**
     *  power on / off
     *      chip implemented payload
    */
    MPLAYER_TASK_POWER_ON           = 1,

    /**
     *  play / stop
     *      chip implemented payload
     *
     *  marshalling arguments:
     *      int folder
     *      int name
    */
    MPLAYER_TASK_PLAY,
    MPLAYER_TASK_PLAY_LOOP,
    MPLAYER_TASK_STOP,

    /**
     *  pause / resume
     *      chip implemented payload
    */
    MPLAYER_TASK_PAUSE,
    MPLAYER_TASK_RESUME,

    /**
     *  volume
     *      chip implemented payload
     *
     *  marshalling arguments:
     *      uint8_t volume
    */
    MPLAYER_TASK_SET_VOLUME,

    /**
     *  peek file exists
    */
    MPLAYER_TASK_PEEK_FILE,

    /**
     *  fade in / out
     *
     *  unmarshalling arguments:
     *      as_u32[0]   milli-second
    */
    MPLAYER_TASK_FADE,

    /**
     *
    */
    MPLAYER_TASK_PEEK_BUSY,
};

struct mtask_t
{
    struct MQ_message_hdr_t hdr;

    semaphore_t executed;
    int err;

    union
    {
        char cmd[48];
        uint32_t timeout;
    };
};

/***************************************************************************
 * @private
 ***************************************************************************/
struct mplayer_attr_t
{
    int devfd;
    int mqd;

    uint32_t pin_busy;

    enum mplayer_stat_t stat;
    uint32_t avg_latency;

    uint8_t vol_max;
    uint8_t vol_min;
    uint8_t vol_curr;

    uint32_t idle_shutdown_timeo;
};

static __attribute__((noreturn)) void *mplayer_controller_thread(struct mplayer_attr_t *ctx);
static struct mtask_t *mplayer_get_task(void);

// var
static struct mplayer_attr_t mplayer_attr = {0};
static uint32_t mplayer_controller_stack[768 / sizeof(uint32_t)];

/*************************************************************************
 * @implements
**************************************************************************/
int mplayer_initlaize(int devfd, uint32_t pin_busy)
{
    mplayer_gpio_power_on();

    while (true)
    {
        int len = readln(devfd, line_buffer, sizeof(line_buffer));
        if (0 != len)
            LOG_debug("WT2605: %s", line_buffer);

        if (0 == strncmp("+DEVICE:sd0", line_buffer, 11))
            break;
        else
            continue;
    }
    if (true)
    {
        uint32_t timeout = 100;
        ioctl(devfd, OPT_RD_TIMEO, &timeout);
    }

    mplayer_attr.devfd = devfd;
    mplayer_attr.pin_busy = pin_busy;
    mplayer_attr.idle_shutdown_timeo = DEF_IDLE_SHUTDOWN_TIMEO;
    mplayer_attr.mqd = mqueue_create(sizeof(struct mtask_t) - sizeof(struct MQ_message_hdr_t), TASK_QUEUE_SIZE);

    mplayer_attr.vol_max = 31;
    mplayer_attr.vol_min = 1;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, mplayer_controller_stack, sizeof(mplayer_controller_stack));

    pthread_t id;
    pthread_create(&id, &attr, (void *)mplayer_controller_thread, &mplayer_attr);
    pthread_attr_destroy(&attr);

    // mplayer_gpio_power_off();
    return 0;
}

enum mplayer_stat_t mplayer_stat(void)
{
    return mplayer_attr.stat;
}

void mplayer_idle_shutdown(uint32_t timeo)
{
    mplayer_attr.idle_shutdown_timeo = timeo;
}

clock_t mplayer_get_tickcount(void)
{
    return clock();
}

static struct mtask_t *get_play_task(char const *path)
{
    struct mtask_t *task = mplayer_get_task();
    if (task)
    {
        char *filename = strchr(path, '/');
        if (NULL != filename)
        {
            *filename ++ = '\0';
            task->hdr.payload_size = (uint8_t)sprintf(task->cmd, "AT+SPLAY=" MEDIA ",/%s/%s.mp3\r\n", path, filename);
        }
        else
            task->hdr.payload_size = (uint8_t)sprintf(task->cmd, "AT+SPLAY=" MEDIA ",%s.mp3\r\n", path);
    }
    return task;
}

int mplayer_play(char const *path)
{
    struct mtask_t *task = get_play_task(path);
    if (task)
    {
        task->hdr.msgid = MPLAYER_TASK_PLAY;
        return mqueue_post(mplayer_attr.mqd, (void *)task);
    }
    else
        return EAGAIN;
}

int mplayer_playlist_queue(char const *path, uint32_t fade_in_intv)
{
    (void)fade_in_intv;
    struct mtask_t *task = get_play_task(path);
    if (task)
    {
        task->hdr.msgid = MPLAYER_TASK_PLAY;
        return mqueue_post(mplayer_attr.mqd, (void *)task);
    }
    else
        return EAGAIN;
}

bool mplayer_file_exists(char const *path)
{
    struct mtask_t *task = get_play_task(path);
    if (task)
    {
        task->hdr.msgid = MPLAYER_TASK_PEEK_FILE;

        mqueue_post(mplayer_attr.mqd, (void *)task);
        sema_wait(&task->executed);
        return 0 == task->err;
    }
    else
        return false;
}

int mplayer_play_loop(char const *path)
{
    (void)path;
    return 0;
}

int mplayer_pause(void)
{
    return 0;
}

int mplayer_resume(void)
{
    return 0;
}

int mplayer_stop(void)
{
    return 0;
}

int mplayer_set_volume(uint8_t percent)
{
    uint8_t range = mplayer_attr.vol_max - mplayer_attr.vol_min;

    if (20 >= percent)
        mplayer_attr.vol_curr = mplayer_attr.vol_min;
    else
        mplayer_attr.vol_curr = mplayer_attr.vol_min + (uint8_t)(range * (percent - 20) / 80);

    return 0;
}

uint8_t mplayer_volume(void)
{
    uint8_t range = mplayer_attr.vol_max - mplayer_attr.vol_min;
    return 20 + (uint8_t)(((mplayer_attr.vol_curr - mplayer_attr.vol_min) * 80 + range / 2) / range);
}

uint8_t mplayer_volume_inc(void)
{
    /*
    int retval = 0;

    if (mplayer_attr.vol_curr < mplayer_attr.vol_max)
    {
        mplayer_attr.vol_curr ++;

        struct mtask_t *task;
        retval = mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_SET_VOLUME);
    }

    LOG_info("volume %d%%", mplayer_volume());
    return retval;
    */
    return 0;
}

uint8_t mplayer_volume_dec(void)
{
    /*
    int retval = 0;

    if (mplayer_attr.vol_curr > mplayer_attr.vol_min)
    {
        mplayer_attr.vol_curr --;

        struct mtask_t *task;
        retval = mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_SET_VOLUME);
    }

    LOG_info("volume %d%%", mplayer_volume());
    return retval;
    */
    return 0;
}

int mplayer_playlist_clear(void)
{
    return mqueue_flush(mplayer_attr.mqd);
}

__attribute__((weak))
void mplayer_power_callback(bool stat)
{
    ARG_UNUSED(stat);
}

__attribute__((weak))
void mplayer_stopping_callback(void)
{
}

__attribute__((weak))
void mplayer_idle_callback(void)
{
}

/*************************************************************************
 * @private
**************************************************************************/
static struct mtask_t *mplayer_get_task(void)
{
    struct mtask_t *task = (void *)mqueue_get_pool(mplayer_attr.mqd, 0);
    if (task)
    {
        sema_init(&task->executed, 0, 1);
        task->err = 0;
    }
    return task;
}

static uint8_t mqueued_prio(int mqd)
{
    uint16_t msgid;
    uint8_t prio;
    if (0 == mqueue_peek_attr(mqd, &msgid, &prio))
        return prio;
    else
        return (uint8_t)-1;
}

static void *mplayer_controller_thread(struct mplayer_attr_t *attr)
{
    struct mtask_t *task = NULL;
    struct mtask_t *prev_task = NULL;

    while (true)
    {
        if (NULL != task)
        {
            if (task->hdr.prio > mqueued_prio(attr->mqd))
            {
                prev_task = task;
                goto recv_task;
            }
            else
                msleep(10);
        }
        else
        {
        recv_task:
            task = (void *)mqueue_recv(attr->mqd);
            if (task)
            {
                LOG_debug("WT2005 => %s", task->cmd);
                write(attr->devfd, task->cmd, task->hdr.payload_size);
            }
        }

        ssize_t line_size = readln(attr->devfd, line_buffer, sizeof(line_buffer));
        if (! mplayer_gpio_is_powered())
        {
        }

        if (0 == line_size)
            continue;
        LOG_debug("WT2005 <=: %s", line_buffer);

        if (NULL != task)
        {
            if (0 == strncmp("OK", line_buffer, 2))
            {
                task->err = 0;
                sema_post(&task->executed);
                msleep(50);

                if (MPLAYER_TASK_PLAY == task->hdr.msgid)
                    task->hdr.msgid = MPLAYER_TASK_PEEK_BUSY;
            }
            else if (0 == strncmp("ERR:", line_buffer, 2))
            {
                task->err = 10;
                sema_post(&task->executed);
                msleep(50);
            }
            else
                continue;

            if (MPLAYER_TASK_STOP == task->hdr.msgid && NULL != prev_task)
            {
                mqueue_release_pool(attr->mqd, (void *)prev_task);
                mqueue_flush(attr->mqd);

                prev_task = NULL;
            }

            mqueue_release_pool(attr->mqd, (void *)task);
            task = NULL;

            if (NULL != prev_task)
            {
                task = prev_task;
                prev_task = NULL;
            }
        }
    }
}

/****************************************************************************
 *  @implements: mplayer gpio weak
 ****************************************************************************/
__attribute__((weak))
bool mplayer_gpio_is_powered(void)
{
    LOG_error("mplayer_gpio_is_powered() is not implemented");
    return true;
}

__attribute__((weak))
void mplayer_gpio_power_on(void)
{
    LOG_error("mplayer_gpio_power_on() is not implemented");
}

__attribute__((weak))
void mplayer_gpio_power_off(void)
{
    LOG_error("mplayer_gpio_power_off() is not implemented");
}

__attribute__((weak))
void mplayer_gpio_mute(void)
{
    LOG_error("mplayer_gpio_mute() is not implemented");
}

__attribute__((weak))
void mplayer_gpio_unmute(void)
{
    LOG_error("mplayer_gpio_unmute() is not implemented");
}
