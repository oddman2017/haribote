/* bootpackのメイン */

#include "bootpack.h"
#include <stdio.h>

extern struct FIFO8 keyfifo, mousefifo;
void enable_mouse(void);
void init_keyboard(void);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40], mcursor[256], keybuf[32], mousebuf[128];
	int mx, my, i;
	unsigned char mouse_dbuf[3], mouse_phase;

	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除 */
	fifo8_init(&keyfifo, 32, keybuf);
	fifo8_init(&mousefifo, 128, mousebuf);
	io_out8(PIC0_IMR, 0xf9); /* PIC1とキーボードを許可(11111001) */
	io_out8(PIC1_IMR, 0xef); /* マウスを許可(11101111) */

	init_keyboard();

	init_palette();
	init_screen8(binfo->vram, binfo->scrnx, binfo->scrny);
	mx = (binfo->scrnx - 16) / 2; /* 画面中央になるように座標計算 */
	my = (binfo->scrny - 28 - 16) / 2;
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
	sprintf(s, "(%d, %d)", mx, my);
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	enable_mouse();
	mouse_phase = 0; /* マウスの0xfaを待っている段階へ */

	for (;;) {
		io_cli();
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
			io_stihlt();
		} else {
			if (fifo8_status(&keyfifo) != 0) {
				i = fifo8_get(&keyfifo);
				io_sti();
				sprintf(s, "%02X", i);
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484,  0, 16, 15, 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
			} else if (fifo8_status(&mousefifo) != 0) {
				i = fifo8_get(&mousefifo);
				io_sti();
				if (mouse_phase == 0) {
					/* マウスの0xfaを待っている段階 */
					if (i == 0xfa) {
						mouse_phase = 1;
					}
				} else if (mouse_phase == 1) {
					/* マウスの1バイト目を待っている段階 */
					mouse_dbuf[0] = i;
					mouse_phase = 2;
				} else if (mouse_phase == 2) {
					/* マウスの2バイト目を待っている段階 */
					mouse_dbuf[1] = i;
					mouse_phase = 3;
				} else if (mouse_phase == 3) {
					/* マウスの3バイト目を待っている段階 */
					mouse_dbuf[2] = i;
					mouse_phase = 1;
					/* データが3バイト揃ったので表示 */
					sprintf(s, "%02X %02X %02X", mouse_dbuf[0], mouse_dbuf[1], mouse_dbuf[2]);
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 32, 16, 32 + 8 * 8 - 1, 31);
					putfonts8_asc(binfo->vram, binfo->scrnx, 32, 16, COL8_FFFFFF, s);
				}
			}
		}
	}
}

#define PORT_KEYDAT				0x0060
#define PORT_KEYSTA				0x0064
#define PORT_KEYCMD				0x0064
#define KEYSTA_SEND_NOTREADY	0x02
#define KEYCMD_WRITE_MODE		0x60
#define KBC_MODE				0x47

void wait_KBC_sendready(void)
{
	/* キーボードコントローラがデータ送信可能になるのを待つ */
	for (;;) {
		if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0) {
			break;
		}
	}
	return;
}

void init_keyboard(void)
{
	/* キーボードコントローラの初期化 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, KBC_MODE);
	return;
}

#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

void enable_mouse(void)
{
	/* マウス有効 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	return; /* うまくいくとACK(0xfa)が送信されてくる */
}
