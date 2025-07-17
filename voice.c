#include <ultracore/nvm.h>
#include <ultracore/log.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "limits.h"
#include "audio/mplayer.h"
#include "voice.h"

/***************************************************************************
 * @def
 ***************************************************************************/
#define VOICE_EXT                   ".lc3"
static char const *filefmt = "%s%02X%02X.lc3";

// VOICE speak weekday before or after date
enum VOICE_wday_fmt_t
{
    WFMT_LEAD,
    WFMT_TAIL,
};

struct VOICE_t
{
    char const *lcid;
    char const *folder;
    char const *voice;
    uint32_t tempo;

    int16_t lcidx;
    int16_t tail_idx;

    enum LOCALE_dfmt_t default_dfmt;
    enum LOCALE_hfmt_t default_hfmt;
    enum VOICE_wday_fmt_t wfmt;
    enum LOCALE_hfmt_t fixed_gr;
};

/***************************************************************************
 * @internal
 ***************************************************************************/
static struct VOICE_t const *voice_sel;
static struct LOCALE_t *locale_ptr;

static char const *common_folder = "voice/";

static struct VOICE_t const __voices[] =
{
    {
        .lcid = "en-AU",
        .lcidx = 11,
        .folder = "voice/enAUf/",
        .voice = "Chloe",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-AU",
        .lcidx = 12,
        .folder = "voice/enAUm/",
        .voice = "James",
        .tempo = 120,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-UK",
        .lcidx = 9,
        .folder = "voice/enUKf/",
        .voice = "Amelia",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-UK",
        .folder = "voice/enUKm/",
        .voice = "Harry",
        .tempo = 100,
        .lcidx = 10,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-US",
        .lcidx = 7,
        .folder = "voice/enUSf/",
        .voice = "Ava",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-US",
        .lcidx = 8,
        .folder = "voice/enUSm/",
        .voice = "Ethan",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "ja-JP",
        .lcidx = 13,
        .folder = "voice/jaJPf/",
        .voice = "Sakura",
        .tempo = 50,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "ja-JP",
        .lcidx = 14,
        .folder = "voice/jaJPm/",
        .voice = "Itsuki",
        .tempo = 50,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "fr",
        .lcidx = 19,
        .folder = "voice/frFRf/",
        .voice = "Jeanne",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fr",
        .lcidx = 20,
        .folder = "voice/frFRm/",
        .voice = "Lucien",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es",
        .folder = "voice/esESf/",
        .lcidx = 25,
        .voice = "Rosalyn",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es",
        .lcidx = 26,
        .folder = "voice/esESm/",
        .voice = "Felipe",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es-MX",
        .lcidx = 27,
        .folder = "voice/esMXf/",
        .voice = "Lola",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es-MX",
        .lcidx = 28,
        .folder = "voice/esMXm/",
        .voice = "Antonio",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "de",
        .lcidx = 31,
        .folder = "voice/deDEf/",
        .voice = "Lena",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "de",
        .lcidx = 32,
        .folder = "voice/deDEm/",
        .voice = "Stefan",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "it",
        .lcidx = 37,
        .folder = "voice/itITf/",
        .voice = "Adriana",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "it",
        .lcidx = 38,
        .folder = "voice/itITm/",
        .voice = "Francesco",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "zh-CN",
        .lcidx = 1,
        .voice = "Liu",
        .folder = "voice/zhCNf/",
        .tempo = 100,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-CN",
        .lcidx = 2,
        .voice = "Xing",
        .folder = "voice/zhCNm/",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-HK",
        .lcidx = 3,
        .voice = "Tai",
        .folder = "voice/zhHKf/",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-HK",
        .lcidx = 4,
        .folder = "voice/zhHKm/",
        .voice = "Choy",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-TW",
        .lcidx = 5,
        .folder = "voice/zhTWf/",
        .voice = "Ting",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-TW",
        .lcidx = 6,
        .folder = "voice/zhTWm/",
        .voice = "Hao",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
#ifndef DEBUG
    {
        .lcid = "pt-BR",
        .lcidx = 43,
        .folder = "voice/ptBRf/",
        .voice = "Carolina",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-BR",
        .lcidx = 44,
        .folder = "voice/ptBRm/",
        .voice = "Pedro",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-PT",
        .lcidx = 45,
        .folder = "voice/ptPTf/",
        .voice = "Maria",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-PT",
        .lcidx = 46,
        .folder = "voice/ptPTm/",
        .voice = "Miguel",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ko",
        .lcidx = 49,
        .folder = "voice/koKRf/",
        .voice = "Jiyoon",
        .tempo = 100,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .wfmt = WFMT_TAIL,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "ko",
        .lcidx = 50,
        .folder = "voice/koKRm/",
        .voice = "Byeongho",
        .tempo = 100,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .wfmt = WFMT_TAIL,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "nb-NO",
        .lcidx = 55,
        .folder = "voice/nbNOf/",
        .voice = "Anita",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "nb-NO",
        .lcidx = 56,
        .folder = "voice/nbNOm/",
        .voice = "Espen",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fi-FI",
        .lcidx = 61,
        .folder = "voice/fiFIf/",
        .voice = "Ada",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fi-FI",
        .lcidx = 62,
        .folder = "voice/fiFIm/",
        .voice = "Fidan",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "sv-SE",
        .lcidx = 69,
        .folder = "voice/svSEf/",
        .voice = "Sofie",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "sv-SE",
        .lcidx = 70,
        .folder = "voice/svSEm/",
        .voice = "Mattias",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "da-DK",
        .lcidx = 73,
        .folder = "voice/daDKf/",
        .voice = "Ella",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "da-DK",
        .lcidx = 74,
        .folder = "voice/daDKm/",
        .voice = "Noah",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "th-TH",
        .lcidx = 79,
        .folder = "voice/thTHf/",
        .voice = "Premwadee",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "th-TH",
        .lcidx = 80,
        .folder = "voice/thTHm/",
        .voice = "Niwat",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "vi-VN",
        .lcidx = 85,
        .folder = "voice/viVNf/",
        .voice = "HoaiMy",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "vi-VN",
        .lcidx = 86,
        .folder = "voice/viVNm/",
        .voice = "NamMinh",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "id-ID",
        .lcidx = 91,
        .folder = "voice/idIDf/",
        .voice = "Indah",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "id-ID",
        .lcidx = 92,
        .folder = "voice/idIDm/",
        .voice = "Adhiarja",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "km-KH",
        .lcidx = 97,
        .folder = "voice/kmKHf/",
        .voice = "Sreymom",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "km-KH",
        .lcidx = 98,
        .folder = "voice/kmKHm/",
        .voice = "Piseth",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "lo-LA",
        .lcidx = 103,
        .folder = "voice/loLAf/",
        .voice = "Keomany",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "lo-LA",
        .lcidx = 104,
        .folder = "voice/loLAm/",
        .voice = "Chanthavong",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "tl-PH",
        .lcidx = 109,
        .folder = "voice/tlPHf/",
        .voice = "Gloria",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "tl-PH",
        .lcidx = 110,
        .folder = "voice/tlPHm/",
        .voice = "Sergio",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "ms-MY",
        .lcidx = 115,
        .folder = "voice/msMYf/",
        .voice = "Yasmin",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ms-MY",
        .lcidx = 116,
        .folder = "voice/msMYm/",
        .voice = "Osman",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "my-MM",
        .lcidx = 121,
        .folder = "voice/myMMf/",
        .voice = "Nilar",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "my-MM",
        .lcidx = 122,
        .folder = "voice/myMMm/",
        .voice = "Thiha",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
    },
#endif
};

/***************************************************************************
 * @def
 ***************************************************************************/
enum VOICE_common_map
{
    IDX_RING_TONE_0             = 1,
    IDX_RING_TONE_END           = 20,
#define RING_TONE_COUNT                 (IDX_RING_TONE_END - IDX_RING_TONE_0 + 1)

