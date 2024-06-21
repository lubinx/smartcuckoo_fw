#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ultracore/nvm.h>

#include "limits.h"
#include "mplayer.h"
#include "mplayer_voice.h"

/***************************************************************************
 * @def: voices
 ***************************************************************************/
#define NVM_VOICE_LOCALE                NVM_DEFINE_KEY('V', 'L', 'O', 'C')

// VOICE speak weekday before or after date
enum VOICE_wday_fmt_t
{
    WFMT_LEAD,
    WFMT_TAIL,
};

struct VOICE_t
{
    char const *lcid;
    char const *folder_alt;
    char const *voice;
    uint32_t tempo;

    int16_t lcidx;
    int16_t tail_idx;

    enum LOCALE_dfmt_t default_dfmt;
    enum LOCALE_hfmt_t default_hfmt;
    enum VOICE_wday_fmt_t wfmt;
    enum LOCALE_hfmt_t fixed_gr;
};

static struct VOICE_t const __voices[] =
{
    {
        .lcid = "en-AU",
        .lcidx = 11,
        .folder_alt = "voice/enAUf/",
        .voice = "Chloe",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-AU",
        .lcidx = 12,
        .folder_alt = "voice/enAUm/",
        .voice = "James",
        .tempo = 120,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-UK",
        .lcidx = 9,
        .folder_alt = "voice/enUKf/",
        .voice = "Amelia",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-UK",
        .folder_alt = "voice/enUKm/",
        .voice = "Harry",
        .tempo = 200,
        .lcidx = 10,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-US",
        .lcidx = 7,
        .folder_alt = "voice/enUSf/",
        .voice = "Ava",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-US",
        .lcidx = 8,
        .folder_alt = "voice/enUSm/",
        .voice = "Ethan",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "ja-JP",
        .lcidx = 13,
        .folder_alt = "voice/jaJPf/",
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
        .folder_alt = "voice/jaJPm/",
        .voice = "Itsuki",
        .tempo = 50,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
#ifndef DEBUG
    {
        .lcid = "fr",
        .lcidx = 19,
        .folder_alt = "voice/frFRf/",
        .voice = "Jeanne",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fr",
        .lcidx = 20,
        .folder_alt = "voice/frFRm/",
        .voice = "Lucien",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es",
        .folder_alt = "voice/esESf/",
        .lcidx = 25,
        .voice = "Rosalyn",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es",
        .lcidx = 26,
        .folder_alt = "voice/esESm/",
        .voice = "Felipe",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es-MX",
        .lcidx = 27,
        .folder_alt = "voice/esMXf/",
        .voice = "Lola",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es-MX",
        .lcidx = 28,
        .folder_alt = "voice/esMXm/",
        .voice = "Antonio",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "de",
        .lcidx = 31,
        .folder_alt = "voice/deDEf/",
        .voice = "Lena",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "de",
        .lcidx = 32,
        .folder_alt = "voice/deDEm/",
        .voice = "Stefan",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "it",
        .lcidx = 37,
        .folder_alt = "voice/itITf/",
        .voice = "Adriana",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "it",
        .lcidx = 38,
        .folder_alt = "voice/itITm/",
        .voice = "Francesco",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "zh-CN",
        .lcidx = 1,
        .voice = "Liu",
        .folder_alt = "voice/zhCNf/",
        .tempo = 200,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-CN",
        .lcidx = 2,
        .voice = "Xing",
        .folder_alt = "voice/zhCNm/",
        .tempo = 200,
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
        .folder_alt = "voice/zhHKf/",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-HK",
        .lcidx = 4,
        .folder_alt = "voice/zhHKm/",
        .voice = "Choy",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-TW",
        .lcidx = 5,
        .folder_alt = "voice/zhTWf/",
        .voice = "Ting",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-TW",
        .lcidx = 6,
        .folder_alt = "voice/zhTWm/",
        .voice = "Hao",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "pt-BR",
        .lcidx = 43,
        .folder_alt = "voice/ptBRf/",
        .voice = "Carolina",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-BR",
        .lcidx = 44,
        .folder_alt = "voice/ptBRm/",
        .voice = "Pedro",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-PT",
        .lcidx = 45,
        .folder_alt = "voice/ptPTf/",
        .voice = "Maria",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-PT",
        .lcidx = 46,
        .folder_alt = "voice/ptPTm/",
        .voice = "Miguel",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ko",
        .lcidx = 49,
        .folder_alt = "voice/koKRf/",
        .voice = "Jiyoon",
        .tempo = 200,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .wfmt = WFMT_TAIL,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "ko",
        .lcidx = 50,
        .folder_alt = "voice/koKRm/",
        .voice = "Byeongho",
        .tempo = 200,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .wfmt = WFMT_TAIL,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "nb-NO",
        .lcidx = 55,
        .folder_alt = "voice/nbNOf/",
        .voice = "Anita",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "nb-NO",
        .lcidx = 56,
        .folder_alt = "voice/nbNOm/",
        .voice = "Espen",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fi-FI",
        .lcidx = 61,
        .folder_alt = "voice/fiFIf/",
        .voice = "Ada",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fi-FI",
        .lcidx = 62,
        .folder_alt = "voice/fiFIm/",
        .voice = "Fidan",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "sv-SE",
        .lcidx = 69,
        .folder_alt = "voice/svSEf/",
        .voice = "Sofie",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "sv-SE",
        .lcidx = 70,
        .folder_alt = "voice/svSEm/",
        .voice = "Mattias",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "da-DK",
        .lcidx = 73,
        .folder_alt = "voice/daDKf/",
        .voice = "Ella",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "da-DK",
        .lcidx = 74,
        .folder_alt = "voice/daDKm/",
        .voice = "Noah",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "th-TH",
        .lcidx = 79,
        .folder_alt = "voice/thTHf/",
        .voice = "Premwadee",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "th-TH",
        .lcidx = 80,
        .folder_alt = "voice/thTHm/",
        .voice = "Niwat",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "vi-VN",
        .lcidx = 85,
        .folder_alt = "voice/viVNf/",
        .voice = "HoaiMy",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "vi-VN",
        .lcidx = 86,
        .folder_alt = "voice/viVNm/",
        .voice = "NamMinh",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "id-ID",
        .lcidx = 91,
        .folder_alt = "voice/idIDf/",
        .voice = "Indah",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "id-ID",
        .lcidx = 92,
        .folder_alt = "voice/idIDm/",
        .voice = "Adhiarja",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "km-KH",
        .lcidx = 97,
        .folder_alt = "voice/kmKHf/",
        .voice = "Sreymom",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "km-KH",
        .lcidx = 98,
        .folder_alt = "voice/kmKHm/",
        .voice = "Piseth",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "lo-LA",
        .lcidx = 103,
        .folder_alt = "voice/loLAf/",
        .voice = "Keomany",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "lo-LA",
        .lcidx = 104,
        .folder_alt = "voice/loLAm/",
        .voice = "Chanthavong",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "tl-PH",
        .lcidx = 109,
        .folder_alt = "voice/tlPHf/",
        .voice = "Gloria",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "tl-PH",
        .lcidx = 110,
        .folder_alt = "voice/tlPHm/",
        .voice = "Sergio",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "ms-MY",
        .lcidx = 115,
        .folder_alt = "voice/msMYf/",
        .voice = "Yasmin",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ms-MY",
        .lcidx = 116,
        .folder_alt = "voice/msMYm/",
        .voice = "Osman",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "my-MM",
        .lcidx = 121,
        .folder_alt = "voice/myMMf/",
        .voice = "Nilar",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "my-MM",
        .lcidx = 122,
        .folder_alt = "voice/myMMm/",
        .voice = "Thiha",
        .tempo = 200,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
    },
#endif
};

static char const *common_folder_alt = "voice/";
static uint8_t __locale_exists[lengthof(__voices) / 8 + 1] = {0};

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
#define COMMON_FOLDER_IDX               (255)

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
    return 0 != (__locale_exists[idx / 8] & (1U << (idx % 8)));
}

