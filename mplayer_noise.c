#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mplayer.h"
#include "mplayer_noise.h"

static struct NOISE_mapping_t __noise_mapping[] =
{
    {.id = "01",    .name = "AirconFan"},
    {.id = "02",    .name = "Cabin"},
    {.id = "03",    .name = "CampFire"},
    {.id = "04",    .name = "CityStreet"},
    {.id = "05",    .name = "Courtyard"},
    {.id = "06",    .name = "Forest"},
    {.id = "07",    .name = "Highway"},
    {.id = "08",    .name = "Raindrops"},
    {.id = "09",    .name = "SummerNight"},
    {.id = "10",    .name = "Thunder"},
    {.id = "11",    .name = "WhiteNoise"},
    {.id = "12",    .name = "Wind"},
};

int NOISE_attr_init(struct NOISE_attr_t *attr)
{
    memset(attr, 0, sizeof(*attr));
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

    char filename[40];
    sprintf(filename, "noise/%s", attr->curr->name);
    return mplayer_play_loop(filename);
}

int NOISE_toggle(struct NOISE_attr_t *attr)
{
    if (NULL == attr->curr)
        attr->curr = &__noise_mapping[0];

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
    if (NULL == attr->curr)
        attr->curr = &__noise_mapping[0];

    if (attr->playing)
    {
        attr->curr ++;
        if (attr->curr > &__noise_mapping[lengthof(__noise_mapping) - 1])
            attr->curr = &__noise_mapping[0];

        return __play(attr);
    }
    else
        return 0;
}

int NOISE_prev(struct NOISE_attr_t *attr)
{
    if (NULL == attr->curr)
        attr->curr = &__noise_mapping[0];

    if (attr->playing)
    {
        attr->curr --;
        if (attr->curr < &__noise_mapping[0])
            attr->curr = &__noise_mapping[lengthof(__noise_mapping) - 1];

        return __play(attr);
    }
    else
        return 0;
}
