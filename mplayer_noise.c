#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mplayer.h"
#include "mplayer_noise.h"

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

int NOISE_attr_init(struct NOISE_attr_t *attr, int stored_idx)
{
    memset(attr, 0, sizeof(*attr));
    attr->curr_id = stored_idx;
    return 0;
}

bool NOISE_is_playing(struct NOISE_attr_t *attr)
{
    return attr->playing;
}

void NOISE_set_stopped(struct NOISE_attr_t *attr)
{
    attr->playing = false;
}

static int __play(struct NOISE_attr_t *attr)
{
    mplayer_stop();
    attr->playing = true;

    if (0 > attr->curr_id)
        attr->curr_id = lengthof(__noise_mapping) - 1;
    else if ((int)lengthof(__noise_mapping) <= attr->curr_id)
        attr->curr_id = 0;

    char filename[40];
    sprintf(filename, "noise/%s", __noise_mapping[attr->curr_id]);
    return mplayer_play_loop(filename);
}

int NOISE_toggle(struct NOISE_attr_t *attr)
{
    attr->playing = ! attr->playing;

    if (attr->playing)
        return __play(attr);
    else
        return mplayer_stop();
}

int NOISE_stop(struct NOISE_attr_t *attr)
{
    attr->playing = false;
    return mplayer_stop();
}

int NOISE_next(struct NOISE_attr_t *attr)
{
    attr->curr_id ++;

    if (attr->playing)
        return __play(attr);
    else
        return 0;
}

int NOISE_prev(struct NOISE_attr_t *attr)
{
    attr->curr_id --;

    if (attr->playing)
        return __play(attr);
    else
        return 0;
}
