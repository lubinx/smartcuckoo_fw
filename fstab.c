#include <unistd.h>

#include <ultracore/log.h>
#include <fs/fat.h>
#include <uart.h>

#include <sdio.h>
#include "PERIPHERAL_config.h"

#ifndef __EM3_SECTION
    #define __EM3_SECTION
#endif

static struct DISKIO_attr_t sdmmc_diskio;
static __EM3_SECTION struct SDMMC_attr_t sdmmc;
static __EM3_SECTION struct FAT_attr_t fat;

void FILESYSTEM_startup(void)
{
    LOG_set_level(LOG_WARN);

    #ifdef CONSOLE_DEV
        UART_pin_mux(CONSOLE_DEV, CONSOLE_TXD, CONSOLE_RXD);

        __stdout_fd = UART_createfd(CONSOLE_DEV, 115200, UART_PARITY_NONE, UART_STOP_BITS_ONE);
        printf("\n\n\n");

        LOG_info("smartcuckoo %s booting", PROJECT_ID);
    #else
        LOG_set_level(LOG_NONE);
    #endif

    DISKIO_init(&sdmmc_diskio, 8);

    SDMMC_attr_init(&sdmmc, 3300, &sdmmc_diskio);
    FAT_attr_init(&fat, &sdmmc_diskio);

    int err;

    if (0 != (err = SDMMC_pin_mux(&sdmmc, SDIO_DEV, SDIO_CLK, SDIO_CMD, SDIO_DAT)))
        goto sdmmc_print_err;

    if (0 != (err = SDMMC_card_insert(&sdmmc)))
        goto sdmmc_print_err;

    if (0 != (err = FAT_mount_fs_root(&fat, NULL)))
        goto sdmmc_print_err;

    #ifndef NDEBUG
        SDMMC_print(&sdmmc);
        FAT_attr_print(&fat);
    #endif

    if (0)
    {
    sdmmc_print_err:
        __BREAK_IFDBG();
        LOG_error("SDMMC error HALT: %s", SDMMC_strerror(err));
    }
}
