#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include <glist.h>
#include <wdt.h>
#include <ultracore/log.h>
#include <ultracore/sema.h>
#include <ultracore/mq.h>

#include "mplayer.h"
#include "wt2003h.h"

/***************************************************************************
 * @def
 ***************************************************************************/
#define MPLAYER_WAIT_FOREVER            ((uint32_t)-1)
#define DEF_IDLE_SHUTDOWN_TIMEO         (250)
#define TASK_QUEUE_SIZE                 (16)

enum mtask_id_t
{
    /**
     *  power on / off
     *      chip implemented payload
    */
    MPLAYER_TASK_POWER_ON           = 1,
    MPLAYER_TASK_POWER_OFF,

    /**
     *  play / stop
     *      chip implemented payload
     *
     *  marshalling arguments:
     *      int folder
     *      int name
    */
    MPLAYER_TASK_PLAY,
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

    MPLAYER_SELF_TASK_BASE          = 0x80,
    /**
     *  playlist
    */
    MPLAYER_TASK_PLAYLIST_QUEUE,
    MPLAYER_TASK_PLAYLIST_CLEAR,

    /**
     *  fade in / out
     *
     *  unmarshalling arguments:
     *      as_u32[0]   milli-second
    */
    MPLAYER_TASK_FADE,
};

#define MPLAYER_IMMEDIATELY_PRIO        (0)
#define MPLAYER_IDLE_PRIO               ((uint8_t)-1)
#define MPLAYER_POWER_OFF_PRIO          (MPLAYER_IDLE_PRIO - 1)

#define MPLAYER_HIGH_PRIO               (1)
#define MPLAYER_NORMAL_PRIO             (5)

#define MPLAYER_PAYLOAD_SIZE            (32)

struct mtask_t
{
    struct MQ_message_hdr_t hdr;

    int err;
    clock_t tick;

    union
    {
        uint8_t     as_u8[MPLAYER_PAYLOAD_SIZE / sizeof(uint8_t)];
        uint16_t    as_u16[MPLAYER_PAYLOAD_SIZE / sizeof(uint16_t)];
        uint32_t    as_u32[MPLAYER_PAYLOAD_SIZE / sizeof(uint32_t)];
        uint64_t    as_u64[MPLAYER_PAYLOAD_SIZE / sizeof(uint64_t)];

        int8_t      as_i8[MPLAYER_PAYLOAD_SIZE / sizeof(int8_t)];
        int16_t     as_i16[MPLAYER_PAYLOAD_SIZE / sizeof(int16_t)];
        int32_t     as_i32[MPLAYER_PAYLOAD_SIZE / sizeof(int32_t)];
        int64_t     as_i64[MPLAYER_PAYLOAD_SIZE / sizeof(int64_t)];

        float       as_float[MPLAYER_PAYLOAD_SIZE / sizeof(float)];
        double      as_double[MPLAYER_PAYLOAD_SIZE / sizeof(double)];

        void *      as_ptr[MPLAYER_PAYLOAD_SIZE / sizeof(void *)];
    } __attribute__((packed)) payload;
};

/***************************************************************************
 * @private
 ***************************************************************************/
struct mplayer_attr_t
{
    int devfd;
    int mqd;

    enum mplayer_stat_t stat;
    uint32_t avg_latency;

    uint8_t vol_max;
    uint8_t vol_min;
    uint8_t vol_curr;

    bool idle_shuttingdown;
    uint32_t idle_shutdown_timeo;
    uint32_t idle_start_tick;

    struct mtask_t *task_curr;
};

static __attribute__((noreturn)) void *mplayer_controller_thread(struct mplayer_attr_t *ctx);
static char const *mplayer_task_id_str(enum mtask_id_t taskid);

static int mplayer_marshalling_send(struct mplayer_attr_t *attr, struct mtask_t **task,
    enum mtask_id_t taskid, ...);

// var
static struct mplayer_attr_t mplayer_attr = {0};
static uint32_t mplayer_controller_stack[768 / sizeof(uint32_t)];

