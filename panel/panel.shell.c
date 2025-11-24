#include "panel_private.h"

/****************************************************************************
 * @private
 ****************************************************************************/
static int SHELL_env_sensor(struct UCSH_env *env);
static int SHELL_dim(struct UCSH_env *env);

/****************************************************************************
 * @implements
 ****************************************************************************/
void PANEL_shell_register(void)
{
    UCSH_register("env", SHELL_env_sensor);
    UCSH_register("dim", SHELL_dim);
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
