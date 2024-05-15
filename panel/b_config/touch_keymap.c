#include "panel_private.h"

struct TOUCH_keymap_t const __touch_keymap[] =
{
    {.key = 0x0010,     .msg_key = MSG_KEY_USB},
    {.key = 0x0020,     .msg_key = MSG_KEY_RIGHT},
    {.key = 0x0040,     .msg_key = MSG_KEY_RECORD},
    {.key = 0x0080,     .msg_key = MSG_KEY_LEFT},
    {.key = 0x0100,     .msg_key = MSG_KEY_WHITE_NOISE},
    {.key = 0x0200,     .msg_key = MSG_KEY_ALM2},
    {.key = 0x0400,     .msg_key = MSG_KEY_ALM1},
    {.key = 0x0800,     .msg_key = MSG_KEY_SETTING},
    {.key = 0x1000,     .msg_key = MSG_KEY_OK},
    {.key = 0}
};
