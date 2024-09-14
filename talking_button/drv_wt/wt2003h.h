#ifndef __WT2003H_H
#define __WT2003H_H                     1

#include <features.h>
#include <stdint.h>

    struct wt2003h_packet_t
    {
        uint8_t tag;
        uint8_t len;
        uint8_t cmd;
        uint8_t data[25];
    };

    #define WT2003H_CMD_INTV            (200U)
    #define WT2003H_START_TAG           (0x7EU)
    #define WT2003H_END_TAG             (0xEFU)

    enum wt2003h_cmd_t
    {
        WT2003H_CMD_SET_SINK        = 0xB6,
        WT2003H_CMD_QRY_VER         = 0xC0,
        WT2003H_CMD_QRY_STATE       = 0xC2,
        WT2003H_CMD_SET_VOLUME      = 0xAE,
        // WT2003H_CMD_LOOP_MODE       = 0xAF,

        WT2003H_CMD_PLAY_FLASH      = 0xA1,
        WT2003H_CMD_PLAY_TF_ROOT    = 0xA3,
        WT2003H_CMD_PLAY_TF_FOLDER  = 0xA5,

        WT2003H_CMD_PAUSE           = 0xAA,
        WT2003H_CMD_STOP            = 0xAB,

        WT2003H_CMD_POWER_UP        = WT2003H_CMD_QRY_VER,
        WT2003H_CMD_BUSY_STAT       = 0xFF
    };

__BEGIN_DECLS

extern __attribute__((nothrow, nonnull))
    void WT2003H_init(int devfd, uint32_t pin_busy);

    /**
     *  chip implement max/min volume value
    */
extern __attribute__((nothrow, const))
    uint8_t WT2003H_get_max_volume(uint8_t *def);
extern __attribute__((nothrow, const))
    uint8_t WT2003H_get_min_volume(void);

    /**
     *  chip implement send / recv packet
     *  @returns 0 / errno
    */
extern __attribute__((nothrow, nonnull))
    int WT2003H_send_payload(int devfd, void *payload, size_t payload_size);
extern __attribute__((nothrow, nonnull))
    int WT2003H_recv_payload(int devfd, void *payload);

    /**
     *  chip implement recv requesting ack
     *      requesting payload was previous send WT2003H_fill_xxx_payload(...)
     *
     *  @returns 0 / errno
    */
extern __attribute__((nothrow, nonnull))
    int WT2003H_recv_ack(int devfd, void *requesting_payload);

    /**
     *  chip implement fill power on / off payload
     *  @returns
     *      payload size
    */
extern __attribute__((nothrow, nonnull))
    uint8_t WT2003H_fill_power_on_payload(void *payload);

    /**
     *  chip implement fill play / stop payload
     *  @returns
     *      payload size
    */
extern __attribute__((nothrow, nonnull))
    uint8_t WT2003H_fill_play_payload(void *payload, char const *path);
extern __attribute__((nothrow, nonnull))
    uint8_t WT2003H_fill_stop_payload(void *payload);

    /**
     *  chip implement fill pause / resume payload
     *  @returns
     *      payload size
    */
extern __attribute__((nothrow, nonnull))
    uint8_t WT2003H_fill_pause_payload(void *payload);
extern __attribute__((nothrow, nonnull))
    uint8_t WT2003H_fill_resume_payload(void *payload);

    /**
     *  chip implement fill volume ctrl / volume +/-
     *  @returns
     *      payload size
    */
extern __attribute__((nothrow, nonnull))
    uint8_t WT2003H_fill_volume_payload(void *payload, uint8_t volume);

    /**
     *  WT2003H_fill_packet()
     *      fill wt2003 serials packet
     *  @returns
     *      really packet length
    */
extern __attribute__((nothrow))
    uint8_t WT2003H_fill_packet(struct wt2003h_packet_t *packet, uint8_t data_size);

    /**
     *  WT2003H_checksum()
     *      calculate wt2003 serials packet checksum
    */
extern __attribute__((nothrow))
    uint8_t WT2003H_checksum(struct wt2003h_packet_t *packet);

__END_DECLS
#endif