    IDX_SETTING_DONE            = 255,
};
#define IDX_COMMON               (255)

enum VOICE_map
{
    IDX_HOUR_0                  = 1,   // 00:00 o'clock AM
        // ... IDX 25 = H12                 // 12:00 o'clock PM
        // ... IDX 36 = H23
        IDX_O_CLOCK             = 25,
    IDX_MINUTE_0                = IDX_O_CLOCK,
    IDX_MINUTE_1,
        // ...
        IDX_MINUTE_59           = 84,

    IDX_MONTH_0                 = 85,
        IDX_JANURAY             = IDX_MONTH_0,
        IDX_FEBRUARY,
        IDX_MARCH,
        IDX_APRIL,
        IDX_MAY,
        IDX_JUNE,
        IDX_JULY,
        IDX_AUGUST,
        IDX_SEPTEMBER,
        IDX_OCTOBER,
        IDX_NOVEMBER,
        IDX_DECEMBER,
    IDX_MDAY_1                  = 97,
        // ...
        IDX_MDAY_31,
    IDX_WDAY_0                  = 128,
        IDX_SUNDAY                  = IDX_WDAY_0,
        IDX_MONDAY,
        IDX_TUESDAY,
        IDX_WEDNESDAY,
        IDX_THURSDAY,
        IDX_FRIDAY,
        IDX_SATURDAY,

