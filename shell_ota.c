#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <pmu.h>
#include <wdt.h>
#include <flash.h>
#include <gpio.h>
#include <wdt.h>
#include <sh/ucsh.h>
#include <fs/ultrafs.h>
#include <hash/crc_ccitt.h>

extern void PERIPHERAL_ota_init(void);

#ifndef OTA_downloading
    #define OTA_downloading()
#endif

#ifndef OTA_download_complete
    #define OTA_download_complete()
#endif

int SHELL_ota(struct UCSH_env *env)
{
    struct OTA_packet
    {
        uint16_t Idx;
        CRC_CCITT_t crc;
        uint8_t Payload[16];
    };

    struct OTA_packet packet;
    uint32_t size;
    CRC_CCITT_t crc;

    int ota_fd;
    // parse command args
    {
        char *param;
        char *p;

        param = CMD_paramvalue_byname("s", env->argc, env->argv);
        if (! param)
            return EINVAL;
        size = strtoul(param, &p, 10);
        if (0 == size && 'x' == tolower(*p))
            size = strtoul(param, NULL, 16);

        param = CMD_paramvalue_byname("c", env->argc, env->argv);
        if (! param)
            return EINVAL;
        crc  = (uint16_t)strtoul(param, &p, 10);
        if (0 == crc && 'x' == tolower(*p))
            crc = (uint16_t)strtoul(param, NULL, 16);
    }

    PMU_power_acquire();
    PERIPHERAL_ota_init();

    ota_fd = FLASH_otafd(size); // open("ota.bin", O_RDWR | O_CREAT | O_TRUNC);
    if (-1 == ota_fd)
    {
        /// @insufficient memory is only case, that we can't do anything about it
        NVIC_SystemReset();
        while (1);
    }
    // response "ok"
    write(env->fd, "0: ok\n", 6);

    uint32_t packet_count = ((size + sizeof(packet.Payload) - 1) & ~(sizeof(packet.Payload) - 1)) / sizeof(packet.Payload);

    for (unsigned idx = 0; idx < packet_count; idx ++)
    {
        WDOG_feed();

        if (sizeof(packet) != readbuf(env->fd, &packet, sizeof(packet)) || idx != packet.Idx)
            goto ota_io_error;

        // packet crc
        uint16_t packet_crc = CRC_CCITT_hash(packet.Payload, sizeof(packet.Payload));
        if (packet_crc != packet.crc)
            goto ota_crc_error;

        // ota writing
        if (sizeof(packet.Payload) != writebuf(ota_fd, packet.Payload, sizeof(packet.Payload)))
            goto ota_io_error;

        OTA_downloading();
    }
    OTA_download_complete();

    if (FLASH_ota_final(ota_fd, crc))
    {
        // response "ok"
        write(env->fd, "0: ok\n", 6);
        msleep(1000);

        /// @successful need to reset to finish UPGRADE
        NVIC_SystemReset();
    }
    else
        goto ota_crc_error;

    if (false)
    {
ota_crc_error:
        UCSH_error_handle(env, EINTEGRITY);
        msleep(1000);
    }
    if (false)
    {
ota_io_error:
        UCSH_error_handle(env, EIO);
        msleep(1000);
    }

    close(ota_fd);
    NVIC_SystemReset();
    return 0;
}
