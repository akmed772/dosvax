/*
Copyright (c) 2021-2024 akm
All rights reserved.
This content is under the MIT License.
*/

#pragma once
//Prepro **need to modify!!!**
#include "vga.h"
//#define VGA_PAGE_PS55TEXT  (0xE0000/4096)
#define NOAHDATA 0xAAAAC
#define GAIJI_RAMBASE (0xA0000/4096)
#define PS55_BITBLT_MEMSIZE 0x80
#define PS55_BITBLT_REGSIZE 0x40
#define PS55_DEBUG_BITBLT_SIZE 0x41

#define IS_MODE_PS55TEXT (vga.mode == M_PS55_TEXT)

void PS55_WakeUp(void);
void PS55_Suspend(void);
Bit16u SJIStoIBMJ(Bit16u knj);

struct MCARegisters {
	Bitu cardsel;
	Bitu idx_nvram;
	bool cardsetupen;
	bool vgasetupen;
	bool mothersetupen;
};

typedef struct {
	Bit8s bitshift_destr;
	Bit8u raster_op;

	Bit8u payload[PS55_BITBLT_MEMSIZE];
	Bitu reg[PS55_BITBLT_REGSIZE];
	Bitu debug_reg[65536][PS55_DEBUG_BITBLT_SIZE];//for debug
	Bit16u debug_reg_ip = 0;//for debug
} PS55_bitblt;

struct PS55Registers {
	PS55_bitblt bitblt;
	bool carden = 0;
	Bitu idx_3e1 = 0;
	Bit16u gaijiram_access = 0;
	//Bitu prev_3e1 = 0;
	Bitu prevdata = NOAHDATA;
	Bitu nextret = NOAHDATA;
	Bitu idx_3e3 = 0;
	//Bitu prev_3e3 = 0;
	Bitu idx_3e5 = 0;
	//Bitu prev_3e5 = 0;
	Bitu idx_3e8 = 0;
	bool latch_3e8 = false;
	//Bitu prev_3e9 = 0;
	Bitu idx_3eb = 0;
	//Bitu prev_3eb = 0;
	//Bit8u palette_attr[16];
	Bit8u palette_line;
	Bit8u attr_mode = 0;//3e8 1d
	Bit8u* gaiji_ram;
	//Bit8u* font_da1; for DA1
	Bit8u mem_conf = 0x2b;//3e3 00
	Bit8u data3e3_08 = 0;//3e3 08
	Bit8u mem_bank = 0;//3e3 0a
	Bit8u mem_select = 0;//3e3 0b
	bool cursor_wide = false;//3e5 1f
	bool crtc_cmode = false;//3e5 1f
	Bit8u cursor_options = 0x13;//3e8 1b
	Bit8u cursor_color = 0x0f;//3e8 1a
	Bit8u cursor_blinkspeed = 0xff;
	Bit16u crtc_reg[0x20];//CRTC registers
	Bit8u p3e1_reg[0x20];//CRTC registers
	Bit8u attr_reg[0x20];//ATTR registers for debug
	Bit8u text_mode03 = 0;//3e8 18
	PageHandler* pagehndl_backup[32];
	Bit8u data3e1_02 = 0;//3e1 02
	Bit8u data3e1_03 = 0x80;//3e1 03
	Bit8u data3e8_1f = 0;//3e8 1f
	Bit8u data3ee = 0;//3ee 00
	bool palette_mono = false;
	Bit8u data_rotate = 0;//3eb 03
	Bit8u set_reset;
	Bit8u enable_set_reset;
	Bit8u bit_mask_low;
	Bit8u bit_mask_high;
	Bit8u map_mask;
	Bit8u data3ea_0b;//???? unknown GC register
	Bit32u full_map_mask_low;
	Bit32u full_not_map_mask_low;
	Bit32u full_map_mask_high;
	Bit32u full_not_map_mask_high;
	Bit32u full_bit_mask_low;//3eb 08
	Bit32u full_bit_mask_high;//3eb 09
	Bit32u full_set_reset_low;
	Bit32u full_not_enable_set_reset_low;
	Bit32u full_enable_set_reset_low;
	Bit32u full_enable_and_set_reset_low;
	Bit32u full_set_reset_high;
	Bit32u full_not_enable_set_reset_high;
	Bit32u full_enable_set_reset_high;
	Bit32u full_enable_and_set_reset_high;
	VGA_Latch latchw1, latchw2;
	//for backup VGA register
	Bit8u vga_crtc_overflow;
	Bitu vga_config_line_compare;
};

extern PS55Registers ps55;
extern MCARegisters mca;

