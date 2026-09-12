#include "ff.h"
#include "diskio.h"


#define DEV_SD 0


static DiskSectorCountProvider sector_count_provider;
static DiskBlockSizeProvider block_size_provider;
static DiskStatusProvider status_provider;
static DiskReadProvider read_provider;
static DiskWriteProvider write_provider;


void disk_set_sector_count_provider (DiskSectorCountProvider provider)
{
    sector_count_provider = provider;
}

void disk_set_block_size_provider (DiskBlockSizeProvider provider)
{
    block_size_provider = provider;
}

void disk_set_status_provider (DiskStatusProvider provider)
{
    status_provider = provider;
}

void disk_set_read_provider (DiskReadProvider provider)
{
    read_provider = provider;
}

void disk_set_write_provider (DiskWriteProvider provider)
{
    write_provider = provider;
}

DSTATUS disk_status (BYTE pdrv)
{
    if (pdrv != DEV_SD || status_provider == 0)
        return STA_NOINIT;

    return status_provider();
}

DSTATUS disk_initialize (BYTE pdrv)
{
    return disk_status(pdrv);
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv != DEV_SD || buff == 0 || count == 0U)
        return RES_PARERR;

    if (read_provider == 0)
        return RES_NOTRDY;

    return read_provider(buff, sector, count);
}

#if FF_FS_READONLY == 0
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv != DEV_SD || buff == 0 || count == 0U)
        return RES_PARERR;

    if (write_provider == 0)
        return RES_NOTRDY;

    return write_provider(buff, sector, count);
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
    if (pdrv != DEV_SD)
        return RES_PARERR;

    switch (cmd)
    {
        case CTRL_SYNC:
            return disk_status(pdrv) == 0U ? RES_OK : RES_NOTRDY;

        case GET_SECTOR_COUNT:
            if (buff == 0 || sector_count_provider == 0)
                return RES_NOTRDY;
            *(LBA_t*)buff = sector_count_provider();
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (buff == 0)
                return RES_PARERR;
            *(WORD*)buff = FF_MAX_SS;;
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (buff == 0 || block_size_provider == 0)
                return RES_NOTRDY;
            *(DWORD*)buff = block_size_provider();
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