/*************************************************************************
 * @implements
**************************************************************************/
int mplayer_initlaize(int devfd, uint32_t pin_busy)
{
    mplayer_gpio_power_on();
    WT2003H_init(devfd, pin_busy);

    mplayer_attr.devfd = devfd;
    mplayer_attr.idle_shutdown_timeo = DEF_IDLE_SHUTDOWN_TIMEO;
    mplayer_attr.mqd = mqueue_create(sizeof(struct mtask_t) - sizeof(struct MQ_message_hdr_t), TASK_QUEUE_SIZE);

    mplayer_attr.vol_max = WT2003H_get_max_volume(&mplayer_attr.vol_curr);
    mplayer_attr.vol_min = WT2003H_get_min_volume();

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, mplayer_controller_stack, sizeof(mplayer_controller_stack));

    pthread_t id;
    pthread_create(&id, &attr, (void *)mplayer_controller_thread, &mplayer_attr);
    pthread_attr_destroy(&attr);

    mplayer_gpio_power_off();
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

void mplayer_verbose_log_hex(char const *hint, void const *buf, size_t bufsize)
{
#ifdef DEBUG
    static char const __xlat[16] = "0123456789ABCDEF";

    if (LOG_DEBUG < __log_level)
    {
        printf(LOG_VERBOSE_COLOR LOG_VERBOSE_LETTER " %-10" PRIu32 "%s:", LOG_timestamp(), hint);

        for (size_t i = 0; i < bufsize; i ++, buf ++)
            printf(" %c%c", __xlat[(*(uint8_t *)buf) >> 4], __xlat[(*(uint8_t *)buf) & 0xF]);

        printf(LOG_END "\n");
    }
#else
    ARG_UNUSED(hint, buf, bufsize);
#endif
}

int mplayer_play(char const *path)
{
    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_PLAY, path);
}

int mplayer_play_loop(char const *path)
    __attribute__((alias("mplayer_play")));

bool mplayer_file_exists(char const *path)
{
    struct mtask_t *task;

    if (0 == mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_PEEK_FILE, path))
    {
        while (mplayer_gpio_is_powered() && task->err == EAGAIN)
            msleep(10);
    }
    return EEXIST == task->err;
}

int mplayer_pause(void)
{
    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_PAUSE);
}

int mplayer_resume(void)
{
    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_RESUME);
}

int mplayer_stop(void)
{
    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_STOP);
}

int mplayer_set_volume(uint8_t percent)
{
    uint8_t range = mplayer_attr.vol_max - mplayer_attr.vol_min;

    if (20 >= percent)
        mplayer_attr.vol_curr = mplayer_attr.vol_min;
    else
        mplayer_attr.vol_curr = mplayer_attr.vol_min + (uint8_t)(range * (percent - 20) / 80);

    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_SET_VOLUME);
}

uint8_t mplayer_volume(void)
{
    uint8_t range = mplayer_attr.vol_max - mplayer_attr.vol_min;
    return 20 + (uint8_t)(((mplayer_attr.vol_curr - mplayer_attr.vol_min) * 80 + range / 2) / range);
}

uint8_t mplayer_volume_inc(void)
{
    if (mplayer_attr.vol_curr < mplayer_attr.vol_max)
    {
        mplayer_attr.vol_curr ++;

        struct mtask_t *task;
        mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_SET_VOLUME);
    }

    return mplayer_volume();
}

uint8_t mplayer_volume_dec(void)
{
    if (mplayer_attr.vol_curr > mplayer_attr.vol_min)
    {
        mplayer_attr.vol_curr --;

        struct mtask_t *task;
        mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_SET_VOLUME);
    }
    return mplayer_volume();
}

int mplayer_playlist_queue(char const *path, uint32_t fade_in_intv)
{
    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_PLAYLIST_QUEUE,
        path, fade_in_intv);
}