static inline void LOCALE_set_exists(int idx)
{
    __locale_exists[idx / 8] |= (uint8_t)(1U << (idx % 8));
}

static void LOCALE_set(struct VOICE_attr_t *attr, struct VOICE_t const *locale)
{
    if (NULL != locale && attr->voice != locale)
    {
        attr->voice = locale;

        if (DFMT_DEFAULT == attr->locale->dfmt)
            attr->locale->dfmt = locale->default_dfmt;
        if (HFMT_DEFAULT == attr->locale->hfmt)
            attr->locale->hfmt = locale->default_hfmt;
    }
}

/***************************************************************************
 * @implements
 ***************************************************************************/
void VOICE_init(struct VOICE_attr_t *attr, struct SMARTCUCKOO_locale_t *locale, bool use_alt_folder)
{
    memset(attr, 0, sizeof(*attr));

    attr->locale = locale;
    attr->use_alt_folder = use_alt_folder;
}

int16_t VOICE_init_locales(struct VOICE_attr_t *attr, int16_t voice_id, bool enum_only_exists)
{
    if (0 != NVM_get(NVM_VOICE_LOCALE, &__locale_exists, sizeof(__locale_exists)))
        enum_only_exists = false;

    if (enum_only_exists)
    {
        bool all_false = true;

        for (unsigned idx = 0; idx < lengthof(__locale_exists); idx ++)
        {
            if (0 != __locale_exists[idx])
            {
                all_false = false;
                break;
            }
        }
        if (all_false)
            enum_only_exists = false;
    }
    else
        memset(&__locale_exists, 0, sizeof(__locale_exists));

    attr->voice_count = 0;
    char filename[20];
    struct VOICE_t const *fallback_locale = NULL;

    again:
    for (int idx = 0; idx < (int)lengthof(__voices); idx ++)
    {
        if (! mplayer_gpio_is_powered())
            break;

        if (enum_only_exists)
        {
            if (! LOCALE_is_exists(idx))
                continue;
        }

        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? __voices[idx].folder_alt : "",
            __voices[idx].lcidx, IDX_SETTING_LANG);

        if (mplayer_file_exists(filename))
        {
            LOCALE_set_exists(idx);

            attr->voice_count ++;
            if (NULL == fallback_locale)
                fallback_locale = &__voices[idx];
        }
        else if (enum_only_exists)
        {
            enum_only_exists = false;
            memset(&__locale_exists, 0, sizeof(__locale_exists));
            goto again;
        }
    }

    if (! enum_only_exists)
        NVM_set(NVM_VOICE_LOCALE, &__locale_exists, sizeof(__locale_exists));

    int16_t select_idx = VOICE_select_voice(attr, voice_id);
    if (-1 == select_idx)
    {
        LOCALE_set(attr, fallback_locale);
        return fallback_locale->lcidx;
    }
    else
        return select_idx;
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

