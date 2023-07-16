#include <calico.h>
#include <ff.h>
#include <diskio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void consoleInit(void);

void systemUserStartup(void)
{
	// Enable graphics
	MK_REG(u32, IO_GFX_A + IO_DISPCNT) = 1U<<16;
	MK_REG(u32, IO_GFX_B + IO_DISPCNT) = 1U<<16;
	threadWaitForVBlank();
}

static Mutex s_fatMutex;

int ff_mutex_create(FATFS* fs)
{
	dietPrint("ff_mutex_create(%p)\n", fs);
	return 1;
}

void ff_mutex_delete(FATFS* fs)
{
	dietPrint("ff_mutex_delete(%p)\n", fs);
}

int ff_mutex_take(FATFS* fs)
{
	//dietPrint("ff_mutex_take(%p)\n", fs);
	mutexLock(&s_fatMutex);
	return 1;
}

void ff_mutex_give(FATFS* fs)
{
	//dietPrint("ff_mutex_give(%p)\n", fs);
	mutexUnlock(&s_fatMutex);
}

DWORD get_fattime(void)
{
	time_t t = time(NULL);
	struct tm* stm = localtime(&t);
	return
		(DWORD)(stm->tm_year - 80) << 25 |
		(DWORD)(stm->tm_mon + 1) << 21 |
		(DWORD)stm->tm_mday << 16 |
		(DWORD)stm->tm_hour << 11 |
		(DWORD)stm->tm_min << 5 |
		(DWORD)stm->tm_sec >> 1;
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

	uptr bufaddr = (uptr)buff;
	if_likely (bufaddr >= MM_MAINRAM && bufaddr < MM_DTCM && (bufaddr & (ARM_CACHE_LINE_SZ-1)) == 0) {
		// Fast path
		return blkDevReadSectors(s_curDev, buff, sector, count) ? RES_OK : RES_ERROR;
	}

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

	uptr bufaddr = (uptr)buff;
	if_likely (bufaddr >= MM_MAINRAM && bufaddr < MM_DTCM && (bufaddr & 3) == 0) {
		// Fast path
		return blkDevWriteSectors(s_curDev, buff, sector, count) ? RES_OK : RES_ERROR;
	}

	while (count) {
		UINT this_count = count > DISK_BUF_NUM_SECTORS ? DISK_BUF_NUM_SECTORS : count;

		memcpy(s_diskBuf, buff, this_count*512);
		buff += this_count*512;

		if (!blkDevWriteSectors(s_curDev, s_diskBuf, sector, this_count)) {
			return RES_ERROR;
		}

		sector += this_count;
		count -= this_count;
	}

	return RES_WRPRT;
}

DRESULT disk_ioctl(void* pdrv, BYTE cmd, void* buff)
{
	switch (cmd) {
		default: {
			dietPrint("disk_ioctl(%u)\n", cmd);
			return RES_PARERR;
		}

		case CTRL_SYNC: {
			dietPrint("disk_ioctl(CTRL_SYNC)\n");
			return RES_OK;
		}
	}
}

int main(int argc, char* argv[])
{
	consoleInit();
	blkInit();

	dietPrint("Hello, world!\n");

	static FATFS fs;
	FRESULT res;
	bool fs_ok = false;
	res = f_mount(&fs, NULL, 0);
	if (res != FR_OK) {
		dietPrint("f_mount returned %d\n", res);
	} else {
		dietPrint("Mount OK\n");
		fs_ok = true;

		f_chdir(&fs, "nds");

		FFDIR dir;
		res = f_opendir(&dir, &fs, ".");
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

	if (fs_ok) dietPrint("Push A to write to file\n");
	dietPrint("Push START to exit\n");

	Keypad pad;
	keypadRead(&pad);

	while (pmMainLoop()) {
		threadWaitForVBlank();
		keypadRead(&pad);

		if (keypadDown(&pad) & KEY_START) {
			break;
		}

		if (fs_ok && (keypadDown(&pad) & KEY_A)) {
			FFFIL f;
			res = f_open(&f, &fs, "hello.txt", FA_OPEN_APPEND | FA_WRITE);
			if (res != FR_OK) {
				dietPrint("f_open returned %d\n", res);
			} else {
				dietPrint("Writing to file...\n");

				char mystr[256];
				int num_chars = sniprintf(mystr, sizeof(mystr), "Hello from calico/fatfs! %u\n", (unsigned)time(NULL));
				if (num_chars > 0) {
					UINT bw;
					f_write(&f, mystr, num_chars, &bw);
				}

				f_close(&f);
			}
		}
	}

	return 0;
}
