#include "panel_private.h"


/****************************************************************************
 * @private
 ****************************************************************************/
static int SHELL_env_sensor(struct UCSH_env *env);
static int SHELL_dim(struct UCSH_env *env);

#ifdef PIN_LAMP
    static int SHELL_lamp(struct UCSH_env *env);
#endif

/****************************************************************************
 * @implements
 ****************************************************************************/
void PERIPHERAL_shell_init(void)
{
    UCSH_register("env", SHELL_env_sensor);
    UCSH_register("dim", SHELL_dim);

#ifdef PIN_LAMP
    UCSH_register("lamp", SHELL_lamp);
#endif
}

/****************************************************************************
 * @private
 ****************************************************************************/
static int SHELL_env_sensor(struct UCSH_env *env)
{
    enum TMPR_unit_t unit = setting.locale.tmpr_unit;

    if (2 == env->argc)
    {
        if (0 == strncasecmp("CELSIUS", env->argv[1], 3))
            unit = CELSIUS;
        else if (0 == strncasecmp("FAHRENHEIT", env->argv[1], 3))
            unit = FAHRENHEIT;

        if (unit != setting.locale.tmpr_unit)
        {
            setting.locale.tmpr_unit = unit;

            panel.setting.is_modified = true;
            SETTING_defer_save(&panel);
        }
    }
    int16_t tmpr;

    if (FAHRENHEIT == unit)
        tmpr = TMPR_fahrenheit(panel.env_sensor.tmpr);
    else
        tmpr = panel.env_sensor.tmpr;

    UCSH_printf(env, "tmpr=%d.%d ", tmpr / 10, abs(tmpr) % 10);
    if (FAHRENHEIT == unit)
        UCSH_puts(env, "°F\n");
    else
        UCSH_puts(env, "°C\n");

    UCSH_printf(env, "humidity=%d\n", panel.env_sensor.humidity);
    return 0;
}

static int SHELL_dim(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        unsigned dim = strtoul(env->argv[1], NULL, 10);
        dim = dim > 100 ? 100 : dim;

        if (setting.dim != dim)
        {
            setting.dim = (uint8_t)dim;
            PANEL_set_dim(&panel.panel_attr, setting.dim, panel.light_sens.percent);
        }
    }

    UCSH_printf(env, "dim=%d\n", setting.dim);
    return 0;
}

#ifdef PIN_LAMP
    static void lamp_color_enum_callback(unsigned id, uint8_t R, uint8_t G, uint8_t B, void *arg, bool final)
    {
        UCSH_printf((struct UCSH_env *)arg, "\t{\"id\":%d, ", id);
        UCSH_printf((struct UCSH_env *)arg, "\"R\":%d, \"G\":%d, \"B\":%d}", R, G, B);

        if (final)
            UCSH_puts((struct UCSH_env *)arg, "\n");
        else
            UCSH_puts((struct UCSH_env *)arg, ",\n");
    }

    static int SHELL_lamp(struct UCSH_env *env)
    {
        if (1 == env->argc)
        {
            UCSH_puts(env, "{\"colors\": [\n");
            LAMP_enum_colors(lamp_color_enum_callback, env);
            UCSH_puts(env, "]}\n");

            return 0;
        }
        else if (2 == env->argc)
        {
            if (0 == strcmp("on", env->argv[1]))
                LAMP_on(&panel.lamp_attr);
            else
                LAMP_off(&panel.lamp_attr);

            UCSH_puts(env, "\n");
            return 0;
        }
        else if (3 == env->argc)
        {
            unsigned idx = strtoul(env->argv[1], NULL, 10);
            LAMP_set_color(&panel.lamp_attr, idx);

            unsigned percent = strtoul(env->argv[2], NULL, 10);
            if (0 != percent)
            {
                LAMP_set_brightness(&panel.lamp_attr, percent);
                LAMP_on(&panel.lamp_attr);
            }
            else
                LAMP_off(&panel.lamp_attr);

            UCSH_puts(env, "\n");
            return 0;
        }
        else
            return EINVAL;
    }
#endif
