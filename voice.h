#ifndef __MPLAYER_VOICE_H
#define __MPLAYER_VOICE_H               1

#include <features.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "locale.h"

    struct VOICE_attr_t
    {
        struct VOICE_t const *voice;
        uint8_t voice_count;

        struct LOCALE_t *locale;
    };

    enum VOICE_setting_part_t
    {
        VOICE_SETTING_LANG,
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

__BEGIN_DECLS

extern __attribute__((nothrow))
    void VOICE_init(struct VOICE_attr_t *attr, struct LOCALE_t *locale);
extern __attribute__((nothrow))
    int16_t VOICE_init_locales(struct VOICE_attr_t *attr, int16_t voice_id, bool enum_only_exists);

    typedef void (* VOICE_avail_locales_callback_t)(int id, char const *lcid, char const *voice,
        void *arg, bool final);
extern __attribute__((nothrow))
    void VOICE_enum_avail_locales(VOICE_avail_locales_callback_t callback, void *arg);

extern __attribute__((nothrow, const))
    enum LOCALE_dfmt_t VOICE_get_default_dfmt(struct VOICE_attr_t *attr);
extern __attribute__((nothrow, const))
    enum LOCALE_hfmt_t VOICE_get_default_hfmt(struct VOICE_attr_t *attr);

    /**
     *  VOICE_select_voice()
     *      direct select language & voice by id
     *  VOICE_select_lcid()
     *      select language ISO639
     *  VOICE_next_locale()
     *      select next language
     *  VOICE_next_voice()
     *      select language next voice
     *  @returns
     *      voice_id
    */
extern __attribute__((nothrow))
    int16_t VOICE_select_voice(struct VOICE_attr_t *attr, int16_t voice_id);
extern __attribute__((nothrow))
    int16_t VOICE_select_lcid(struct VOICE_attr_t *attr, char const *lcid);
extern __attribute__((nothrow))
    int16_t VOICE_next_locale(struct VOICE_attr_t *attr);

    /**
     *  say date using struct tm / unix epoch
    */
extern __attribute__((nothrow))
    int VOICE_say_date(struct VOICE_attr_t *attr, struct tm const *tm);
extern __attribute__((nothrow))
    int VOICE_say_date_epoch(struct VOICE_attr_t *attr, time_t epoch);

    /**
     *  say time using struct tm / unix epoch
    */
extern __attribute__((nothrow))
    int VOICE_say_time(struct VOICE_attr_t *attr, struct tm const *tm);
extern __attribute__((nothrow))
    int VOICE_say_time_epoch(struct VOICE_attr_t *attr, time_t epoch);

    /**
     *  say aux voice
    */
extern __attribute__((nothrow))
    int VOICE_say_setting(struct VOICE_attr_t *attr, enum VOICE_setting_part_t setting, void *arg);
extern __attribute__((nothrow))
    int VOICE_say_setting_part(struct VOICE_attr_t *attr, enum VOICE_setting_part_t setting, struct tm const *tm, void *arg);

    /**
     *  get next ring tone index & play ring tone at index
    */
extern __attribute__((nothrow))
    int VOICE_select_ringtone(struct VOICE_attr_t *attr, int ringtone_id);
extern __attribute__((nothrow, pure))
    int VOICE_next_ringtone(struct VOICE_attr_t *attr, int ringtone_id);
extern __attribute__((nothrow, pure))
    int VOICE_prev_ringtone(struct VOICE_attr_t *attr, int ringtone_id);

extern __attribute__((nothrow))
    int VOICE_play_ringtone(struct VOICE_attr_t *attr, int ringtone_id);

    /**
     *  play reminder
    */
extern __attribute__((nothrow))
    int VOICE_play_reminder(struct VOICE_attr_t *attr, int reminder_id);

__END_DECLS
#endif
