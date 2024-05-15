#include <ultracore.h>
#include <unistd.h>
#include <flash.h>
#include <fs/ultrafs.h>
#include <i2c.h>

/****************************************************************************
 *  @filesystem
 ****************************************************************************/
void FILESYSTEM_startup(void) /** @override */
{
    /*
    int flash_fd = FLASH_createfd();

    if (0 == ULTRAFS_mount(flash_fd, "flashfs", FLASH_PAGE_SIZE, FLASH_AVAIL_SIZE, FLASH_ERASE_FILL))
        chdir("/mnt/flashfs");
    */
}