int mplayer_playlist_clear(void)
{
    struct mtask_t *task;
    return mplayer_marshalling_send(&mplayer_attr, &task, MPLAYER_TASK_PLAYLIST_CLEAR);
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
static struct mtask_t *mqueue_get_task_buf(int mqd)
{
    struct mtask_t *task = (void *)mqueue_get_pool(mqd, 0);
    if (task)
    {
        task->tick = 0;
        task->err = 0;
    }
    return task;
}

static uint16_t mqueued_task_id(int mqd)
{
    uint16_t msgid;
    uint8_t prio;
    if (0 == mqueue_peek_attr(mqd, &msgid, &prio))
        return msgid;
    else
        return MPLAYER_TASK_POWER_OFF;
}

static uint16_t mqueued_last_task_id(int mqd)
{
    uint16_t msgid;
    uint8_t prio;
    if (0 == mqueue_peek_last_attr(mqd, &msgid, &prio))
        return msgid;
    else
        return MPLAYER_TASK_POWER_OFF;
}

static uint8_t mqueued_prio(int mqd)
{
    uint16_t msgid;
    uint8_t prio;
    if (0 == mqueue_peek_attr(mqd, &msgid, &prio))
        return prio;
    else
        return MPLAYER_IDLE_PRIO;
}

static void *mplayer_controller_thread(struct mplayer_attr_t *attr)
{
    struct mtask_t *task;
    clock_t tick = 0;
    struct mtask_t *prev_task = NULL;

    while (true)
    {
        WDOG_feed();
        task = attr->task_curr;

        if (NULL == task)
        {
            if (MPLAYER_IDLE_PRIO == mqueued_prio(attr->mqd))
            {
                if (attr->idle_shuttingdown)
                {
                    if (attr->idle_shutdown_timeo <= mplayer_get_tickcount() - attr->idle_start_tick)
                    {
                power_off:
                        if (mplayer_gpio_is_powered())
                        {
                            mplayer_gpio_power_off();
                            mplayer_power_callback(false);
                            LOG_info("mplayer_gpio_power_off()");
                        }

                        // forever waitfor a task
                        goto recv_next_task;
                    }
                    else
                        msleep(50);
                }
                else
                {
                    attr->stat = MPLAYER_STOPPED;
                    mplayer_idle_callback();

                    if (MPLAYER_IDLE_PRIO == mqueued_prio(attr->mqd))
                    {
                        attr->idle_shuttingdown = true;
                        attr->idle_start_tick = mplayer_get_tickcount();
                    }
                }

                continue;
            }
            else
            {
            recv_next_task:
                attr->task_curr = task = (void *)mqueue_recv(attr->mqd);
                attr->idle_shuttingdown = false;
                bool abort = false;

                switch (task->hdr.msgid)
                {
                default:
                    break;

                case MPLAYER_TASK_PLAY:
                    mplayer_gpio_unmute();
                    break;

                case MPLAYER_TASK_RESUME:
                    abort = (NULL == prev_task);
                    if (! abort)
                        mplayer_gpio_unmute();
                    break;

                case MPLAYER_TASK_PAUSE:
                    abort = (NULL == prev_task);
                    if (! abort)
                        mplayer_gpio_unmute();
                    break;
                };

                if (abort)
                {
                    mqueue_release_pool(attr->mqd, (void *)task);
                    attr->task_curr = NULL;
                    continue;
                }

                task->err = EAGAIN;
                tick = mplayer_get_tickcount();

                if (MPLAYER_TASK_FADE == task->hdr.msgid)
                {
                    LOG_debug("mplayer: %s %" PRIu32 " ms", mplayer_task_id_str(task->hdr.msgid),
                        task->payload.as_u32[0]);
                }
                else
                    LOG_debug("mplayer: %s", mplayer_task_id_str(task->hdr.msgid));
            }
        }
        else
        {
            if (0 != task->tick && 1500 < clock() - task->tick)
            {
                LOG_debug("mplayer timedout: %s", mplayer_task_id_str(task->hdr.msgid));

                mqueue_flush(attr->mqd);
                if (attr->task_curr)
                {
                    mqueue_release_pool(attr->mqd, (void *)attr->task_curr);
                    attr->task_curr = NULL;
                }
                if (prev_task)
                {
                    mqueue_release_pool(attr->mqd, (void *)prev_task);
                    prev_task = NULL;
                }
                goto power_off;
            }

            if (NULL == prev_task)
            {
                uint8_t queued_prio = mqueued_prio(attr->mqd);
                if (queued_prio < task->hdr.prio)
                {
                    prev_task = task;
                    goto recv_next_task;
                }
            }
            msleep(10);
        }

        if (MPLAYER_SELF_TASK_BASE > task->hdr.msgid)
        {
            if (0 == task->tick)
            {
                task->tick = clock();

                switch (task->hdr.msgid)
                {
                default:
                    break;

                case MPLAYER_TASK_PLAY:
                    mplayer_gpio_unmute();
                    break;

                case MPLAYER_TASK_PAUSE:
                case MPLAYER_TASK_RESUME:
                case MPLAYER_TASK_STOP:
                    if (NULL == prev_task)
                        task->err = 0;
                    break;
                };

                if (0 != task->err)
                {
                    if (mplayer_gpio_is_powered())
                        WT2003H_send_payload(attr->devfd, &task->payload, task->hdr.payload_size);
                    else
                        task->err = 0;
                }
            }

            if (0 != task->err)
                task->err = WT2003H_recv_ack(attr->devfd, &task->payload);
        }
        else
        {
            if (MPLAYER_TASK_FADE == task->hdr.msgid)
            {
                if (task->payload.as_u32[0] <= mplayer_get_tickcount() - tick)
                    task->err = 0;
                else
                    msleep(10);
            }
        }

        if (! mplayer_gpio_is_powered())
        {
            task->err = 0;

            mqueue_flush(attr->mqd);
            LOG_warning("mplayer: clear tasks due to power is down");
        }
        else if (0 == task->err)
        {
            switch (task->hdr.msgid)
            {
            default:
                break;

            case MPLAYER_TASK_POWER_ON:
                mplayer_power_callback(true);
                break;

            case MPLAYER_TASK_PLAY:
                mplayer_gpio_mute();
                break;

            case MPLAYER_TASK_PAUSE:
                attr->stat = MPLAYER_PAUSED;
                break;

            case MPLAYER_TASK_RESUME:
                attr->stat = MPLAYER_PLAYING;
                break;
            };


            if (MPLAYER_TASK_FADE != task->hdr.msgid)
            {
                LOG_debug("mplayer done: %s %u ms", mplayer_task_id_str(task->hdr.msgid),
                    mplayer_get_tickcount() - tick);
            }
        }
        else
        {
            if (EAGAIN == task->err)
            {
                if (MPLAYER_PLAYING == attr->stat && MPLAYER_TASK_PLAY == task->hdr.msgid)
                {
                    // cheat playing no timeout
                    task->tick = clock();
                }
                continue;
            }

            if (EEXIST == task->err)
            {
                if (0 == attr->avg_latency)
                    attr->avg_latency = mplayer_get_tickcount() - tick;
                else
                    attr->avg_latency = (attr->avg_latency + mplayer_get_tickcount() - tick) / 2;

                LOG_info("mplayer: average latency: %" PRIu32, attr->avg_latency);
            }

            if (MPLAYER_TASK_PEEK_FILE == task->hdr.msgid)
            {
                msleep(50);
                if (EEXIST == task->err)
                {
                    task->hdr.payload_size = WT2003H_fill_stop_payload(&task->payload);
                    while (true)
                    {
                        WT2003H_send_payload(attr->devfd, &task->payload, task->hdr.payload_size);
                        if (0 == WT2003H_recv_ack(attr->devfd, &task->payload))
                            break;
                        else
                            msleep(100);
                    }
                }

                attr->stat = MPLAYER_STOPPED;
                goto end_task;
            }
            else if (MPLAYER_TASK_PLAY == task->hdr.msgid)
            {
                // EEXIST => EAGAIN to peek busy stat
                if (EEXIST == task->err)
                {
                    msleep(attr->avg_latency);

                    attr->stat = MPLAYER_PLAYING;
                    task->err = EAGAIN;
                }
                else
                {
                    while (MPLAYER_TASK_FADE == mqueued_task_id(attr->mqd))
                    {
                        struct mtask_t *remove_fade = (void *)mqueue_recv(attr->mqd);
                        mqueue_release_pool(attr->mqd, (void *)remove_fade);
                    }
                    attr->stat = MPLAYER_STOPPED;
                }
            }
            else
            {
                LOG_error("mplayer err: %s", strerror(task->err));
                task->err = 0;
            }
        }

        if (0 == task->err)
        {
        end_task:
            if (MPLAYER_TASK_STOP == task->hdr.msgid)
            {
                if (NULL != prev_task)
                {
                    mqueue_release_pool(attr->mqd, (void *)prev_task);
                    prev_task = NULL;
                }
                attr->stat = MPLAYER_STOPPED;
            }

            mqueue_release_pool(attr->mqd, (void *)task);

            if (NULL != prev_task)
            {
                attr->task_curr = prev_task;
                prev_task = NULL;
            }
            else
                attr->task_curr = NULL;
        }
    }
}

static int mplayer_marshalling_send(struct mplayer_attr_t *attr, struct mtask_t **task,
    enum mtask_id_t taskid, ...)
{
    if (0 >= mplayer_attr.devfd)
        return ENODEV;

    if (MPLAYER_TASK_PLAYLIST_CLEAR == taskid)
    {
        mqueue_flush(mplayer_attr.mqd);
        return 0;
    }

    if (NULL == mplayer_attr.task_curr && MPLAYER_TASK_STOP != taskid)
    {
        if (! mplayer_gpio_is_powered())
        {
            mqueue_flush(mplayer_attr.mqd);

            // push power on task
            struct mtask_t *power_task = mqueue_get_task_buf(mplayer_attr.mqd);
            if (power_task)
            {
                power_task->hdr.prio = MPLAYER_HIGH_PRIO;
                power_task->hdr.msgid = MPLAYER_TASK_POWER_ON;
                power_task->hdr.payload_size = WT2003H_fill_power_on_payload(&power_task->payload);

                mplayer_gpio_power_on();
                mqueue_post(mplayer_attr.mqd, (void *)power_task);
            }
        }
    }

    *task = mqueue_get_task_buf(mplayer_attr.mqd);
    if (NULL != *task)
    {
        va_list vl;
        va_start(vl, taskid);

        switch (taskid)
        {
        case MPLAYER_TASK_POWER_ON:
        case MPLAYER_TASK_POWER_OFF:
        case MPLAYER_SELF_TASK_BASE:
        case MPLAYER_TASK_PLAYLIST_CLEAR:
            break;

        case MPLAYER_TASK_PLAY:
        case MPLAYER_TASK_PLAYLIST_QUEUE:
        case MPLAYER_TASK_PEEK_FILE:
            attr->stat = MPLAYER_SEEKING;

            (*task)->hdr.prio = MPLAYER_NORMAL_PRIO;
            (*task)->hdr.payload_size = WT2003H_fill_play_payload(&(*task)->payload, va_arg(vl, char *));

            if (MPLAYER_TASK_PLAYLIST_QUEUE == taskid)
            {
                taskid = MPLAYER_TASK_PLAY;

                if (MPLAYER_TASK_PLAY == mqueued_last_task_id(mplayer_attr.mqd) ||
                    MPLAYER_TASK_PLAY == attr->task_curr->hdr.msgid)
                {
                    uint32_t fade_in = va_arg(vl, uint32_t);
                    if (0 != fade_in && attr->avg_latency < fade_in)
                    {
                        fade_in = fade_in - attr->avg_latency;

                        struct mtask_t *fade = mqueue_get_task_buf(mplayer_attr.mqd);
                        if (fade)
                        {
                            fade->hdr.msgid = MPLAYER_TASK_FADE;
                            fade->hdr.prio = MPLAYER_NORMAL_PRIO;
                            fade->hdr.payload_size = sizeof(uint32_t);
                            fade->payload.as_u32[0] = fade_in;

                            mqueue_post(mplayer_attr.mqd,(void *)fade);
                        }
                    }
                }
            }
            break;

        case MPLAYER_TASK_PAUSE:
            (*task)->hdr.prio = MPLAYER_HIGH_PRIO;
            (*task)->hdr.payload_size = WT2003H_fill_pause_payload(&(*task)->payload);
            break;

        case MPLAYER_TASK_RESUME:
            (*task)->hdr.prio = MPLAYER_HIGH_PRIO;
            (*task)->hdr.payload_size = WT2003H_fill_resume_payload(&(*task)->payload);
            break;

        case MPLAYER_TASK_STOP:
            mplayer_stopping_callback();
            mplayer_gpio_mute();

            if (NULL == mplayer_attr.task_curr ||
                MPLAYER_TASK_STOP == mplayer_attr.task_curr->hdr.msgid ||
                MPLAYER_HIGH_PRIO == mqueued_prio(mplayer_attr.mqd))
            {
                mqueue_release_pool(mplayer_attr.mqd, (void *)*task);
                *task = NULL;
            }
            else
            {
                (*task)->hdr.prio = MPLAYER_HIGH_PRIO;
                (*task)->hdr.payload_size = WT2003H_fill_stop_payload(&(*task)->payload);
            }
            mqueue_flush(mplayer_attr.mqd);
            break;

        case MPLAYER_TASK_FADE:
            (*task)->hdr.prio = MPLAYER_NORMAL_PRIO;
            if (true)
            {
                uint32_t fade_timeout = va_arg(vl, uint32_t);
                (*task)->hdr.prio = MPLAYER_NORMAL_PRIO;
                (*task)->hdr.payload_size = sizeof(uint32_t);
                (*task)->payload.as_u32[0] = fade_timeout;
            }
            break;

        case MPLAYER_TASK_SET_VOLUME:
            (*task)->hdr.prio = MPLAYER_HIGH_PRIO;
            (*task)->hdr.payload_size = WT2003H_fill_volume_payload(&(*task)->payload, mplayer_attr.vol_curr);
            break;
        };
        va_end(vl);

        if (NULL != *task)
        {
            if (mplayer_gpio_is_powered())
            {
                (*task)->hdr.msgid = taskid;
                (*task)->err = EAGAIN;

                mqueue_post(mplayer_attr.mqd, (void *)*task);
            }
            else
                mqueue_release_pool(mplayer_attr.mqd, (void *)*task);
        }
        return 0;
    }
    else
        return EAGAIN;
}

static char const *mplayer_task_id_str(enum mtask_id_t taskid)
{
    switch (taskid)
    {
    case MPLAYER_TASK_POWER_ON:
        return "mplayer_power_on()";
    case MPLAYER_TASK_POWER_OFF:
        return "mplayer_power_off()";

    case MPLAYER_TASK_PLAY:
        return "mplayer_play()";
    case MPLAYER_TASK_STOP:
        return "mplayer_stop()";
    case MPLAYER_TASK_PEEK_FILE:
        return "mplayer_file_exists()";

    case MPLAYER_TASK_PAUSE:
        return "mplayer_pause()";
    case MPLAYER_TASK_RESUME:
        return "mplayer_resume()";

    case MPLAYER_TASK_SET_VOLUME:
        return "mplayer_set_volume()";

    case MPLAYER_TASK_FADE:
        return "mplayer_fade()";
    case MPLAYER_TASK_PLAYLIST_QUEUE:
        return "mplayer_playerlist_queue()";
    case MPLAYER_TASK_PLAYLIST_CLEAR:
        return "mplayer_playerlist_clear()";

    case MPLAYER_SELF_TASK_BASE:
        break;
    }

    return "error_task_id";
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

/****************************************************************************
 *  @implements: chip implement weak
 ****************************************************************************/
int WT2003H_send_payload(int devfd, void *payload, size_t payload_size)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;
    mplayer_verbose_log_hex("send", payload, 2 + packet->len);

    if (-1 != write(devfd, payload, payload_size))
        return 0;
    else
        return EIO;
}

uint8_t WT2003H_fill_packet(struct wt2003h_packet_t *packet, uint8_t data_size)
{
    packet->tag = WT2003H_START_TAG;
    packet->len = data_size + 3;

    packet->data[data_size + 0] = WT2003H_checksum(packet);
    packet->data[data_size + 1] = WT2003H_END_TAG;

    return /*START/END*/(2) + packet->len;
}

uint8_t WT2003H_checksum(struct wt2003h_packet_t *packet)
{
    uint8_t cs = 0;
    uint8_t *ptr = &packet->len;

    for (int i = 0; i < packet->len - 1; i ++)
    {
        cs += *ptr;
        ptr ++;
    }
    return cs;
}

int WT2003H_recv_payload(int devfd, void *payload)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;
    int retval = 0;

    while (1)
    {
        uint8_t ch = 0;

        if (sizeof(ch) != read(devfd, &ch, sizeof(ch)))
        {
            retval = errno;
            break;
        }
        // BUG: powering-up request any thing will reponse '\0'
        if (0 == ch)
            continue;
        if (WT2003H_START_TAG == ch)
            break;
    }

    if (0 == retval)
    {
        packet->tag = WT2003H_START_TAG;
        if (2 != readbuf(devfd, &packet->len, 2))
            retval = errno;
    }
    if (0 == retval)
    {
        uint8_t data_size = packet->len - 1;        // include CS / END

        if (data_size != readbuf(devfd, &packet->data, data_size))
            retval = errno;
        else
            mplayer_verbose_log_hex("recv", packet, 2 + packet->len);
    }
    return retval;
}