    IDX_NOW                     = 141,
    IDX_TODAY                   = 142,

    IDX_GR_MORNING              = 150,
    IDX_GR_AFTERNOON,
    IDX_GR_EVENING,
    IDX_IN_MORNING              = 155,  // 00:00 ~ 11:59 in the morning
    IDX_IN_AFTERNOON,                   // 12:00 ~ 17:59 in the afternoon
    IDX_IN_EVENING,                     // 18:00 ~ 23:59 in the evening
    IDX_MID_NIGHT               = 160,
    IDX_NOON                    = 161,

    IDX_SETTING_LANG            = 170,
    IDX_SETTING_VOICE,
    IDX_SETTING_YEAR,
    IDX_SETTING_MONTH,
    IDX_SETTING_MDAY,
    IDX_SETTING_HOUR,
    IDX_SETTING_MINUTE,
    IDX_SETTING_ALARM_HOUR,
    IDX_SETTING_ALARM_MIN,
    IDX_SETTING_VOLUME          = IDX_SETTING_LANG,

    IDX_REMINDER_0              = 180,
    IDX_REMINDER_END            = 195,
#define REMINDER_COUNT                  (IDX_REMINDER_END - IDX_REMINDER_0 + 1)

    IDX_SETTING_EXT_LOW_BATT    = 197,
    IDX_SETTING_EXT_ALARM_ON    = 198,
    IDX_SETTING_EXT_ALARM_OFF   = 199,

