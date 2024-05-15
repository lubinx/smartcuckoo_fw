#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>
#include <ultracore/log.h>

#include "mplayer.h"
#include "wt2003h.h"

/***************************************************************************
 * @implements
 ***************************************************************************/
void WT2003H_init(int devfd, uint32_t pin_busy)
{
    if (0 != pin_busy)
    {
        // WT2003H serials has no busy pin
        __BKPT(0);
    }
    uint32_t timeo = WT2003H_CMD_INTVt;
    ioctl(devfd, OPT_RD_TIMEO, &timeo);

    uint32_t ts = LOG_timestamp();
    struct wt2003h_packet_t request;
    struct wt2003h_packet_t resp;

    request.cmd = WT2003H_CMD_SET_SINK;
    request.data[0] = 1;    // DAC
    WT2003H_fill_packet(&request, 1);

    while (true)
    {
        WT2003H_send_payload(devfd, &request, 2 + request.len);

        if (0 == WT2003H_recv_payload(devfd, &resp) && resp.cmd == request.cmd)
            break;
    }
    LOG_info("WT2003H_init() use time %" PRIu32 "ms", LOG_timestamp() - ts);
}

uint8_t WT2003H_get_max_volume(uint8_t *def)
{
    if (def)
        *def = 30;
    return 30;
}

uint8_t WT2003H_get_min_volume(void)
{
    return 6;
}

int WT2003H_recv_ack(int devfd, void *requesting_payload)
{
    static bool paused = false;
    struct wt2003h_packet_t *requesting = (struct wt2003h_packet_t *)requesting_payload;
    struct wt2003h_packet_t resp = {0};

    // waitfor power up
    if (WT2003H_CMD_POWER_UP == requesting->cmd)
    {
        for (unsigned i = 0; i < 100; i ++)
        {
            WT2003H_send_payload(devfd, requesting_payload, 2 + requesting->len);

            if (0 == WT2003H_recv_payload(devfd, &resp) &&
                WT2003H_CMD_POWER_UP == resp.cmd)
            {
                return 0;
            }
        }
        return EAGAIN;
    }

    if (WT2003H_CMD_BUSY_STAT != requesting->cmd)
    {
        int ack = EAGAIN;

        if (0 == WT2003H_recv_payload(devfd, &resp) &&
            resp.cmd == requesting->cmd)
        {
            switch (resp.cmd)
            {
            case WT2003H_CMD_PLAY_TF_ROOT:
            case WT2003H_CMD_PLAY_TF_FOLDER:
                if (WT2003H_CMD_PLAY_TF_FOLDER == requesting->cmd)
                {
                    requesting->data[requesting->len - 3] = '\0';
                    memmove(&requesting->data[6], &requesting->data[5], requesting->len - 7);
                    requesting->data[5] = '/';
                }
                else
                    requesting->data[requesting->len - 3] = '\0';

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
        if (0 == WT2003H_recv_payload(devfd, &resp))
        {
            if (WT2003H_CMD_QRY_STATE == resp.cmd && 1 != resp.data[0])  // not playing, pause or stopped
                return 0;

            // 外设连接状态
            if (0xCA == resp.cmd)
            {
                // PC 接入
                if (0 != ((1U << 3) & resp.data[0]))
                    return 0;
            }
            return EAGAIN;
        }
        else
            return EAGAIN;
    }
}

uint8_t WT2003H_fill_play_payload(void *payload, char const *path)
{
    struct wt2003h_packet_t *packet = (struct wt2003h_packet_t *)payload;

    if ('/' == *path || '\\' == *path)      // root path
        path ++;

    char const *iter = path;
    while (*iter && ('/' != *iter) && ('\\' != *iter)) { iter ++; }

    size_t folder_ch_count = 0;

    if ('\0' != *iter)
    {
        packet->cmd = WT2003H_CMD_PLAY_TF_FOLDER;

        folder_ch_count = (size_t)(iter - path);
        if (5 != folder_ch_count)
        {
            // folder character count is fixed 5
            __BKPT(0);
        }
        memcpy((char *)&packet->data, path, folder_ch_count);

        iter = path = path + folder_ch_count + 1;
    }
    else
    {
        packet->cmd = WT2003H_CMD_PLAY_TF_ROOT;
        iter = path;
    }

    while (*iter && ('.' != *iter)) { iter ++; }

    size_t filename_ch_count = (size_t)(iter - path);
    if (8 < filename_ch_count)
    {
        // filename character count must < 8
        __BKPT(0);
    }
    memcpy((char *)&packet->data[folder_ch_count], path, filename_ch_count);

    return WT2003H_fill_packet(packet, (uint8_t)(folder_ch_count + filename_ch_count));
}
