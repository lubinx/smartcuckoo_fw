#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <ultracore/timeo.h>
#include <ultracore/log.h>
#include <gpio.h>
#include <pmu.h>
#include "mplayer.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define SHELL_CMD_TIMEOUT               (500)

struct mplayer_attr_t
{
    pthread_mutex_t lock;
    timeout_t timeo_execute;
    int devfd;
    uint32_t pin_busy;

    bool mplayer_stopping;
    uint8_t volume;

    char line[512];
    char err[16];
};

/****************************************************************************
 *  @internal
 ****************************************************************************/
static struct mplayer_attr_t attr = {0};

static int SHELL_execute(char const *cmdline);
static int SHELL_post(char const *cmdline);
static void GPIO_busy_callback(uint32_t pins, void *arg);
static void TIMEO_execute_callback(void *arg);

/****************************************************************************
 *  @implements
 ****************************************************************************/
int mplayer_initlaize(int devfd, uint32_t pin_busy)
{
    mplayer_gpio_power_on();
    if (true)
    {
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

        pthread_mutex_init(&attr.lock, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
    }
    timeout_init(&attr.timeo_execute, 100, TIMEO_execute_callback, 0);

    attr.devfd = devfd;
    attr.pin_busy = pin_busy;

    fcntl(devfd, F_SETFL, fcntl(devfd, F_GETFL) | O_NONBLOCK);
    GPIO_intr_enable(pin_busy, TRIG_BY_RISING_EDGE, GPIO_busy_callback, NULL);

    while (0 != SHELL_execute("pwd")) {};
    return 0;
}

enum mplayer_stat_t mplayer_stat(void)
{
    if (0 != GPIO_peek(attr.pin_busy))
        return MPLAYER_PLAYING;
    else
        return MPLAYER_STOPPED;
}

void mplayer_idle_shutdown(uint32_t timeo)
{
    ARG_UNUSED(timeo);
}

int mplayer_play(char const *path)
{
    pthread_mutex_lock(&attr.lock);
    attr.mplayer_stopping = true;

    sprintf(attr.line, "mplayer play %s.mp3", path);
    int retval = SHELL_execute(attr.line);
    attr.mplayer_stopping = false;

    pthread_mutex_unlock(&attr.lock);
    return retval;
}

int mplayer_play_loop(char const *path)
{
    pthread_mutex_lock(&attr.lock);
    attr.mplayer_stopping = true;

    sprintf(attr.line, "mplayer play %s.mp3 -1", path);
    int retval = SHELL_execute(attr.line);
    attr.mplayer_stopping = false;

    pthread_mutex_unlock(&attr.lock);
    return retval;
}

bool mplayer_file_exists(char const *path)
{
    pthread_mutex_lock(&attr.lock);

    sprintf(attr.line, "ls %s.mp3", path);
    bool retval = 0 == SHELL_execute(attr.line);

    pthread_mutex_unlock(&attr.lock);
    return retval;
}

int mplayer_pause(void)
{
    return SHELL_execute("mplayer pause");
}

int mplayer_resume(void)
{
    return SHELL_execute("mplayer resume");
}

int mplayer_stop(void)
{
    pthread_mutex_lock(&attr.lock);
    attr.mplayer_stopping = true;

    int retval = SHELL_execute("mplayer stop");
    attr.mplayer_stopping = false;

    pthread_mutex_unlock(&attr.lock);

    pthread_yield();
    return retval;
}

int mplayer_playlist_queue(char const *path, uint32_t fade_in_intv)
{
    pthread_mutex_lock(&attr.lock);

    sprintf(attr.line, "mplayer queue %s.mp3 %u", path, (unsigned)fade_in_intv);
    int retval = SHELL_execute(attr.line);

    pthread_mutex_unlock(&attr.lock);
    return retval;
}

int mplayer_playlist_clear(void)
{
    return SHELL_execute("mplayer clear");
}

int mplayer_set_volume(uint8_t percent)
{
    pthread_mutex_lock(&attr.lock);

    sprintf(attr.line, "volume set %d", percent);
    int retval;

    if (0 == (retval = SHELL_execute(attr.line)))
        attr.volume = percent;

    pthread_mutex_unlock(&attr.lock);

    return retval;
}

uint8_t mplayer_volume(void)
{
    return attr.volume;
}

uint8_t mplayer_volume_inc(void)
{
    if (100 > attr.volume)
    {
        uint8_t vol = attr.volume + 5;
        if (100 < vol) vol = 100;

        mplayer_set_volume(vol);
    }
    return attr.volume;
}

uint8_t mplayer_volume_dec(void)
{
    if (5 < attr.volume)
    {
        uint8_t vol = attr.volume - 5;
        if (5 > vol) vol = 5;

        mplayer_set_volume(vol);
    }
    return attr.volume;
}

/****************************************************************************
 *  @internal
 ****************************************************************************/
ssize_t SHELL_readln(int fd, char *buf, size_t bufsize)
{
    int readed = 0;
    uint32_t tick = clock();

    while ((size_t)readed < bufsize)
    {
        char CH;

        if (0 > read(fd, &CH, sizeof(CH)))
        {
            if (EAGAIN == errno)
            {
                if (SHELL_CMD_TIMEOUT > clock() - tick)
                {
                    msleep(0);
                    continue;
                }
                else
                    return -1;
            }
            else
                return -1;
        }

        if ('\n' == CH)
        {
            if ( '\r' == *(buf - 1))
            {
                buf --;
                readed --;
            }

            *buf = '\0';
            return readed;
        }
        else
        {
            *buf = CH;

            buf ++;
            readed ++;
        }
    }

    return __set_errno_neg(EMSGSIZE);
}

static int SHELL_post(char const *cmdline)
{
    LOG_info("%s", cmdline);
    int retval = 0;

    pthread_mutex_lock(&attr.lock);
    while (-1 != read(attr.devfd, attr.err, sizeof(attr.err)));

    if (0 > writeln(attr.devfd, cmdline, strlen(cmdline)))
        retval = errno;

    pthread_mutex_unlock(&attr.lock);
    return retval;
}

static int SHELL_execute(char const *cmdline)
{
    pthread_mutex_lock(&attr.lock);
    int retval = SHELL_post(cmdline);

    if (0 == retval)
    {
        // waitfor empty line
        while (true)
        {
            ssize_t line_size = SHELL_readln(attr.devfd, attr.line, sizeof(attr.line));
            if (-1 == line_size)
            {
                // so..line is too long
                if (EMSGSIZE == (retval = errno))
                    continue;
                else
                    break;
            }
            else if (0 == line_size)
            {
                retval = 0;
                break;
            }
            else
                LOG_verbose("%s", attr.line);
        };
        // recevie #<errno>
        if (0 == retval)
        {
            while (true)
            {
                ssize_t line_size = SHELL_readln(attr.devfd, attr.err, sizeof(attr.err));

                if (-1 == line_size)
                    continue;
                else
                    LOG_verbose("%s", attr.err);

                if (2 > line_size || '#' != attr.err[0])
                {
                    retval = ECMD;
                    break;
                }

                retval = (int)strtoul((char *)&attr.err[1], NULL, 10);
                break;
            };
        }
    }
    pthread_mutex_unlock(&attr.lock);
    return retval;
}

static void TIMEO_execute_callback(void *arg)
{
    ARG_UNUSED(arg);
    mplayer_idle_callback();
}

static void GPIO_busy_callback(uint32_t pins, void *arg)
{
    ARG_UNUSED(pins);

    if (! attr.mplayer_stopping)
        timeout_start(&attr.timeo_execute, arg);
}
