#ifndef __MPLAYER_VOICE_H
#define __MPLAYER_VOICE_H               1

#include <features.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "locale.h"

    enum VOICE_setting_t
    {
        VOICE_SETTING_LANG,
        VOICE_SETTING_VOICE,

        VOICE_SETTING_HOUR,
        VOICE_SETTING_MINUTE,
        VOICE_SETTING_YEAR,
        VOICE_SETTING_MONTH,
        VOICE_SETTING_MDAY,

        VOICE_SETTING_ALARM_HOUR,
        VOICE_SETTING_ALARM_MIN,
        VOICE_SETTING_ALARM_RINGTONE,

        VOICE_SETTING_COUNT,
        VOICE_SETTING_DONE          = VOICE_SETTING_COUNT,

        VOICE_SETTING_EXT_LOW_BATT,
        VOICE_SETTING_EXT_ALARM_ON,
        VOICE_SETTING_EXT_ALARM_OFF,
    };

    typedef void (* VOICE_avail_locales_callback_t)(int id, char const *lcid,
        enum LOCALE_dfmt_t dfmt, enum LOCALE_hfmt_t hfmt,  char const *voice, void *arg, bool final);

__BEGIN_DECLS

extern __attribute__((nothrow))
    int16_t VOICE_init(int16_t voice_id, struct LOCALE_t const *locale);

extern __attribute__((nothrow))
    void VOICE_enum_avail_locales(VOICE_avail_locales_callback_t callback, void *arg);

extern __attribute__((nothrow, pure))
    enum LOCALE_dfmt_t VOICE_get_default_dfmt(void);
extern __attribute__((nothrow, pure))
    enum LOCALE_hfmt_t VOICE_get_default_hfmt(void);

    /**
     *  VOICE_first_setting() / VOICE_next_setting() / VOICE_prev_setting()
    */
extern __attribute__((nothrow))
    enum VOICE_setting_t VOICE_first_setting(void);
extern __attribute__((nothrow))
    enum VOICE_setting_t VOICE_prev_setting(enum VOICE_setting_t setting);
extern __attribute__((nothrow))
    enum VOICE_setting_t VOICE_next_setting(enum VOICE_setting_t setting);

    /**
     *  VOICE_select_voice()
     *      direct select language & voice by id
     *
     *  VOICE_select_lcid()
     *      select language ISO639
     *
     *
     *  VOICE_next_voice()
     *      select language next voice
     *
     *  @returns
     *      voice_id
    */
extern __attribute__((nothrow))
    int16_t VOICE_select_voice(int16_t voice_id);
extern __attribute__((nothrow))
    int16_t VOICE_select_lcid(char const *lcid);

    /**
     *  VOICE_prev_locale() / VOICE_next_locale()
    */
extern __attribute__((nothrow))
    int16_t VOICE_prev_locale(void);
extern __attribute__((nothrow))
    int16_t VOICE_next_locale(void);


    /**
     *  VOICE_prev_voice() / VOICE_next_voice()
    */
extern __attribute__((nothrow))
    int16_t VOICE_prev_voice(void);
extern __attribute__((nothrow))
    int16_t VOICE_next_voice(void);

    /**
     *  say date using struct tm / unix epoch
    */
extern __attribute__((nothrow))
    int VOICE_say_date(struct tm const *tm);
extern __attribute__((nothrow))
    int VOICE_say_date_epoch(time_t epoch);

    /**
     *  say time using struct tm / unix epoch
    */
extern __attribute__((nothrow))
    int VOICE_say_time(struct tm const *tm);
extern __attribute__((nothrow))
    int VOICE_say_time_epoch(time_t epoch);

    /**
     *  say aux voice
    */
extern __attribute__((nothrow))
    int VOICE_say_setting(enum VOICE_setting_t setting);
extern __attribute__((nothrow))
    int VOICE_say_setting_part(enum VOICE_setting_t setting, struct tm const *tm, int ringtone_id);

    /**
     *  get next ring tone index & play ring tone at index
    */
extern __attribute__((nothrow))
    int VOICE_select_ringtone(int ringtone_id);
extern __attribute__((nothrow, pure))
    int VOICE_next_ringtone(int ringtone_id);
extern __attribute__((nothrow, pure))
    int VOICE_prev_ringtone(int ringtone_id);

extern __attribute__((nothrow))
    int VOICE_play_ringtone(int ringtone_id);

    /**
     *  play reminder
    */
extern __attribute__((nothrow))
    int VOICE_play_reminder(int reminder_id);

__END_DECLS
#endif
