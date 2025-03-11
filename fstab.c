#include <unistd.h>

#include <ultracore/log.h>
#include <fs/fat.h>
#include <uart.h>

#include <sdio.h>
#include "PERIPHERAL_config.h"

static struct DISKIO_attr_t sdmmc_diskio;
static struct SDMMC_attr_t sdmmc;
static struct FAT_attr_t fat;

static struct DISKIO_lblk_t diskio_ext_buf[16];

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

    DISKIO_init(&sdmmc_diskio);
    DISKIO_add_cache(&sdmmc_diskio, diskio_ext_buf, lengthof(diskio_ext_buf));
    SDMMC_attr_init(&sdmmc, 3300, &sdmmc_diskio);

    int retval = SDMMC_pin_mux(&sdmmc, SDIO_DEV, SDIO_CLK, SDIO_CMD, SDIO_DAT);
    if (0 != retval)
        goto sdmmc_print_err;

    retval = SDMMC_card_insert(&sdmmc);
    if (0 != retval)
        goto sdmmc_print_err;

    FAT_attr_init(&fat, &sdmmc_diskio);
    retval = FAT_attr_mount(&fat);
    if (0 != retval)
        goto sdmmc_print_err;
    else
        FAT_mount_root(&fat);

    #ifndef NDEBUG
        SDMMC_print(&sdmmc);
        FAT_attr_print(&fat);
    #endif

    if (false)
    {
    sdmmc_print_err:
        __BREAK_IFDBG();
        LOG_error("SDMMC error HALT: %s", SDMMC_strerror(retval));
    }
}