    IDX_YEAR_2024               = 224,
        // ...
        IDX_YEAR_2050           = 250,
#define IDX_YEAR_LO                     (IDX_YEAR_2024 + (COMPILE_YEAR - 2024))
};

/***************************************************************************
 * @def: private
 ***************************************************************************/
static inline bool LOCALE_is_exists(int idx)
{
    return (unsigned)idx < lengthof(__voices) ;
}

static void LOCALE_set(struct VOICE_t const *locale)
{
    if (NULL != locale && voice_sel != locale)
    {
        voice_sel = locale;

        if (DFMT_DEFAULT == locale_ptr->dfmt)
            locale_ptr->dfmt = locale->default_dfmt;
        if (HFMT_DEFAULT == locale_ptr->hfmt)
            locale_ptr->hfmt = locale->default_hfmt;
    }
}

/***************************************************************************
 * @implements
 ***************************************************************************/
int16_t VOICE_init(int16_t voice_id, struct LOCALE_t *locale)
{
    locale_ptr = locale;

    int16_t select_idx = VOICE_select_voice(voice_id);
    LOCALE_set(&__voices[select_idx]);

    return select_idx;
}

unsigned VOICE_get_count(void)
{
    return lengthof(__voices);
}

void VOICE_enum_avail_locales(VOICE_avail_locales_callback_t callback, void *arg)
{
    struct VOICE_t const *last_locale = NULL;
    for (int idx = lengthof(__voices) - 1; idx >= 0; idx --)
    {
        if (LOCALE_is_exists(idx))
        {
            last_locale = &__voices[idx];
            break;
        }
    }

    for (int idx = 0; idx < (int)lengthof(__voices); idx ++)
    {
        if (LOCALE_is_exists(idx))
        {
            struct VOICE_t const *locale = &__voices[idx];
            callback(idx, locale->lcid, locale->voice, arg,
                NULL == last_locale || locale == last_locale);
        }
    }
}

enum LOCALE_dfmt_t VOICE_get_default_dfmt()
{
    return voice_sel->default_dfmt;
}

enum LOCALE_hfmt_t VOICE_get_default_hfmt()
{
    return voice_sel->default_hfmt;
}

int16_t VOICE_select_voice(int16_t voice_id)
{
    if (voice_id < (int)lengthof(__voices) && LOCALE_is_exists(voice_id))
    {
        LOCALE_set(&__voices[voice_id]);
        return (int16_t)(voice_sel - __voices);
    }
    else
    {
        for (int16_t idx = 0; idx < (int)lengthof(__voices); idx ++)
        {
            if (LOCALE_is_exists(idx))
            {
                LOCALE_set(&__voices[idx]);
                return idx;
            }
        }
        return -1;
    }
}

int16_t VOICE_select_lcid(char const *lcid)
{
    struct VOICE_t const *fallback_locale = NULL;
    struct VOICE_t const *lang_match = NULL;

    for (int16_t idx = 0; idx < (int)lengthof(__voices); idx ++)
    {
        if (LOCALE_is_exists(idx))
        {
            if (NULL == fallback_locale)
                fallback_locale = &__voices[idx];

            if (! lang_match)
            {
                if (0 == strncasecmp(lcid, __voices[idx].lcid, 2))
                    lang_match = &__voices[idx];
            }

            if (0 == strcasecmp(lcid, __voices[idx].lcid))
            {
                LOCALE_set(&__voices[idx]);
                return idx;
            }
        }
    }

    if (NULL != lang_match)
    {
        LOCALE_set(lang_match);
        return (int16_t)(lang_match - __voices);
    }
    else if (NULL != fallback_locale)
    {
        LOCALE_set(fallback_locale);
        return (int16_t)(fallback_locale - __voices);
    }
    else
        return -1;
}

int16_t VOICE_next_locale(void)
{
    struct VOICE_t const *first_locale = NULL;
    for (int idx = 0; idx <  (int)lengthof(__voices); idx ++)
    {
        if (LOCALE_is_exists(idx))
        {
            first_locale = &__voices[idx];
            break;
        }
    }

    if (0 <= voice_sel - __voices)
    {
        for (int16_t idx = (int16_t)(voice_sel - __voices + 1); idx < (int)lengthof(__voices); idx ++)
        {
            if (LOCALE_is_exists(idx))
            {
                LOCALE_set(&__voices[idx]);
                return idx;
            }
        }
    }

    if (first_locale)
    {
        LOCALE_set(first_locale);
        return (int16_t)(first_locale - __voices);
    }
    else
        return -1;
}

int VOICE_say_date(struct tm const *tm)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    int full_year = tm->tm_year + 1900;
    full_year = YEAR_ROUND_LO > full_year ? YEAR_ROUND_LO :
        (YEAR_ROUND_HI < full_year ?        YEAR_ROUND_HI : full_year);

    char filename[64];
    int retval;

    sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_TODAY);
    if (0 == (retval = mplayer_playlist_queue(filename)))
        mplayer_playlist_queue_intv(voice_sel->tempo);

    int year_vidx = full_year - YEAR_ROUND_LO + IDX_YEAR_LO;
    int month_vidx = tm->tm_mon + IDX_JANURAY;
    int day_vidx = tm->tm_mday - 1 + IDX_MDAY_1;
    int wday_vidx = tm->tm_wday + IDX_SUNDAY;

    if (0 == retval && WFMT_LEAD == voice_sel->wfmt)
    {
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, wday_vidx);
        retval = mplayer_playlist_queue(filename);
        if (0 == retval)
            mplayer_playlist_queue_intv(voice_sel->tempo);
    }

    enum LOCALE_dfmt_t dfmt = locale_ptr->dfmt;
    if (DFMT_DEFAULT == dfmt)
        dfmt = voice_sel->default_dfmt;

    switch (dfmt)
    {
    case DFMT_DEFAULT:
    case DFMT_YYMMDD:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, year_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, month_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, day_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;

    case DFMT_DDMMYY:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, day_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, month_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, year_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;

    case DFMT_MMDDYY:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, month_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, day_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, year_vidx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;
    };

    if (0 == retval && WFMT_TAIL == voice_sel->wfmt)
    {
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, wday_vidx);
        retval = mplayer_playlist_queue(filename);
        if (0 == retval)
            mplayer_playlist_queue_intv(voice_sel->tempo);
    }

    if (0 == retval && -1 != voice_sel->tail_idx)
    {
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, voice_sel->tail_idx);
        retval = mplayer_playlist_queue(filename);
        if (0 == retval)
            mplayer_playlist_queue_intv(voice_sel->tempo);
    }
    return retval;
}

int VOICE_say_date_epoch(time_t epoch)
{
    return VOICE_say_date(localtime(&epoch));
}

int VOICE_say_time(struct tm const *tm)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    LOG_info("%04d/%02d/%02d %02d:%02d:%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    int retval = 0;
    char filename[64];
    int saying_hour;

    enum LOCALE_hfmt_t hfmt = locale_ptr->hfmt;
    if (HFMT_DEFAULT == hfmt)
        hfmt = voice_sel->default_hfmt;

    // 0/12 点整
    if (0 == tm->tm_min && 0 == tm->tm_hour % 12)
    {
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_NOW);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }

        if (0 == retval)
        {
            if (0 == tm->tm_hour)
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_MID_NIGHT);
            }
            else
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_NOON);
            }
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }

        return retval;
    }
    else
        saying_hour = tm->tm_hour;

    if (HFMT_DEFAULT != voice_sel->fixed_gr)
    {
        switch (voice_sel->fixed_gr)
        {
        case HFMT_DEFAULT:
            break;

        case HFMT_12:
            saying_hour %= 12;
            goto fixed_gr12;

        case HFMT_24:
            if (HFMT_12 == hfmt && 12 != saying_hour)
                saying_hour %= 12;
            goto fixed_gr24;
        }
    }

    if (HFMT_12 == hfmt)
    {
    fixed_gr12:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_NOW);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            if (0 == saying_hour || 12 == saying_hour)
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, 12 + IDX_HOUR_0);
            }
            else
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, saying_hour % 12 + IDX_HOUR_0);
            }
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, tm->tm_min + IDX_MINUTE_0);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            if (tm->tm_hour < 12)
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_IN_MORNING);
            }
            else if (tm->tm_hour < 18)
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_IN_AFTERNOON);
            }
            else
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_IN_EVENING);
            }
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
    }
    else
    {
    fixed_gr24:
        if (0 == retval)
        {
            if (tm->tm_hour < 12)
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_GR_MORNING);
            }
            else if (tm->tm_hour < 18)
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_GR_AFTERNOON);
            }
            else
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, IDX_GR_EVENING);
            }
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_NOW);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, saying_hour + IDX_HOUR_0);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, tm->tm_min + IDX_MINUTE_0);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
    }

    if (-1 != voice_sel->tail_idx)
    {
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, voice_sel->tail_idx);

        retval = mplayer_playlist_queue(filename);
        if (0 == retval)
            mplayer_playlist_queue_intv(voice_sel->tempo);
    }
    return retval;
}