/*
Registers I/O test used in the Diagnostic Diskette (@EFFE.DGS)
si Method ?(Subst)
0B ?(Subst) *A Mask? wb rb test
0D ?(Subst) *A Mask-H? *A Mask-L? wbwb rbrb test
0C ?(Subst) *A Mask-H? *A Mask-L? ww rw test
01 Data-High Data-Low ww
02 Index Data-H Data-L wb ww
AA 1010 1010
55 0101 0101

3E0 3E1
		;00 01->
1 0f 0c
b 01 17;?
1 01 00
b 02 3d;?
1 02 15	;02 15 ->
00:
	Mode a: 0
	Mode e:0
02:
	Mode a:11 -> 10
	Mode e:0
		;03 (read only?) bit 1: busy
		;08 bit 4: Gaiji RAM access enabled (A0000-BFFFFh)
		;0A (read only) ?
		;0B (read only) ?
b 0d 01;?
b 0e 07;?
b 0f 08;?
1 0f 0c
1 01 00
1 0d 00
1 0e 00
f5;end ?

3E2 3E3;Font Buffer Registers (undoc)
b 00 6f;?;00 -> 80
b 01 0f;?
b 08 81;?
b 09 0f;?
1 08 00
b 0a ff;?bit 0: Gaiji RAM Page Select 1/0
b 0b ff;?
		;18 (read only) ??? 07 -> 00 -> B0
1 00 00
1 01 00
f5;end?

3E4 3E5;CRT Control Registers (undoc) (AH=Data, AL=Index)
b 1c e0
1 1c 80
b 00 ff		;Horizontal Total Register
b 01 ff		;Horizontal Display End Register
b 02 ff		;Start Horizontal Blanking Register
b 03 ff		;End Horizontal Blanking Register
b 04 ff		;Start Horizontal Retrace Register
b 05 ff		;End Horizontal Retrace Register
d 06 cf ff	;Vertical Total Register (dw)
b 08 7f		;Preset Row Scan Register
b 09 ff		;Maximum Scan Line Register
b 0a 1f		;Cursor Start Register
b 0b 7f		;Cursor End Register
d 0c 03 ff	;Start Address High Register (dw)
b 0d ff		;Start Address Low Register
d 0e 0f ff	;Cursor Location High Register (dw)
b 0f ff		;Cursor Location Low Register
d 10 0f ff	;Vertical Retrace Start Register (dw)
b 11 3f		;Vertical Retrace End Register
d 12 0f ff	;Vertical Display End Register (dw)
b 13 ff		;Offset register
b 14 7f		;Underline Location Register
d 15 0f ff	;Start Vertical Blank Register (dw)
d 16 0f ff	;End Vertical Blank Register (dw)
b 17 c7		;Mode Control Register
d 18 0f ff	;Line Compare Register (dw)
			;19 cannot test?
d 1a 0f ff	;? (dw)
b 1b 64	(0011 1000)	;?
			;1c cannot test?
b 1d 87	(1000 0111)	;? 80
b 1e dc	(1101 1100)	;? 44
					;? 62
			;1f cursor control? (02 only in videomode 3)
f5;end?

3E8 3E9 Attribute Controller Registers (undoc)
b 00 3f		;Text Attribute Color chosen from 64 palettes (00h-0fh)
b 01 3f
b 02 3f
b 03 3f
b 04 3f
b 05 3f
b 06 3f
b 07 3f
b 08 3f
b 09 3f
b 0a 3f
b 0b 3f
b 0c 3f
b 0d 3f
b 0e 3f
b 0f 3f
b 10 b8 (1011 1000)
b 11 ff
b 12 3f
b 13 1f
b 14 0f
b 17 3f
b 18 0f
b 19 1f
b 1a ff
b 1b 8b
b 1c 46
b 1d ff
b 1e 3f
b 1f c0
b 30 ff
b 31 ff		;
b 34 0f		; Line color
b 38 49		; (01 only in videomode 3)
b 3a 0f		;? 0c
b 3b 1b		; Cursor options (blinking speed, invisible)
			;3c cant test? w80 r08(-wait)
b 3d e0 (1110 0000);Attribute Mode? bit 7:color/mono, bit 6:light color
			;3e cannot test?
b 3f 0f
f5 00;end

3EA 3EB 3EC;Graphics Contoller Registers
c 05 ff 00
2 05 40 00
c 0b 0f 00
2 0b 08 00
b 00 0f		;Set/Reset Register
b 01 0f		;Enable Set/Reset Register
b 02 0f		;Color Compare Register
b 03 ff		;Data Rotate
b 04 0f		;Read Map Select Register
			;05 cannot test? (Mode Register)
			;06 Reserved?
b 07 0f		;Color Don't Care Register
c 08 ff 00	;Bit Mask Register Low
c 09 ff 00	;Bit Mask Register High
b 0a 0f		;Map Mask
			;0b cannot test? (Command)
c 0c ff 00	;?
1 0a 01		;01->Map Mask
1 04 00		;00->Read Map Select Register
c 06 ff ff	;?
1 0a 02		;02->Map Mask
1 04 01		;01->Read Map Select Register
c 06 ff ff	;?
1 0a 04		;04->Map Mask
1 04 02		;02->Read Map Select Register
c 06 ff ff	;?
1 0a 08		;08->Map Mask
1 04 03		;03->Read Map Select Register
c 06 ff ff	;?

f5;end
*/
