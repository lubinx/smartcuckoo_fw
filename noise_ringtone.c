#include <ultracore/nvm.h>
#include <audio/mynoise.h>
#include <sys/errno.h>
#include <sh/cmdline.h>

#include <audio/renderer.h>
#include <audio/mplayer.h>

#include <stdlib.h>
#include <string.h>

#include "clock.h"

/******************************************************************************
 *  @def
 *****************************************************************************/
#define NOISE_RINGTONE_NVM_ID           NVM_DEFINE_KEY('0', 'N', 'R', 'T')

struct noise_ringtone_t
{
    char noise[MYNOISE_FOLDER_MAX + MYNOISE_THEME_MAX];
    uint32_t off_seconds;
};

struct noise_ringtone_nvm_t
{
    struct noise_ringtone_t item[NVM_MAX_OBJECT_SIZE / sizeof(struct noise_ringtone_t)];
};

/******************************************************************************
 *  @implements
 *****************************************************************************/
void CLOCK_start_app_ringtone_cb(uint8_t alarm_idx)
{
    struct noise_ringtone_nvm_t *nvm_ptr;
    char scenario[MYNOISE_FOLDER_MAX];

    mplayer_stop();
    AUDIO_renderer_stop();

    unsigned nvm_id = NOISE_RINGTONE_NVM_ID + (alarm_idx - 10) / lengthof(nvm_ptr->item);
    unsigned idx = (alarm_idx - 10) % lengthof(nvm_ptr->item);

    nvm_ptr = NVM_get_ptr(nvm_id, sizeof(*nvm_ptr));
    if (NULL != nvm_ptr)
    {
        char *theme = NULL;
        struct noise_ringtone_t *ringtone = &nvm_ptr->item[idx];
        if (1)
        {
            char *ptr = ringtone->noise;
            while (*ptr && '.' != *ptr) ptr ++;

            if ('.' == *ptr)
            {
                memset(scenario, 0, sizeof(scenario));

                strncpy(scenario, ringtone->noise, MIN(sizeof(scenario), (unsigned)(ptr -ringtone->noise)));
                theme = ptr + 1;
            }
            else
                strncpy(scenario, ringtone->noise, sizeof(scenario) - 1);
        }

        int err = MYNOISE_start_scenario(scenario, theme);
        if (0 != err)   // REVIEW: fallback
            err = MYNOISE_start();

        if (0 == err)
        {
            MYNOISE_no_store_stat();

            if (0 != ringtone->off_seconds)
                MYNOISE_power_off_seconds(ringtone->off_seconds);
        }
    }
}

void CLOCK_stop_app_ringtone_cb(uint8_t alarm_idx)
{
    ARG_UNUSED(alarm_idx);
    MYNOISE_stop();
}

char const *CLOCK_get_app_ringtone_cb(uint8_t alarm_idx)
{
    struct noise_ringtone_nvm_t *nvm_ptr;

    unsigned nvm_id = NOISE_RINGTONE_NVM_ID + (alarm_idx - 10) / lengthof(nvm_ptr->item);
    unsigned idx = (alarm_idx - 10) % lengthof(nvm_ptr->item);

    nvm_ptr = NVM_get_ptr(nvm_id, sizeof(struct noise_ringtone_nvm_t));
    if (NULL != nvm_ptr)
    {
        struct noise_ringtone_t *ptr = &nvm_ptr->item[idx];
        char *retval = malloc(MYNOISE_FOLDER_MAX + MYNOISE_THEME_MAX + 16);

        sprintf(retval, "\"%s off_seconds=%u\"", ptr->noise, (unsigned)ptr->off_seconds);
        return retval;
    }
    else
        return NULL;
}

int CLOCK_set_app_ringtone_cb(uint8_t alarm_idx, char *str)
{
    char *argv[2];
    int argc = CMD_parse(str, lengthof(argv), argv);
    if (0 == argc)
        return EINVAL;

    struct noise_ringtone_nvm_t *nvm_ptr = malloc(sizeof(*nvm_ptr));
    if (NULL == nvm_ptr)
        return ENOMEM;

    unsigned nvm_id = NOISE_RINGTONE_NVM_ID + (alarm_idx - 10) / lengthof(nvm_ptr->item);
    unsigned idx = (alarm_idx - 10) % lengthof(nvm_ptr->item);

    if (0 != NVM_get(nvm_id, sizeof(struct noise_ringtone_nvm_t), nvm_ptr))
        memset(nvm_ptr, 0, sizeof(*nvm_ptr));

    struct noise_ringtone_t *ptr = &nvm_ptr->item[idx];
    strncpy(ptr->noise, argv[0], sizeof(ptr->noise) - 1);

    char *off_seconds_str = CMD_paramvalue_byname("off_seconds", argc, argv);
    if (NULL != off_seconds_str)
        ptr->off_seconds = strtoul(off_seconds_str, NULL, 10);
    else
        ptr->off_seconds = 0;

    return NVM_set(nvm_id, sizeof(*nvm_ptr), nvm_ptr);
}
