#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>

#include <gpio.h>
#include <ultracore/log.h>

#include "mplayer.h"
#include "wt2003h.h"

/***************************************************************************
 * @private
 ***************************************************************************/
static uint32_t PIN_busy = 0;

/***************************************************************************
 * @implements
 ***************************************************************************/
void WT2003H_init(int devfd, uint32_t pin_busy)
{
    PIN_busy = pin_busy;

    struct wt2003h_packet_t request_set_sink;
    struct wt2003h_packet_t resp;

    uint32_t ts = LOG_timestamp();

    request_set_sink.cmd = WT2003H_CMD_SET_SINK;
    request_set_sink.data[0] = 1;   // DAC
    WT2003H_fill_packet(&request_set_sink, 1);

    #ifdef __linux__
        // select IO, set non-block
        int opt = O_NONBLOCK | fcntl(devfd, F_GETFL);
        fcntl(devfd, F_SETFL, opt);
    #else
        uint32_t timeo = WT2003H_CMD_INTVt;
        ioctl(devfd, OPT_RD_TIMEO, &timeo);
    #endif

    clock_t tick = clock();
    while (1000 > clock() - tick)
    {
        WT2003H_send_payload(devfd, &request_set_sink, 2 + request_set_sink.len);

        if (0 == WT2003H_recv_payload(devfd, &resp))
            break;
    }

    LOG_info("WT2003H_init() use time %" PRIu32 "ms", LOG_timestamp() - ts);
    GPIO_setdir_input(PIN_busy);
}

uint8_t WT2003H_get_max_volume(uint8_t *def)
{
    if (def)
        *def = 31;

    return 31;
}

uint8_t WT2003H_get_min_volume(void)
{
    return 31 - 24;
}

int WT2003H_recv_ack(int devfd, void *requesting_payload)
{
    static bool paused = false;
    struct wt2003h_packet_t *requesting = (struct wt2003h_packet_t *)requesting_payload;
    struct wt2003h_packet_t resp = {0};

    // waitfor power up
    if (WT2003H_CMD_POWER_UP == requesting->cmd)
    {
        WT2003H_send_payload(devfd, requesting_payload, 2 + requesting->len);
        msleep(50);

        if (0 == WT2003H_recv_payload(devfd, &resp) &&
            WT2003H_CMD_POWER_UP == resp.cmd)
        {
            return 0;
        }
        return EAGAIN;
    }

    if (WT2003H_CMD_BUSY_STAT != requesting->cmd)
    {
        int ack = EAGAIN;

        if (0 == WT2003H_recv_payload(devfd, &resp) &&
            resp.cmd == requesting->cmd)
        {
            switch (requesting->cmd)
            {
            case WT2003H_CMD_PLAY_FLASH:
                requesting->data[4] = '\0';

                if (0 == resp.data[0])
                {
                    LOG_info("playing: %s", requesting->data);
                    requesting->cmd = WT2003H_CMD_BUSY_STAT;
                    ack = EEXIST;
                }
                else
                {
                    LOG_warning("file: %s not exists", requesting->data);
                    ack = ENOENT;
                }
                break;

            case WT2003H_CMD_PAUSE:
                requesting->cmd = WT2003H_CMD_BUSY_STAT;
                paused = ! paused;
                ack = 0;
                break;

            default:
                ack = (resp.cmd == requesting->cmd ? 0 : EAGAIN);
                break;
            };
        }
        else
        {
            if (WT2003H_CMD_STOP == requesting->cmd)
            {
                msleep(100);
                ack = 0;
            }
        }
        return ack;
    }
    else
    {
        if (! paused && (0 == GPIO_peek(PIN_busy)))
            return 0;
        else
            return EAGAIN;
    }
}

uint8_t WT2003H_fill_play_payload(void *payload, char const *path)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;

    packet->cmd = WT2003H_CMD_PLAY_FLASH;
    strncpy((char *)&packet->data, path, 4);
    return WT2003H_fill_packet(packet, 4);
}