enum LOCALE_dfmt_t VOICE_get_default_dfmt(struct VOICE_attr_t *attr)
{
    return attr->voice->default_dfmt;
}

enum LOCALE_hfmt_t VOICE_get_default_hfmt(struct VOICE_attr_t *attr)
{
    return attr->voice->default_hfmt;
}

int16_t VOICE_select_voice(struct VOICE_attr_t *attr, int16_t voice_id)
{
    if (voice_id < (int)lengthof(__voices) && LOCALE_is_exists(voice_id))
    {
        LOCALE_set(attr, &__voices[voice_id]);
        return (int16_t)(attr->voice - __voices);
    }
    else
    {
        for (int16_t idx = 0; idx < (int)lengthof(__voices); idx ++)
        {
            if (LOCALE_is_exists(idx))
            {
                LOCALE_set(attr, &__voices[idx]);
                return idx;
            }
        }
        return -1;
    }
}

int16_t VOICE_select_lcid(struct VOICE_attr_t *attr, char const *lcid)
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
                LOCALE_set(attr, &__voices[idx]);
                return idx;
            }
        }
    }

    if (NULL != lang_match)
    {
        LOCALE_set(attr, lang_match);
        return (int16_t)(lang_match - __voices);
    }
    else if (NULL != fallback_locale)
    {
        LOCALE_set(attr, fallback_locale);
        return (int16_t)(fallback_locale - __voices);
    }
    else
        return -1;
}

