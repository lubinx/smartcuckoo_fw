#include <ultracore/nvm.h>
#include <ultracore/log.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>

#include "limits.h"
#include "audio/mplayer.h"
#include "voice.h"

/***************************************************************************
 * @def
 ***************************************************************************/
#define IDX_CUSTOM_REMINDER_START   (10000)
#define IDX_CUSTOM_REMINDER_END     (19999)

#define IDX_CUSTOM_RINGTONE_START   (20000)
#define IDX_CUSTOM_RINGTONE_END     (29999)

#define EXT_VOICE                   ".lc3"
#define EXT_CUSTOM                  ".wav"

static char const *common_folder = "/voice/";
static char const *custom_reminder_folder = "/download/reminder/";
static char const *custom_ringtone_folder = "/download/ringtone/";

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
static struct LOCALE_t const *locale_ptr;

static struct VOICE_t const __voices[] =
{
    {
        .lcid = "en-AU",
        .folder = "/voice/enAUf/",
        .voice = "Chloe",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-AU",
        .folder = "/voice/enAUm/",
        .voice = "James",
        .tempo = 120,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-UK",
        .folder = "/voice/enUKf/",
        .voice = "Amelia",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-UK",
        .folder = "/voice/enUKm/",
        .voice = "Harry",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-US",
        .folder = "/voice/enUSf/",
        .voice = "Ava",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-US",
        .folder = "/voice/enUSm/",
        .voice = "Ethan",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    /*
    {
        .lcid = "en-CA",
        .folder = "/voice/enCAf/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "en-CA",
        .folder = "/voice/enCAm/",
        .voice = "Unknown",
        .tempo = 120,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    */
    {
        .lcid = "ja-JP",
        .folder = "/voice/jaJPf/",
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
        .folder = "/voice/jaJPm/",
        .voice = "Itsuki",
        .tempo = 50,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "fr-FR",
        .folder = "/voice/frFRf/",
        .voice = "Jeanne",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fr-FR",
        .folder = "/voice/frFRm/",
        .voice = "Lucien",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    /*
    {
        .lcid = "fr-CA",
        .folder = "/voice/frCAf/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "fr-CA",
        .folder = "/voice/frCAm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    */
    {
        .lcid = "es-ES",
        .folder = "/voice/esESf/",
        .voice = "Rosalyn",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es-ES",
        .folder = "/voice/esESm/",
        .voice = "Felipe",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "es-MX",
        .folder = "/voice/esMXf/",
        .voice = "Lola",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "es-MX",
        .folder = "/voice/esMXm/",
        .voice = "Antonio",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "de-DE",
        .folder = "/voice/deDEf/",
        .voice = "Lena",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "de-DE",
        .folder = "/voice/deDEm/",
        .voice = "Stefan",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "it-IT",
        .folder = "/voice/itITf/",
        .voice = "Adriana",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "it-IT",
        .folder = "/voice/itITm/",
        .voice = "Francesco",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "zh-CN",
        .voice = "Liu",
        .folder = "/voice/zhCNf/",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-CN",
        .voice = "Xing",
        .folder = "/voice/zhCNm/",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-HK",
        .voice = "Tai",
        .folder = "/voice/zhHKf/",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-HK",
        .folder = "/voice/zhHKm/",
        .voice = "Choy",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "zh-TW",
        .folder = "/voice/zhTWf/",
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
        .folder = "/voice/zhTWm/",
        .voice = "Hao",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
        .wfmt = WFMT_TAIL,
    },
    {
        .lcid = "pt-BR",
        .folder = "/voice/ptBRf/",
        .voice = "Carolina",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "pt-BR",
        .folder = "/voice/ptBRm/",
        .voice = "Pedro",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "pt-PT",
        .folder = "/voice/ptPTf/",
        .voice = "Maria",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pt-PT",
        .folder = "/voice/ptPTm/",
        .voice = "Miguel",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ko-KR",
        .folder = "/voice/koKRf/",
        .voice = "Jiyoon",
        .tempo = 100,
        .tail_idx = 145,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .wfmt = WFMT_TAIL,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "ko-KR",
        .folder = "/voice/koKRm/",
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
        .folder = "/voice/nbNOf/",
        .voice = "Anita",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "nb-NO",
        .folder = "/voice/nbNOm/",
        .voice = "Espen",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fi-FI",
        .folder = "/voice/fiFIf/",
        .voice = "Ada",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "fi-FI",
        .folder = "/voice/fiFIm/",
        .voice = "Fidan",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "sv-SE",
        .folder = "/voice/svSEf/",
        .voice = "Sofie",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "sv-SE",
        .folder = "/voice/svSEm/",
        .voice = "Mattias",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "da-DK",
        .folder = "/voice/daDKf/",
        .voice = "Ella",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "da-DK",
        .folder = "/voice/daDKm/",
        .voice = "Noah",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    /*
    {
        .lcid = "nl-NL",
        .folder = "/voice/nlNLf/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "nl-NL",
        .folder = "/voice/nlNLm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pl-PL",
        .folder = "/voice/plPLm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "pl-PL",
        .folder = "/voice/plPLm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ro-RO",
        .folder = "/voice/roROf/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ro-RO",
        .folder = "/voice/roROm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ru-RU",
        .folder = "/voice/ruRUf/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ru-RU",
        .folder = "/voice/ruRUm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    */
    {
        .lcid = "th-TH",
        .folder = "/voice/thTHf/",
        .voice = "Premwadee",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "th-TH",
        .folder = "/voice/thTHm/",
        .voice = "Niwat",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "vi-VN",
        .folder = "/voice/viVNf/",
        .voice = "HoaiMy",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "vi-VN",
        .folder = "/voice/viVNm/",
        .voice = "NamMinh",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "id-ID",
        .folder = "/voice/idIDf/",
        .voice = "Indah",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "id-ID",
        .folder = "/voice/idIDm/",
        .voice = "Adhiarja",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "km-KH",
        .folder = "/voice/kmKHf/",
        .voice = "Sreymom",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "km-KH",
        .folder = "/voice/kmKHm/",
        .voice = "Piseth",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "lo-LA",
        .folder = "/voice/loLAf/",
        .voice = "Keomany",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "lo-LA",
        .folder = "/voice/loLAm/",
        .voice = "Chanthavong",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "tl-PH",
        .folder = "/voice/tlPHf/",
        .voice = "Gloria",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "tl-PH",
        .folder = "/voice/tlPHm/",
        .voice = "Sergio",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_MMDDYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "ms-MY",
        .folder = "/voice/msMYf/",
        .voice = "Yasmin",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "ms-MY",
        .folder = "/voice/msMYm/",
        .voice = "Osman",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_24,
    },
    {
        .lcid = "my-MM",
        .folder = "/voice/myMMf/",
        .voice = "Nilar",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
    },
    {
        .lcid = "my-MM",
        .folder = "/voice/myMMm/",
        .voice = "Thiha",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_YYMMDD,
        .default_hfmt = HFMT_24,
        .fixed_gr = HFMT_24,
    },
    /*
    {
        .lcid = "ar-SA",
        .folder = "/voice/arSAf/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    {
        .lcid = "ar-SA",
        .folder = "/voice/arSAm/",
        .voice = "Unknown",
        .tempo = 100,
        .tail_idx = -1,
        .default_dfmt = DFMT_DDMMYY,
        .default_hfmt = HFMT_12,
    },
    */
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
    IDX_IN_AFTERNOON,        // 12:00 ~ 17:59 in the afternoon
    IDX_IN_EVENING,          // 18:00 ~ 23:59 in the evening
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
static int VOICE_play(int idx)
{
    char filename[32];

// CUSTOM reminder
    if (IDX_CUSTOM_REMINDER_START <= idx && IDX_CUSTOM_REMINDER_END >= idx)
        sprintf(filename, "%srmd_%02X" EXT_CUSTOM, custom_reminder_folder, idx - IDX_CUSTOM_REMINDER_START);
// CUSTOM ringtone
    else if (IDX_CUSTOM_RINGTONE_START <= idx && IDX_CUSTOM_RINGTONE_END >= idx)
        sprintf(filename, "%salm_%02X" EXT_CUSTOM, custom_ringtone_folder, idx - IDX_CUSTOM_RINGTONE_START);
// common folder: ringtone
    else if (IDX_RING_TONE_0 <= idx && IDX_RING_TONE_END >= idx)
        sprintf(filename, "%s%02X" EXT_VOICE, common_folder, idx);
// common folder: setting done
    else if (IDX_SETTING_DONE == idx)
        sprintf(filename, "%s%02X" EXT_VOICE, common_folder, idx);
// locale folder
    else if (0xFF > idx)
        sprintf(filename, "%s%02X" EXT_VOICE, voice_sel->folder, idx);
    else
        return ENOENT;

    return mplayer_play(filename);
}

static int VOICE_queue(int idx)
{
    char filename[32];
    sprintf(filename, "%s%02X" EXT_VOICE, voice_sel->folder, idx);

    int retval = mplayer_playlist_queue(filename);

    // if (0 == retval)
    //     mplayer_playlist_queue_intv(voice_sel->tempo);
    return retval;
}

static void LOCALE_set(struct VOICE_t const *locale)
{
    if (NULL != locale && voice_sel != locale)
        voice_sel = locale;
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
    for (int idx = 0; idx < (int)lengthof(__voices); idx ++)
    {
        struct VOICE_t const *locale = &__voices[idx];
        callback(idx, locale->lcid, locale->default_dfmt, locale->default_hfmt, locale->voice, arg, idx == lengthof(__voices) - 1);
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
    voice_id = (unsigned)voice_id < lengthof(__voices) ? voice_id : (int16_t)(voice_sel - __voices);

    if ((unsigned)voice_id < lengthof(__voices))
    {
        struct VOICE_t const *voice = &__voices[voice_id];

        char filename[64];
        sprintf(filename, "%s%02X" EXT_VOICE, voice->folder, IDX_SETTING_LANG);
        int fd = open(filename, O_RDONLY);

        if (-1 != fd)
        {
            close(fd);
            voice_sel = &__voices[voice_id];
        }
        else
            voice_id = 0;
    }
    else
        voice_id = 0;

    LOCALE_set(&__voices[voice_id]);
    return voice_id;
}

int16_t VOICE_select_lcid(char const *lcid)
{
    struct VOICE_t const *fallback_locale = voice_sel;
    struct VOICE_t const *lang_match = NULL;

    for (int16_t idx = 0; idx < (int)lengthof(__voices); idx ++)
    {
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
    if (0 <= voice_sel - __voices)
    {
        for (int16_t idx = (int16_t)(voice_sel - __voices + 1); idx < (int)lengthof(__voices); idx ++)
        {
            struct VOICE_t const *voice = &__voices[idx];

            char filename[64];
            sprintf(filename, "%s%02X" EXT_VOICE, voice->folder, IDX_SETTING_LANG);
            int fd = open(filename, O_RDONLY);

            if (-1 != fd)
            {
                close(fd);

                LOCALE_set(voice);
                return idx;
            }
            else
                continue;
        }
    }

    LOCALE_set(&__voices[0]);
    return 0;
}

int VOICE_say_date(struct tm const *tm)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    int retval = VOICE_queue(IDX_TODAY);

    int year_vidx;
    {
        int full_year = tm->tm_year + 1900;
        full_year = YEAR_ROUND_LO > full_year ? YEAR_ROUND_LO :
            (YEAR_ROUND_HI < full_year ?        YEAR_ROUND_HI : full_year);

        year_vidx = full_year - YEAR_ROUND_LO + IDX_YEAR_LO;
    }
    int month_vidx = tm->tm_mon + IDX_JANURAY;
    int day_vidx = tm->tm_mday - 1 + IDX_MDAY_1;
    int wday_vidx = tm->tm_wday + IDX_SUNDAY;

    if (0 == retval && WFMT_LEAD == voice_sel->wfmt)
        retval = VOICE_queue(wday_vidx);

    enum LOCALE_dfmt_t dfmt = locale_ptr->dfmt;
    if (DFMT_DEFAULT == dfmt)
        dfmt = voice_sel->default_dfmt;

    switch (dfmt)
    {
    case DFMT_DEFAULT:
    case DFMT_YYMMDD:
        if (0 == retval)
            retval = VOICE_queue(year_vidx);
        if (0 == retval)
            retval = VOICE_queue(month_vidx);
        if (0 == retval)
            retval = VOICE_queue(day_vidx);
        break;

    case DFMT_DDMMYY:
        if (0 == retval)
            retval = VOICE_queue(day_vidx);
        if (0 == retval)
            retval = VOICE_queue(month_vidx);
        if (0 == retval)
            retval = VOICE_queue(year_vidx);
        break;

    case DFMT_MMDDYY:
        if (0 == retval)
            retval = VOICE_queue(month_vidx);
        if (0 == retval)
            retval = VOICE_queue(day_vidx);
        if (0 == retval)
            retval = VOICE_queue(year_vidx);
        break;
    };

    if (0 == retval && WFMT_TAIL == voice_sel->wfmt)
        retval = VOICE_queue(wday_vidx);

    if (0 == retval && -1 != voice_sel->tail_idx)
        retval = VOICE_queue(voice_sel->tail_idx);
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

    // mplayer_playlist_clear();

    LOG_info("%04d/%02d/%02d %02d:%02d:%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    int retval = 0;
    int saying_hour;

    enum LOCALE_hfmt_t hfmt = locale_ptr->hfmt;
    if (HFMT_DEFAULT == hfmt)
        hfmt = voice_sel->default_hfmt;

    // 0/12 点整
    if (0 == tm->tm_min && 0 == tm->tm_hour % 12)
    {
        if (0 == retval)
            retval = VOICE_queue(IDX_NOW);

        if (0 == retval)
        {
            if (0 == tm->tm_hour)
                retval = VOICE_queue(IDX_MID_NIGHT);
            else
                retval = VOICE_queue(IDX_NOON);
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
            retval = VOICE_queue(IDX_NOW);
        if (0 == retval)
        {
            if (0 == saying_hour || 12 == saying_hour)
                retval = VOICE_queue(12 + IDX_HOUR_0);
            else
                retval = VOICE_queue(saying_hour % 12 + IDX_HOUR_0);
        }
        if (0 == retval)
            retval = VOICE_queue(tm->tm_min + IDX_MINUTE_0);
        if (0 == retval)
        {
            if (tm->tm_hour < 12)
                retval = VOICE_queue(IDX_IN_MORNING);
            else if (tm->tm_hour < 18)
                retval = VOICE_queue(IDX_IN_AFTERNOON);
            else
                retval = VOICE_queue(IDX_IN_EVENING);
        }
    }
    else
    {
    fixed_gr24:
        if (0 == retval)
        {
            if (tm->tm_hour < 12)
                retval = VOICE_queue(IDX_GR_MORNING);
            else if (tm->tm_hour < 18)
                retval = VOICE_queue(IDX_GR_AFTERNOON);
            else
                retval = VOICE_queue(IDX_GR_EVENING);
        }
        if (0 == retval)
            retval = VOICE_queue(IDX_NOW);
        if (0 == retval)
            retval = VOICE_queue(saying_hour + IDX_HOUR_0);
        if (0 == retval)
            retval = VOICE_queue(tm->tm_min + IDX_MINUTE_0);
    }

    if (-1 != voice_sel->tail_idx)
        retval = VOICE_queue(voice_sel->tail_idx);

    return retval;
}

int VOICE_say_time_epoch(time_t epoch)
{
    return VOICE_say_time(localtime(&epoch));
}

int VOICE_say_setting(enum VOICE_setting_part_t setting, void *arg)
{
    if (setting == VOICE_SETTING_DONE)
        return VOICE_play(IDX_SETTING_DONE);

    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    switch (setting)
    {
    case VOICE_SETTING_DONE:
        break;

    case VOICE_SETTING_LANG:
        return VOICE_play(IDX_SETTING_LANG);

    case VOICE_SETTING_HOUR:
        return VOICE_play(IDX_SETTING_HOUR);

    case VOICE_SETTING_MINUTE:
        return VOICE_play(IDX_SETTING_MINUTE);

    case VOICE_SETTING_YEAR:
        return VOICE_play(IDX_SETTING_YEAR);

    case VOICE_SETTING_MONTH:
        return VOICE_play(IDX_SETTING_MONTH);

    case VOICE_SETTING_MDAY:
        return VOICE_play(IDX_SETTING_MDAY);

    case VOICE_SETTING_ALARM_HOUR:
        return VOICE_play(IDX_SETTING_ALARM_HOUR);

    case VOICE_SETTING_ALARM_MIN:
        return VOICE_play(IDX_SETTING_ALARM_MIN);

    case VOICE_SETTING_ALARM_RINGTONE:
        return VOICE_play_ringtone((int)arg);

    case VOICE_SETTING_EXT_LOW_BATT:
        return VOICE_play(IDX_SETTING_EXT_LOW_BATT);

    case VOICE_SETTING_EXT_ALARM_ON:
        return VOICE_play(IDX_SETTING_EXT_ALARM_ON);

    case VOICE_SETTING_EXT_ALARM_OFF:
        return VOICE_play(IDX_SETTING_EXT_ALARM_OFF);
    };

    return EINVAL;
}

int VOICE_say_setting_part(enum VOICE_setting_part_t setting,
    struct tm const *tm, void *arg)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    int retval = 0;

    switch (setting)
    {
    case VOICE_SETTING_LANG:
        if (0 == retval)
            retval = VOICE_queue(IDX_SETTING_VOICE);
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
                        retval = VOICE_queue(12 + IDX_HOUR_0);
                    else
                        retval = VOICE_queue(tm->tm_hour % 12 + IDX_HOUR_0);
                }

                if (0 == retval)
                {
                    if (tm->tm_hour < 12)
                        retval = VOICE_queue(IDX_IN_MORNING);
                    else if (tm->tm_hour < 18)
                        retval = VOICE_queue(IDX_IN_AFTERNOON);
                    else
                        retval = VOICE_queue(IDX_IN_EVENING);
                }

                if (HFMT_24 == voice_sel->fixed_gr)
                {
                    if (0 == tm->tm_hour || 12 == tm->tm_hour)
                        retval = VOICE_queue(12 + IDX_HOUR_0);
                    else
                        retval = VOICE_queue(tm->tm_hour % 12 + IDX_HOUR_0);
                }
            }
            else
                retval = VOICE_queue(tm->tm_hour + IDX_HOUR_0);
        }
        break;

    case VOICE_SETTING_MINUTE:
    case VOICE_SETTING_ALARM_MIN:
        if (0 == retval)
            retval = VOICE_queue(tm->tm_min + IDX_MINUTE_0);
        break;

    case VOICE_SETTING_YEAR:
        if (0 == retval)
        {
            int idx = tm->tm_year + 1900;
            idx = (YEAR_ROUND_LO > idx ? YEAR_ROUND_LO : (YEAR_ROUND_HI < idx ? YEAR_ROUND_HI : idx)) -
                YEAR_ROUND_LO + IDX_YEAR_LO;

            retval = VOICE_queue(idx);
        }
        break;

    case VOICE_SETTING_MDAY:
        if (0 == retval)
            retval = VOICE_queue(tm->tm_mday - 1 + IDX_MDAY_1);
        if (0 == retval && voice_sel->default_dfmt != DFMT_YYMMDD)
            goto fallthrough_month;
        break;

    fallthrough_month:
    case VOICE_SETTING_MONTH:
        if (0 == retval)
            retval = VOICE_queue(tm->tm_mon + IDX_JANURAY);
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
    return (ringtone_id + RING_TONE_COUNT - 1) % RING_TONE_COUNT;
}

int VOICE_play_ringtone(int ringtone_id)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    if (0xFF > ringtone_id)
        return VOICE_play(IDX_RING_TONE_0 + ringtone_id);
    else
        return VOICE_play(ringtone_id);
}

int VOICE_play_reminder(int reminder_id)
{
    if (NULL == voice_sel)
        return EMODU_NOT_CONFIGURED;

    if (0xFF > reminder_id)
        return VOICE_play(IDX_REMINDER_0 + reminder_id);
    else
        return VOICE_play(reminder_id);
}
