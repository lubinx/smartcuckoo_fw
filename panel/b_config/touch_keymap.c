#include "panel_private.h"

struct TOUCH_keymap_t const __touch_keymap[] =
{
    {.key = 0x0010,     .msg_key = MSG_BUTTON_MEDIA},
    {.key = 0x0020,     .msg_key = MSG_BUTTON_RIGHT},
    {.key = 0x0040,     .msg_key = MSG_BUTTON_OK},
    {.key = 0x0080,     .msg_key = MSG_BUTTON_LEFT},
    {.key = 0x0100,     .msg_key = MSG_BUTTON_RECORD},
    {.key = 0x0200,     .msg_key = MSG_BUTTON_ALM2},
    {.key = 0x0400,     .msg_key = MSG_BUTTON_ALM1},
    {.key = 0x0800,     .msg_key = MSG_BUTTON_SETTING},
    {.key = 0x1000,     .msg_key = MSG_BUTTON_SNOOZE},
    {.key = 0}
};