__attribute__((weak))
void WT2003H_init(int devfd, uint32_t pin_busy)
{
    (void)devfd;
    (void)pin_busy;
}

__attribute__((weak))
uint8_t WT2003H_get_max_volume(uint8_t *def_vol)
{
    if (def_vol)
        *def_vol = 100;
    return 100;
}

__attribute__((weak))
uint8_t WT2003H_get_min_volume(void)
{
    return 0;
}

__attribute__((weak))
int WT2003H_recv_ack(int devfd, void *requesting_payload)
{
    (void)devfd, (void)requesting_payload;
    return ENOSYS;
}

__attribute__((weak))
uint8_t WT2003H_fill_power_on_payload(void *payload)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;
    packet->cmd = WT2003H_CMD_POWER_UP;
    return WT2003H_fill_packet(packet, 0);
}

__attribute__((weak))
uint8_t WT2003H_fill_play_payload(void *payload, char const *path)
{
    (void)payload, (void)path;
    LOG_error("WT2003H_fill_play_payload() is not implemented");
    return 0;
}

__attribute__((weak))
uint8_t WT2003H_fill_stop_payload(void *payload)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;

    packet->cmd = WT2003H_CMD_STOP;
    return WT2003H_fill_packet(packet, 0);
}

__attribute__((weak))
uint8_t WT2003H_fill_pause_payload(void *payload)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;

    packet->cmd = WT2003H_CMD_PAUSE;
    return WT2003H_fill_packet(packet, 0);
}

__attribute__((weak))
uint8_t WT2003H_fill_resume_payload(void *payload)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;

    packet->cmd = WT2003H_CMD_PAUSE;
    return WT2003H_fill_packet(packet, 0);
}

__attribute__((weak))
uint8_t WT2003H_fill_volume_payload(void *payload, uint8_t volume)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;

    packet->cmd = WT2003H_CMD_SET_VOLUME;
    packet->data[0] = volume;
    return WT2003H_fill_packet(packet, 1);
}
