#include <ultracore/nvm.h>
#include <audio/mynoise.h>

#include <inttypes.h>
#include <stdlib.h>
#include <sys/errno.h>

#include <sh/ucsh.h>

#include "clock.h"
#include "voice.h"
#include "noise_apwr.h"

/****************************************************************************
 *  @def
 ****************************************************************************/
#define ZONE_APWR_NVM_ID                NVM_DEFINE_KEY('A', 'P', 'W', 'R')

struct AUTO_power_attr_t
{
    char noise[96];
    uint32_t off_seconds;
};
static_assert(sizeof(struct AUTO_power_attr_t) < NVM_MAX_OBJECT_SIZE, "");

/****************************************************************************
 *  @implements
 ****************************************************************************/
static void APWR_callback(void);
static int APWR_shell(struct UCSH_env *env);

void APWR_init(void)
{
    CLOCK_app_specify_callback(APWR_callback);
    UCSH_REGISTER("apwr", APWR_shell);
}

/****************************************************************************
 *  @private: auto power & shell
 ****************************************************************************/
static void APWR_callback(void)
{
    char scenario[MYNOISE_FOLDER_MAX];

    struct AUTO_power_attr_t *nvm_ptr = NVM_get_ptr(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr));
    if (NULL != nvm_ptr)
    {
        char *theme = NULL;
        if (1)
        {
            char *ptr = nvm_ptr->noise;
            while (*ptr && '.' != *ptr) ptr ++;

            if ('.' == *ptr)
            {
                memset(scenario, 0, sizeof(scenario));

                strncpy(scenario, nvm_ptr->noise,  MIN(sizeof(scenario), (unsigned)(ptr -nvm_ptr->noise)));
                theme = ptr + 1;
            }
            else
                strncpy(scenario, nvm_ptr->noise, sizeof(scenario));
        }

        int err = MYNOISE_start_scenario(scenario, theme);
        if (0 != err)   // REVIEW: fallback
            err = MYNOISE_start();

        if (0 == err)
        {
            MYNOISE_no_store_stat();

            if (0 != nvm_ptr->off_seconds)
                MYNOISE_power_off_seconds(nvm_ptr->off_seconds);
        }
    }
}

static int APWR_shell(struct UCSH_env *env)
{
    struct AUTO_power_attr_t *nvm_ptr = NVM_get_ptr(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr));
    int err = 0;

    if (1 == env->argc)
    {
        int pos = 0;
        pos += sprintf(env->buf + pos, "{");

        struct CLOCK_moment_t const *moment = CLOCK_get_app_specify_moment();
        if (1)
        {
            pos += sprintf(env->buf + pos, "\n\t\"enabled\": %s", moment->enabled ? "true" : "false");
            pos += sprintf(env->buf + pos, ",\n\t\"mtime\" :%d", moment->mtime);
            pos += sprintf(env->buf + pos, ",\n\t\"wdays\": %d", moment->wdays);
            pos += sprintf(env->buf + pos, ",\n\t\"mdate\": %" PRIu32, moment->mdate);
            pos += sprintf(env->buf + pos, ",\n\t\"noise\": \"%s\"", NULL == nvm_ptr ? "" : nvm_ptr->noise);
            pos += sprintf(env->buf + pos, ",\n\t\"off_seconds\": %" PRIu32, NULL == nvm_ptr ? 0 : nvm_ptr->off_seconds);
        }

        pos += sprintf(env->buf + pos, "\n}\n");
        writebuf(env->fd, env->buf, (unsigned)pos);
    }
    else
    {
        nvm_ptr = malloc(sizeof(*nvm_ptr));
        if (NULL == nvm_ptr)
            return ENOMEM;

        memset(nvm_ptr, 0, sizeof(*nvm_ptr));
        NVM_get(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr), nvm_ptr);

        struct CLOCK_moment_t moment;
        if (1)
        {
            struct CLOCK_moment_t const *ptr = CLOCK_get_app_specify_moment();
            if (NULL != ptr)
                moment = *ptr;
            else
                memset(&moment, 0, sizeof(moment));
        }
        /*
            apwr enable / disable
        */
        if (2 == env->argc)
        {
            if (0 == strcasecmp("off", env->argv[1]))
                moment.enabled = false;
            else if (0 == strcasecmp("on", env->argv[1]))
                moment.enabled = true;
            else
                err = EINVAL;
        }
        /*
            apwr "noise" mtime [wdays=<mask> / mdate=<mdate>] [off_seconds=<seconds>]
        */
        else
        {
            strncpy(nvm_ptr->noise, env->argv[1], sizeof(nvm_ptr->noise));
            moment.enabled = true;

            if (1)
            {
                int mtime = strtol(env->argv[2], NULL, 10);
                if (60 <= mtime % 100 || 24 <= mtime / 100)     // 0000 ~ 2359
                    err = EINVAL;
                else
                    moment.mtime = (int16_t)mtime;
            }

            if (0 == err)
            {
                char *str = CMD_paramvalue_byname("wdays", env->argc, env->argv);
                if (str)
                {
                    int wdays = strtol(str, NULL, 10);
                    if (0 == wdays)
                        wdays = strtol(str, NULL, 16);
                    if (0x7F < wdays)
                        err = EINVAL;

                    moment.wdays = (int8_t)wdays;
                }
                else
                    moment.wdays = 0x7F;
            }
            if (0 == err)
            {
                char *str = CMD_paramvalue_byname("off_seconds", env->argc, env->argv);
                if (str)
                    nvm_ptr->off_seconds = strtoul(str, NULL, 10);
                else
                    nvm_ptr->off_seconds = 0;
            }

            if (0 == err)
            {
                char *str = CMD_paramvalue_byname("mdate", env->argc, env->argv);
                if (str)
                {
                    moment.mdate = strtol(str, NULL, 10);
                    moment.wdays = 0;
                }
                else
                    moment.mdate = 0;
            }
            // REVIEW: least one of alarm date or week days masks must set
            if (0 == moment.wdays && 0 == moment.wdays)
                err = EINVAL;

            if (0 == strlen(nvm_ptr->noise))
                err = EINVAL;
        }

        if (0 == err)
        {
            if (0 == (err = NVM_set(ZONE_APWR_NVM_ID, sizeof(*nvm_ptr), nvm_ptr)))
            {
                CLOCK_store_app_specify_moment(&moment);
                VOICE_say_setting(VOICE_SETTING_DONE);
            }
        }

        free(nvm_ptr);
    }
    return err;
}
