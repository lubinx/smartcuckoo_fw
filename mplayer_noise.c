#include "smartcuckoo.h"

/****************************************************************************
 * @def
 ****************************************************************************/
    // #define NVM_SETTING                 NVM_DEFINE_KEY('S', 'E', 'T', 'T')

struct NOISE_mapping_t
{
    char const *id;
    char const *name;
};

struct NOISE_attr_t
{
    bool playing;
    uint16_t curr_id;
};

struct NOISE_nvm_t
{

};

/****************************************************************************
 * @internal
 ****************************************************************************/
static int SHELL_noise(struct UCSH_env *env);

// var
static struct NOISE_attr_t context = {0};

static char const *__noise_mapping[] =
{
    "AirconFan",
    "Cabin",
    "CampFire",
    "CityStreet",
    "Courtyard",
    "Forest",
    "Highway",
    "Raindrops",
    "SummerNight",
    "Thunder",
    "WhiteNoise",
    "Wind",
};

/****************************************************************************
 * @implements
 ****************************************************************************/
int NOISE_attr_init(uint16_t stored_id)
{
    context.curr_id = stored_id;

    UCSH_register("nois", SHELL_noise);
    return 0;
}

void NOISE_enum_themes(NOISE_theme_callback_t callback, void *arg)
{
    for (uint16_t i = 0; i < lengthof(__noise_mapping); i ++)
        callback(i, __noise_mapping[i], arg, i == lengthof(__noise_mapping) - 1);
}

bool NOISE_is_playing(void)
{
    return context.playing;
}

void NOISE_set_stopped(void)
{
    context.playing = false;
}

static int __play(void)
{
    mplayer_stop();
    context.playing = true;

    if (0 > (int16_t)context.curr_id)
        context.curr_id = lengthof(__noise_mapping) - 1;
    else if ((int)lengthof(__noise_mapping) <= context.curr_id)
        context.curr_id = 0;

    char filename[40];
    sprintf(filename, "noise/%s", __noise_mapping[context.curr_id]);
    return mplayer_play_loop(filename);
}

int NOISE_toggle(void)
{
    context.playing = ! context.playing;

    if (context.playing)
        return __play();
    else
        return mplayer_stop();
}

int NOISE_play(uint16_t id)
{
    context.curr_id = id;
    return __play();
}

int NOISE_stop(void)
{
    context.playing = false;
    return mplayer_stop();
}

int NOISE_next(void)
{
    context.curr_id ++;

    if (context.playing)
        return __play();
    else
        return 0;
}

int NOISE_prev(void)
{
    context.curr_id --;

    if (context.playing)
        return __play();
    else
        return 0;
}

/****************************************************************************
 * @internal
 ****************************************************************************/
static void noise_theme_enum_callback(uint16_t id, char const *theme, void *arg, bool final)
{
    UCSH_printf((struct UCSH_env *)arg, "\t{\"id\":%d, ", id);
    UCSH_printf((struct UCSH_env *)arg, "\"theme\":\"%s\"}", theme);

    if (final)
        UCSH_puts((struct UCSH_env *)arg, "\n");
    else
        UCSH_puts((struct UCSH_env *)arg, ",\n");
}

static int SHELL_noise(struct UCSH_env *env)
{
    if (1 == env->argc)
    {
        UCSH_puts(env, "{\"themes\": [\n");
        NOISE_enum_themes(noise_theme_enum_callback, env);
        UCSH_puts(env, "]}\n");

        return 0;
    }
    else if (2 == env->argc)
    {
        if (0 == strcasecmp("ON", env->argv[1]))
        {
            if (! NOISE_is_playing())
                NOISE_toggle();
        }
        else if (0 == strcasecmp("OFF", env->argv[1]))
        {
            NOISE_stop();
        }
        else if (0 == strcasecmp("NEXT", env->argv[1]))
        {
            NOISE_next();
        }
        else if (0 == strcasecmp("PREV", env->argv[1]))
        {
            NOISE_prev();
        }
        else
        {
            unsigned id = strtoul(env->argv[1], NULL, 10);
            NOISE_play((uint16_t)id);
        }

        UCSH_puts(env, "\n");
        return 0;
    }
    else
        return EINVAL;
}
