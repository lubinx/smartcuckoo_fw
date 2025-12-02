#include <stdint.h>

uint32_t SMART_LED_HAL_color_xform(uint32_t rgb)
{
    // NOTE: WS2812B is GRB not RGB
    return ((0xFF0000 & rgb) >> 8) | ((0x00FF00 & rgb) << 8) | (0x0000FF & rgb);
}