int16_t VOICE_next_locale(struct VOICE_attr_t *attr)
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

    if (0 <= attr->voice - __voices)
    {
        for (int16_t idx = (int16_t)(attr->voice - __voices + 1); idx < (int)lengthof(__voices); idx ++)
        {
            if (LOCALE_is_exists(idx))
            {
                LOCALE_set(attr, &__voices[idx]);
                return idx;
            }
        }
    }

    if (first_locale)
    {
        LOCALE_set(attr, first_locale);
        return (int16_t)(first_locale - __voices);
    }
    else
        return -1;
}

int VOICE_say_date(struct VOICE_attr_t *attr, struct tm const *tm)
{
    if (NULL == attr->voice)
        return EMODU_NOT_CONFIGURED;

    int full_year = tm->tm_year + 1900;
    full_year = YEAR_ROUND_LO > full_year ? YEAR_ROUND_LO :
        (YEAR_ROUND_HI < full_year ?        YEAR_ROUND_HI : full_year);

    char filename[20];

    sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
        attr->voice->lcidx, IDX_TODAY);
    int retval = mplayer_playlist_queue(filename, attr->voice->tempo);

    int year_vidx = full_year - YEAR_ROUND_LO + IDX_YEAR_LO;
    int month_vidx = tm->tm_mon + IDX_JANURAY;
    int day_vidx = tm->tm_mday - 1 + IDX_MDAY_1;
    int wday_vidx = tm->tm_wday + IDX_SUNDAY;

    if (0 == retval && WFMT_LEAD == attr->voice->wfmt)
    {
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, wday_vidx);
        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
    }

    enum LOCALE_dfmt_t dfmt = attr->locale->dfmt;
    if (DFMT_DEFAULT == dfmt)
        dfmt = attr->voice->default_dfmt;

    switch (dfmt)
    {
    case DFMT_DEFAULT:
    case DFMT_YYMMDD:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, year_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, month_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, day_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;

    case DFMT_DDMMYY:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, day_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, month_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, year_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;

    case DFMT_MMDDYY:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, month_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, day_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, year_vidx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;
    };

    if (0 == retval && WFMT_TAIL == attr->voice->wfmt)
    {
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, wday_vidx);
        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
    }

    if (0 == retval && -1 != attr->voice->tail_idx)
    {
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, attr->voice->tail_idx);
        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
    }
    return retval;
}

int VOICE_say_date_epoch(struct VOICE_attr_t *attr, time_t epoch)
{
    return VOICE_say_date(attr, localtime(&epoch));
}

int VOICE_say_time(struct VOICE_attr_t *attr, struct tm const *tm)
{
    if (NULL == attr->voice)
        return EMODU_NOT_CONFIGURED;

    int retval = 0;
    char filename[20];
    int saying_hour;

    enum LOCALE_hfmt_t hfmt = attr->locale->hfmt;
    if (HFMT_DEFAULT == hfmt)
        hfmt = attr->voice->default_hfmt;

    // 0/12 点整
    if (0 == tm->tm_min && 0 == tm->tm_hour % 12)
    {
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, IDX_NOW);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }

        if (0 == retval)
        {
            if (0 == tm->tm_hour)
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_MID_NIGHT);
            }
            else
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_NOON);
            }
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }

        return retval;
    }
    else
        saying_hour = tm->tm_hour;

    if (HFMT_DEFAULT != attr->voice->fixed_gr)
    {
        switch (attr->voice->fixed_gr)
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
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, IDX_NOW);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            if (0 == saying_hour || 12 == saying_hour)
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, 12 + IDX_HOUR_0);
            }
            else
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, saying_hour % 12 + IDX_HOUR_0);
            }
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, tm->tm_min + IDX_MINUTE_0);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            if (tm->tm_hour < 12)
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_IN_MORNING);
            }
            else if (tm->tm_hour < 18)
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_IN_AFTERNOON);
            }
            else
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_IN_EVENING);
            }
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
    }
    else
    {
    fixed_gr24:
        if (0 == retval)
        {
            if (tm->tm_hour < 12)
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_GR_MORNING);
            }
            else if (tm->tm_hour < 18)
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_GR_AFTERNOON);
            }
            else
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, IDX_GR_EVENING);
            }
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, IDX_NOW);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, saying_hour + IDX_HOUR_0);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, tm->tm_min + IDX_MINUTE_0);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
    }

    if (-1 != attr->voice->tail_idx)
    {
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, attr->voice->tail_idx);
        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
    }
    return retval;
}

