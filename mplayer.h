#ifndef __MPLAYER_H
#define __MPLAYER_H                     1

#include <features.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

    enum mplayer_stat_t
    {
        MPLAYER_STOPPED         = 0,
        MPLAYER_PAUSED,
        MPLAYER_SEEKING,
        MPLAYER_PLAYING,
    };

__BEGIN_DECLS

extern __attribute__((nothrow))
    int mplayer_initlaize(int devfd, uint32_t pin_busy);

extern __attribute__((nothrow, pure))
    enum mplayer_stat_t mplayer_stat(void);

    /**
     *  cut power when after idle timeout
    */
extern __attribute__((nothrow))
    void mplayer_idle_shutdown(uint32_t timeo);

    // utils, get tickcount in milli-second
extern __attribute__((nothrow))
    clock_t mplayer_get_tickcount(void);

    // utils, log verbose hex
extern __attribute__((nothrow))
    void mplayer_verbose_log_hex(char const *hint, void const *buf, size_t bufsize);

    /**
     *  play / peek file
    */
extern __attribute__((nothrow))
    int mplayer_play(char const *path);
extern __attribute__((nothrow))
    int mplayer_play_loop(char const *path);
extern __attribute__((nothrow))
    bool mplayer_file_exists(char const *path);

    /**
     *  pause / resume / stop
    */
extern __attribute__((nothrow))
    int mplayer_pause(void);
extern __attribute__((nothrow))
    int mplayer_resume(void);
extern __attribute__((nothrow))
    int mplayer_stop(void);

    /** volume set / + / - */
extern __attribute__((nothrow))
    int mplayer_set_volume(uint8_t percent);
extern __attribute__((nothrow))
    uint8_t mplayer_volume(void);
extern __attribute__((nothrow))
    uint8_t mplayer_volume_inc(void);
extern __attribute__((nothrow))
    uint8_t mplayer_volume_dec(void);

    /**
     *  player list add queue / clear queue
    */
extern __attribute__((nothrow))
    int mplayer_playlist_queue(char const *path, uint32_t fade_in_intv);
extern __attribute__((nothrow))
    int mplayer_playlist_clear(void);

/****************************************************************************
 *  @def: mplayer command & callback
 ****************************************************************************/
extern __attribute__((nothrow))
    int mplayer_command_post(char const *cmdline);
extern __attribute__((nothrow))
    int mplayer_sendbuf(void *buf, size_t bufsize);
extern __attribute__((nothrow))
    int mplayer_recvbuf(void *buf, size_t bufsize);

    typedef int (*SH_callback_t)(char const *line, void *arg);
extern __attribute__((nothrow))
    int mplayer_commnad_cb(char const *cmdline, SH_callback_t line_cb, void *arg);

/****************************************************************************
 *  @def: overridable
 ****************************************************************************/
extern __attribute__((nothrow))
    bool mplayer_gpio_is_powered(void);
extern __attribute__((nothrow))
    void mplayer_gpio_power_on(void);
extern __attribute__((nothrow))
    void mplayer_gpio_power_off(void);

extern __attribute__((nothrow))
    void mplayer_gpio_mute(void);
extern __attribute__((nothrow))
    void mplayer_gpio_unmute(void);

extern __attribute__((nothrow))
    void mplayer_power_callback(bool stat);
extern __attribute__((nothrow))
    void mplayer_stopping_callback(void);
extern __attribute__((nothrow))
    void mplayer_idle_callback(void);

__END_DECLS
#endif
