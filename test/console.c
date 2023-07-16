#include <calico.h>

#include "font_bin.h"

#define PALRAM ((vu16*)MM_PALRAM)

static Mutex s_conMutex;
static u16* s_conTilemap;
static int s_conCursorX, s_conCursorY;

static void consoleNewRow(void)
{
	s_conCursorY ++;
	if (s_conCursorY >= 24) {
		s_conCursorY --;
		svcCpuSet(&s_conTilemap[32], &s_conTilemap[0], SVC_SET_UNIT_16 | SVC_SET_SIZE_16(23*32*2));
	}
	u16 fill = 0;
	svcCpuSet(&fill, &s_conTilemap[s_conCursorY*32], SVC_SET_UNIT_16 | SVC_SET_FIXED | SVC_SET_SIZE_16(32*2));
}

static void consolePutChar(int c)
{
	if (s_conCursorX >= 32 && c != '\n') {
		s_conCursorX = 0;
		consoleNewRow();
	}

	switch (c) {
		default:
			s_conTilemap[s_conCursorY*32 + s_conCursorX] = c;
			s_conCursorX++;
			break;

		case '\t':
			s_conCursorX = (s_conCursorX + 4) &~ 3;
			break;

		case '\n':
			consoleNewRow();
			/* fallthrough */

		case '\r':
			s_conCursorX = 0;
			break;
	}
}

static void consoleWrite(const char* buf, size_t size)
{
	mutexLock(&s_conMutex);

	if_likely (buf) {
		for (size_t i = 0; i < size; i ++) {
			consolePutChar(buf[i]);
		}
	} else {
		for (size_t i = 0; i < size; i ++) {
			consolePutChar(' ');
		}
	}

	mutexUnlock(&s_conMutex);
}

void consoleInit(void)
{
	REG_VRAMCNT_A = VRAM_CONFIG(1, 0);

	SvcBitUnpackParams params = {
		.in_length_bytes = font_bin_size,
		.in_width_bits   = 1,
		.out_width_bits  = 4,
		.data_offset     = 14, // 1+14 = 15
		.zero_data_flag  = 0,
	};
	svcBitUnpack(font_bin, (void*)MM_VRAM_BG_A, &params);

	MK_REG(u16, IO_GFX_A + IO_BGxCNT(0)) = 0 | (0<<2) | (0<<7) | (4<<8) | (0<<14);
	MK_REG(u32, IO_GFX_A + IO_DISPCNT) |= (1<<8);

	s_conTilemap = (u16*)(MM_VRAM_BG_A + 8*1024);

	PALRAM[0] = 0 | (0<<5) | (0<<10);
	PALRAM[15] = 24 | (24<<5) | (24<<10);
	PALRAM[0x200] = 0 | (0<<5) | (0<<10);

	dietPrintSetFunc(consoleWrite);
	installArm7DebugSupport(consoleWrite, MAIN_THREAD_PRIO-1);
}

void consoleHorridHack(unsigned pos)
{
	mutexLock(&s_conMutex);
	if (s_conCursorY >= 0) {
		s_conCursorY --;
	}
	s_conCursorX = pos;
	mutexUnlock(&s_conMutex);
}
