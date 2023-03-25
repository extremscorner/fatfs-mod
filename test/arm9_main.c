#include <calico.h>
#include <ff.h>
#include <diskio.h>

#include <stdlib.h>
#include <string.h>

void consoleInit(void);

void systemUserStartup(void)
{
	// Enable graphics
	MEOW_REG(u32, IO_GFX_A + IO_DISPCNT) = 1U<<16;
	MEOW_REG(u32, IO_GFX_B + IO_DISPCNT) = 1U<<16;
	threadWaitForVBlank();
}

static Mutex s_fatMutex;

int ff_mutex_create(int vol)
{
	dietPrint("ff_mutex_create(%d)\n", vol);
	return 1;
}

void ff_mutex_delete(int vol)
{
	dietPrint("ff_mutex_delete(%d)\n", vol);
}

int ff_mutex_take(int vol)
{
	//dietPrint("ff_mutex_take(%d)\n", vol);
	mutexLock(&s_fatMutex);
	return 1;
}

void ff_mutex_give(int vol)
{
	//dietPrint("ff_mutex_give(%d)\n", vol);
	mutexUnlock(&s_fatMutex);
}

static BlkDevice s_curDev;

DSTATUS disk_initialize(void* pdrv)
{
	dietPrint("disk_initialize\n");
	if (blkDevInit(BlkDevice_Dldi)) {
		s_curDev = BlkDevice_Dldi;
	} else if (blkDevInit(BlkDevice_TwlSdCard)) {
		s_curDev = BlkDevice_TwlSdCard;
	} else {
		return RES_ERROR;
	}
	return RES_OK;
}

DSTATUS disk_status(void* pdrv)
{
	//dietPrint("disk_status\n");
	return RES_OK;
}

#define DISK_BUF_NUM_SECTORS 16
alignas(ARM_CACHE_LINE_SZ) static u8 s_diskBuf[DISK_BUF_NUM_SECTORS*512];

DRESULT disk_read(void* pdrv, BYTE* buff, LBA_t sector, UINT count)
{
	dietPrint("RD %p 0x%lx %u\n", buff, sector, count);

	while (count) {
		UINT this_count = count > DISK_BUF_NUM_SECTORS ? DISK_BUF_NUM_SECTORS : count;
		if (!blkDevReadSectors(s_curDev, s_diskBuf, sector, this_count)) {
			return RES_ERROR;
		}

		memcpy(buff, s_diskBuf, this_count*512);
		buff += this_count*512;

		sector += this_count;
		count -= this_count;
	}

	return RES_OK;
}

DRESULT disk_write(void* pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
	dietPrint("WR %p 0x%lx %u\n", buff, sector, count);
	return RES_WRPRT;
}

int main(int argc, char* argv[])
{
	consoleInit();
	blkInit();

	dietPrint("Hello, world!\n");

	static FATFS fs;
	FRESULT res;
	res = f_mount(&fs, "0:/", NULL, 1);
	if (res != FR_OK) {
		dietPrint("f_mount returned %d\n", res);
	} else {
		dietPrint("Mount OK\n");

		FFDIR dir;
		res = f_opendir(&dir, "0:/");
		if (res == FR_OK) {

			for (;;) {
				static FILINFO info;
				res = f_readdir(&dir, &info);
				if (res != FR_OK || !info.fname[0]) {
					break;
				}

				if (info.fattrib & AM_DIR) {
					dietPrint("[%s]\n", info.fname);
				} else {
					dietPrint(" %s\n", info.fname);
				}
			}

			f_closedir(&dir);
		}
	}

	dietPrint("Push START to exit\n");

	Keypad pad;
	keypadRead(&pad);

	while (pmMainLoop()) {
		threadWaitForVBlank();
		keypadRead(&pad);

		if (keypadDown(&pad) & KEY_START) {
			break;
		}
	}

	return 0;
}