int VOICE_say_time_epoch(struct VOICE_attr_t *attr, time_t epoch)
{
    return VOICE_say_time(attr, localtime(&epoch));
}

int VOICE_say_setting(struct VOICE_attr_t *attr, enum VOICE_setting_part_t setting, void *arg)
{
    if (NULL == attr->voice)
        return EMODU_NOT_CONFIGURED;
    char filename[20];

    switch (setting)
    {
    case VOICE_SETTING_LANG:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_LANG);
        return mplayer_play(filename);

    case VOICE_SETTING_HOUR:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_HOUR);
        return mplayer_play(filename);

    case VOICE_SETTING_MINUTE:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_MINUTE);
        return mplayer_play(filename);

    case VOICE_SETTING_YEAR:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_YEAR);
        return mplayer_play(filename);

    case VOICE_SETTING_MONTH:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_MONTH);
        return mplayer_play(filename);

    case VOICE_SETTING_MDAY:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_MDAY);
        return mplayer_play(filename);

    case VOICE_SETTING_ALARM_HOUR:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_ALARM_HOUR);
        return mplayer_play(filename);

    case VOICE_SETTING_ALARM_MIN:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_ALARM_MIN);
        return mplayer_play(filename);

    case VOICE_SETTING_ALARM_RINGTONE:
        return VOICE_play_ringtone(attr, (int)arg);

    case VOICE_SETTING_DONE:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? common_folder_alt : "",
            COMMON_FOLDER_IDX, IDX_SETTING_DONE);
        return mplayer_play(filename);

    case VOICE_SETTING_EXT_LOW_BATT:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_EXT_LOW_BATT);
        return mplayer_play(filename);

    case VOICE_SETTING_EXT_ALARM_ON:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_EXT_ALARM_ON);
        return mplayer_play(filename);

    case VOICE_SETTING_EXT_ALARM_OFF:
        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
            attr->voice->lcidx, IDX_SETTING_EXT_ALARM_OFF);
        return mplayer_play(filename);
    };

    return EINVAL;
}

int VOICE_say_setting_part(struct VOICE_attr_t *attr, enum VOICE_setting_part_t setting,
    struct tm const *tm, void *arg)
{
    if (NULL == attr->voice)
        return EMODU_NOT_CONFIGURED;

    char filename[20];
    int retval = 0;

    switch (setting)
    {
    case VOICE_SETTING_LANG:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, IDX_SETTING_VOICE);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;