int VOICE_say_time_epoch(time_t epoch, int8_t dst_minute_offset)
{
    epoch += (int)dst_minute_offset * 60;
    struct tm *tm = localtime(&epoch);

    if (0 != dst_minute_offset)
        LOG_warning("DST shift %d minutes", dst_minute_offset);

    return VOICE_say_time(tm);
}

int VOICE_say_setting(enum VOICE_setting_part_t setting, void *arg)
{
    char filename[64];

    if (setting == VOICE_SETTING_DONE)
    {
        sprintf(filename, filefmt, common_folder, IDX_COMMON, IDX_SETTING_DONE);
        return mplayer_play(filename);
    }

    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    switch (setting)
    {
    case VOICE_SETTING_DONE:
        break;

    case VOICE_SETTING_LANG:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_LANG);
        return mplayer_play(filename);

    case VOICE_SETTING_HOUR:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_HOUR);
        return mplayer_play(filename);

    case VOICE_SETTING_MINUTE:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_MINUTE);
        return mplayer_play(filename);

    case VOICE_SETTING_YEAR:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_YEAR);
        return mplayer_play(filename);

    case VOICE_SETTING_MONTH:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_MONTH);
        return mplayer_play(filename);

    case VOICE_SETTING_MDAY:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_MDAY);
        return mplayer_play(filename);

    case VOICE_SETTING_ALARM_HOUR:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_ALARM_HOUR);
        return mplayer_play(filename);

    case VOICE_SETTING_ALARM_MIN:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_ALARM_MIN);
        return mplayer_play(filename);

    case VOICE_SETTING_ALARM_RINGTONE:
        return VOICE_play_ringtone((int)arg);

    case VOICE_SETTING_EXT_LOW_BATT:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_EXT_LOW_BATT);
        return mplayer_play(filename);

    case VOICE_SETTING_EXT_ALARM_ON:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_EXT_ALARM_ON);
        return mplayer_play(filename);

    case VOICE_SETTING_EXT_ALARM_OFF:
        sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_EXT_ALARM_OFF);
        return mplayer_play(filename);
    };

    return EINVAL;
}

