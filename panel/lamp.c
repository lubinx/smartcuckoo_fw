#include "panel_private.h"

#define RGB(R, G, B)                    ((uint32_t)(G) << 16U | (uint32_t)(R) << 8 | (uint32_t)(B))

/****************************************************************************
 *  @internal
 ****************************************************************************/
// shell
static int SHELL_lamp(struct UCSH_env *env);

static uint32_t __color_xlat[] =
{
    RGB(142, 142, 142),
    RGB(60, 255, 60),
    // RGB(40, 255, 80),
    RGB(60, 155, 155),
    // RGB(40, 80, 255),
    RGB(60, 60, 255),
    // RGB(80, 40, 255),
    RGB(155, 60, 155),
    // RGB(255, 40, 80),
    RGB(255, 60, 60),
    // RGB(255, 80, 40),
    RGB(155, 155, 60),
    // RGB(80, 255, 40),
};

static void WS2812B_set_24bit(uint32_t grb);
static void WS2812B_set(uint32_t color);
static uint32_t color_trans(uint32_t color, int8_t percent);

/****************************************************************************
 *  @public
 ****************************************************************************/
void LAMP_attr_init(struct LAMP_attr_t *attr)
{
    attr->color_idx = 0;
    WS2812B_set(0);

    UCSH_register("lamp", SHELL_lamp);
}

void LAMP_enum_colors(LAMP_color_callback_t callback, void *arg)
{
    for (size_t i = 0; i < lengthof(__color_xlat); i ++)
    {
        uint32_t color = __color_xlat[i];

        uint8_t R = (0x00FF00 & color) >> 8;
        uint8_t G = (0xFF0000 & color) >> 16;
        uint8_t B = (0x0000FF & color);

        callback(i, R, G, B, arg, i == lengthof(__color_xlat) - 1);
    }
}

void LAMP_toggle(struct LAMP_attr_t *attr)
{
    attr->en = ! attr->en;

    if (attr->en)
    {
        uint32_t color = __color_xlat[attr->color_idx];
        WS2812B_set(color_trans(color, attr->weight));
    }
    else
        WS2812B_set(0);
}

void LAMP_on(struct LAMP_attr_t *attr)
{
    attr->en = true;
    uint32_t color = __color_xlat[attr->color_idx];
    WS2812B_set(color_trans(color, attr->weight));
}

void LAMP_off(struct LAMP_attr_t *attr)
{
    attr->en = false;
    WS2812B_set(0);
}

void LAMP_inc(struct LAMP_attr_t *attr)
{
    if (attr->en)
    {
        uint32_t color = __color_xlat[attr->color_idx];
        attr->weight += 16;
        if (80 < attr->weight) attr->weight = 80;

        WS2812B_set(color_trans(color, attr->weight));
    }
}

void LAMP_dec(struct LAMP_attr_t *attr)
{
    if (attr->en)
    {
        uint32_t color = __color_xlat[attr->color_idx];
        attr->weight -= 16;
        if (-80 > attr->weight) attr->weight = -80;

        WS2812B_set(color_trans(color, attr->weight));
    }
}

void LAMP_next_color(struct LAMP_attr_t *attr)
{
    if (attr->en)
    {
        if (attr->color_idx < lengthof(__color_xlat))
            attr->color_idx ++;
        else
            attr->color_idx = 0;

        uint32_t color = __color_xlat[attr->color_idx];
        WS2812B_set(color_trans(color, attr->weight));
    }
}

void LAMP_update(struct LAMP_attr_t *attr, unsigned idx, unsigned percent)
{
    if (idx < lengthof(__color_xlat))
        attr->color_idx = idx;
    else
        attr->color_idx = 0;

    if (percent != 0)
    {
        if (percent > 100) percent = 60;
        if (percent < 20) percent = 20;

        if (60 == percent)
            attr->weight = 0;
        else
            attr->weight = (int8_t)(160 * (percent - 20) / 80 - 80);

        LAMP_on(attr);
    }
    else
        LAMP_off(attr);
}

/****************************************************************************
 *  @internal: shell
 ****************************************************************************/
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
        unsigned percent = strtoul(env->argv[2], NULL, 10);

        LAMP_update(&panel.lamp_attr, idx, percent);
        UCSH_puts(env, "\n");
        return 0;
    }
    else
        return EINVAL;
}

/****************************************************************************
 *  @internal
 ****************************************************************************/
__attribute__((optimize("Ofast")))
static void WS2812B_set_24bit(uint32_t grb)
{
    static uint32_t volatile *DOUT_SET = &GPIO->P_SET[0].DOUT;
    static uint32_t volatile *DOUT_CLR = &GPIO->P_CLR[0].DOUT;

    for(uint8_t zz = 0; zz < 24; zz ++)
    {
        if (0x800000 & (grb << zz))
        {
            *DOUT_SET = PIN_LAMP;
            usleep(2);

            *DOUT_CLR = PIN_LAMP;
            __NOP(); __NOP();
        }
        else
        {
            *DOUT_SET = PIN_LAMP;
            __NOP(); __NOP();

            *DOUT_CLR = PIN_LAMP;
            usleep(2);
        }
    }
}

__attribute__((optimize("Ofast")))
static void WS2812B_set(uint32_t color)
{
    __disable_irq();

    for (int i = 0; i < 12; i ++)
        WS2812B_set_24bit(color);

    __enable_irq();
}

static uint32_t color_trans(uint32_t color, int8_t percent)
{
    if (0 == percent)
        return color;

    int R = (0x00FF00 & color) >> 8;
    int G = (0xFF0000 & color) >> 16;
    int B = (0x0000FF & color);

    R = R * (100 + percent) / 100;
    G = G * (100 + percent) / 100;
    B = B * (100 + percent) / 100;

    R = R < 0 ? 0 : (R > 255 ? 255 : R);
    G = G < 0 ? 0 : (G > 255 ? 255 : G);
    B = B < 0 ? 0 : (B > 255 ? 255 : B);

    return RGB(R, G, B);
}