    case VOICE_SETTING_HOUR:
    case VOICE_SETTING_ALARM_HOUR:
        if (0 == retval)
        {
            enum LOCALE_hfmt_t hfmt = attr->locale->hfmt;
            if (HFMT_DEFAULT == hfmt)
                hfmt = attr->voice->default_hfmt;

            if (HFMT_12 == hfmt)
            {
                // 强制24小时地区(华文区)总是读“早午晚”在后
                if (HFMT_24 != attr->voice->fixed_gr)
                {
                    if (0 == tm->tm_hour || 12 == tm->tm_hour)
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, 12 + IDX_HOUR_0);
                    }
                    else
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, tm->tm_hour % 12 + IDX_HOUR_0);
                    }
                    retval = mplayer_playlist_queue(filename, attr->voice->tempo);
                }

                if (0 == retval)
                {
                    if (tm->tm_hour < 12)
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, IDX_IN_MORNING);
                        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
                    }
                    else if (tm->tm_hour < 18)
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, IDX_IN_AFTERNOON);
                        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
                    }
                    else
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, IDX_IN_EVENING);
                        retval = mplayer_playlist_queue(filename, attr->voice->tempo);
                    }
                }

                if (HFMT_24 == attr->voice->fixed_gr)
                {
                    if (0 == tm->tm_hour || 12 == tm->tm_hour)
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, 12 + IDX_HOUR_0);
                    }
                    else
                    {
                        sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                            attr->voice->lcidx, tm->tm_hour % 12 + IDX_HOUR_0);
                    }
                    retval = mplayer_playlist_queue(filename, attr->voice->tempo);
                }
            }
            else
            {
                sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                    attr->voice->lcidx, tm->tm_hour + IDX_HOUR_0);
                retval = mplayer_playlist_queue(filename, attr->voice->tempo);
            }
        }
        break;

    case VOICE_SETTING_MINUTE:
    case VOICE_SETTING_ALARM_MIN:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, tm->tm_min + IDX_MINUTE_0);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;

    case VOICE_SETTING_YEAR:
        if (0 == retval)
        {
            int idx = tm->tm_year + 1900;
            idx = (YEAR_ROUND_LO > idx ? YEAR_ROUND_LO : (YEAR_ROUND_HI < idx ? YEAR_ROUND_HI : idx)) -
                YEAR_ROUND_LO + IDX_YEAR_LO;

            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, idx);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;

    case VOICE_SETTING_MDAY:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, tm->tm_mday - 1 + IDX_MDAY_1);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        if (0 == retval && attr->voice->default_dfmt != DFMT_YYMMDD)
            goto fallthrough_month;
        break;

    fallthrough_month:
    case VOICE_SETTING_MONTH:
        if (0 == retval)
        {
            sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
                attr->voice->lcidx, tm->tm_mon + IDX_JANURAY);
            retval = mplayer_playlist_queue(filename, attr->voice->tempo);
        }
        break;

    case VOICE_SETTING_ALARM_RINGTONE:
        retval = VOICE_play_ringtone(attr, (int)arg);
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

int VOICE_select_ringtone(struct VOICE_attr_t *attr, int ringtone_id)
{
    ARG_UNUSED(attr);
    return ringtone_id % RING_TONE_COUNT;
}

int VOICE_next_ringtone(struct VOICE_attr_t *attr, int ringtone_id)
{
    ARG_UNUSED(attr);
    return (ringtone_id + 1) % RING_TONE_COUNT;
}

int VOICE_prev_ringtone(struct VOICE_attr_t *attr, int ringtone_id)
{
    ARG_UNUSED(attr);

    if (ringtone_id > 0)
        return ringtone_id - 1;
    else
        return RING_TONE_COUNT - 1;
}

int VOICE_play_ringtone(struct VOICE_attr_t *attr, int ringtone_id)
{
    if (NULL == attr->voice)
        return EMODU_NOT_CONFIGURED;
    char filename[20];

    if (0 > ringtone_id)
        ringtone_id = 0;
    else
        ringtone_id %= RING_TONE_COUNT;

    sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? common_folder_alt : "",
        COMMON_FOLDER_IDX, IDX_RING_TONE_0 + ringtone_id);
    return mplayer_play(filename);
}

int VOICE_play_reminder(struct VOICE_attr_t *attr, int reminder_id)
{
    if (NULL == attr->voice)
        return EMODU_NOT_CONFIGURED;
    char filename[20];

    sprintf(filename, "%s%02X%02X", attr->use_alt_folder ? attr->voice->folder_alt : "",
        attr->voice->lcidx, IDX_REMINDER_0 + reminder_id);
    return mplayer_play(filename);
}