int VOICE_say_setting_part(enum VOICE_setting_part_t setting,
    struct tm const *tm, void *arg)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    char filename[64];
    int retval = 0;

    switch (setting)
    {
    case VOICE_SETTING_LANG:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_SETTING_VOICE);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;

    case VOICE_SETTING_HOUR:
    case VOICE_SETTING_ALARM_HOUR:
        if (0 == retval)
        {
            enum LOCALE_hfmt_t hfmt = locale_ptr->hfmt;
            if (HFMT_DEFAULT == hfmt)
                hfmt = voice_sel->default_hfmt;

            if (HFMT_12 == hfmt)
            {
                // 强制24小时地区(华文区)总是读“早午晚”在后
                if (HFMT_24 != voice_sel->fixed_gr)
                {
                    if (0 == tm->tm_hour || 12 == tm->tm_hour)
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, 12 + IDX_HOUR_0);
                    }
                    else
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, tm->tm_hour % 12 + IDX_HOUR_0);
                    }
                    retval = mplayer_playlist_queue(filename);
                    if (0 == retval)
                        mplayer_playlist_queue_intv(voice_sel->tempo);
                }

                if (0 == retval)
                {
                    if (tm->tm_hour < 12)
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, IDX_IN_MORNING);

                        if (0 == (retval = mplayer_playlist_queue(filename)))
                            mplayer_playlist_queue_intv(voice_sel->tempo);
                    }
                    else if (tm->tm_hour < 18)
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, IDX_IN_AFTERNOON);
                        if (0 == (retval = mplayer_playlist_queue(filename)))
                            mplayer_playlist_queue_intv(voice_sel->tempo);
                    }
                    else
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, IDX_IN_EVENING);
                        if (0 == (retval = mplayer_playlist_queue(filename)))
                            mplayer_playlist_queue_intv(voice_sel->tempo);
                    }
                }

                if (HFMT_24 == voice_sel->fixed_gr)
                {
                    if (0 == tm->tm_hour || 12 == tm->tm_hour)
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, 12 + IDX_HOUR_0);
                    }
                    else
                    {
                        sprintf(filename, filefmt, voice_sel->folder,            voice_sel->lcidx, tm->tm_hour % 12 + IDX_HOUR_0);
                    }
                    if (0 == (retval = mplayer_playlist_queue(filename)))
                        mplayer_playlist_queue_intv(voice_sel->tempo);
                }
            }
            else
            {
                sprintf(filename, filefmt, voice_sel->folder,    voice_sel->lcidx, tm->tm_hour + IDX_HOUR_0);

                if (0 == (retval = mplayer_playlist_queue(filename)))
                    mplayer_playlist_queue_intv(voice_sel->tempo);
            }
        }
        break;

    case VOICE_SETTING_MINUTE:
    case VOICE_SETTING_ALARM_MIN:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, tm->tm_min + IDX_MINUTE_0);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;

    case VOICE_SETTING_YEAR:
        if (0 == retval)
        {
            int idx = tm->tm_year + 1900;
            idx = (YEAR_ROUND_LO > idx ? YEAR_ROUND_LO : (YEAR_ROUND_HI < idx ? YEAR_ROUND_HI : idx)) -
                YEAR_ROUND_LO + IDX_YEAR_LO;

            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, idx);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;

    case VOICE_SETTING_MDAY:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, tm->tm_mday - 1 + IDX_MDAY_1);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        if (0 == retval && voice_sel->default_dfmt != DFMT_YYMMDD)
            goto fallthrough_month;
        break;

    fallthrough_month:
    case VOICE_SETTING_MONTH:
        if (0 == retval)
        {
            sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, tm->tm_mon + IDX_JANURAY);
            if (0 == (retval = mplayer_playlist_queue(filename)))
                mplayer_playlist_queue_intv(voice_sel->tempo);
        }
        break;

    case VOICE_SETTING_ALARM_RINGTONE:
        retval = VOICE_play_ringtone((int)arg);
        break;

    case VOICE_SETTING_COUNT:
        retval = EINVAL;
        break;

    case VOICE_SETTING_EXT_LOW_BATT:
    case VOICE_SETTING_EXT_ALARM_ON:
    case VOICE_SETTING_EXT_ALARM_OFF:
        break;
    };

    return retval;
}

int VOICE_select_ringtone(int ringtone_id)
{
    return ringtone_id % RING_TONE_COUNT;
}

int VOICE_next_ringtone(int ringtone_id)
{
    return (ringtone_id + 1) % RING_TONE_COUNT;
}

int VOICE_prev_ringtone(int ringtone_id)
{
    if (ringtone_id > 0)
        return ringtone_id - 1;
    else
        return RING_TONE_COUNT - 1;
}

int VOICE_play_ringtone(int ringtone_id)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;
    char filename[64];

    if (0 > ringtone_id)
        ringtone_id = 0;
    else
        ringtone_id %= RING_TONE_COUNT;

    sprintf(filename, filefmt, common_folder, IDX_COMMON, IDX_RING_TONE_0 + ringtone_id);
    return mplayer_play(filename);
}

int VOICE_play_reminder(int reminder_id)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    char filename[64];
    sprintf(filename, filefmt, voice_sel->folder, voice_sel->lcidx, IDX_REMINDER_0 + reminder_id);

    return mplayer_play(filename);
}
