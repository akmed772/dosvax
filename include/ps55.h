#pragma once
//Prepro **need to modify!!!**
//#define VGA_PAGE_PS55TEXT  (0xE0000/4096)
#define NOAHDATA 0xAAAAC
#define GAIJI_RAMBASE (0xA0000/4096)

#define IS_MODE_PS55TEXT (vga.mode == M_PS55_TEXT)

void PS55_WakeUp(void);
//void PS55_ShutDown(void);
Bit16u SJIStoIBMJ(Bit16u knj);

struct MCARegisters {
	Bitu cardsel;
	bool cardsetupen;
	bool vgasetupen;
	bool mothersetupen;
};

struct PS55Registers {
	/* IBM never disclosed its internal registers although Japanese DOS uses much of them :<) */
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
	Bit8u text_mode03 = 0;//3e8 18
	PageHandler* pagehndl_backup[32];
	Bit8u data3e1_02 = 0;//3e1 02
	Bit8u data3e8_1f = 0;//3e8 1f

	Bit8u data_rotate = 0;//3eb 03
	Bit8u set_reset;
	Bit8u enable_set_reset;
	Bit8u bit_mask_low;
	Bit8u bit_mask_high;
	Bit8u map_mask;
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

typedef union {
	Bit32u d = 0;
	Bit8u b[4];
} FontPattern;


//To generate Half-width DBCS font placed at IBM code 400-4A5h
//used by DOS Bunsho Program
const Bit16u tbl_halfdbcs[][2] =
{
{0x2015,0},//˙Uin IBM Ext Kanji
{0x118,0},
{0x0,0},
{0x189,0},
{0x14c,0x10},//Åç
{0x150,0},
{0x14a,0x10},//Åã
{0x14b,0x10},//Åå
{0x144,0},
{0x145,0},
{0x13f,0},
{0x141,0},
{0x13e,0},
{0x13d,0},
{0x139,0x1a},//Åy
{0x13a,0x11},//Åz
{0x137,0x1a},//Åw
{0x138,0x11},//Åx
{0x133,0x1a},//Ås
{0x134,0x11},//Åt
{0x131,0x1a},//Åq
{0x132,0x11},//År
{0x12b,0x1a},//Åk
{0x12c,0x11},//Ål
{0x127,0x1c},//Åg
{0x128,0x10},//Åh
{0x125,0x1c},//Åe
{0x126,0x10},//Åf
{0x11f,0},
{0x120,0},
{0x11c,0},
{0x11d,0},
{0x112,0},
{0x113,0},
{0x10c,0},
{0x10e,0},
{0x2cd,0},
{0x283,0},
{0x285,0},
{0x287,0},
{0x289,0},
{0x28b,0},
{0x28d,0},
{0x28f,0},
{0x291,0},
{0x293,0},
{0x295,0},
{0x297,0},
{0x299,0},//430
{0x29c,0},
{0x29e,0},
{0x2a0,0},
{0x2a7,0},
{0x2a8,0},
{0x2aa,0},
{0x2ab,0},
{0x2ad,0},
{0x2ae,0},
{0x2b0,0},
{0x2b1,0},
{0x2b3,0},
{0x2b4,0},
{0x2c5,0},
{0x2c7,0},
{0x2c8,0},
{0x2cb, 0},
{ 0x2cc,0},//442
{0x0,0},
{0x0,0},
{0x0,0},
{0x162,0},//446 1/4Chr Triangle
{0x0,0},
{0x16A,0},//448 ? Return symbol?
{0x0,0},//449 Horizontal page border?
{0x0,0},
{0x0,0},
{0,0},//44C ?
{0x124,0},//44D Two center dots
{0x0,0},
{0x0,0},
{0,0},//450 ?
{0x0,0},
{0x0,0},
{0x21A,0},
{0x21B,0},
{0x21C,0},
{0x21D,0},
{0x21E,0},
{0x21F,0},
{0x220,0},
{0x221,0},
{0x222,0},
{0x223,0},
{0x224,0},
{0x225,0},
{0x226,0},
{0x227,0},
{0x228,0},
{0x229,0},
{0x22A,0},
{0x22B,0},
{0x22C,0},
{0x22D,0},
{0x22E,0},
{0x22F,0},
{0x230,0},
{0x231,0},
{0x232,0},
{0x233,0},
{0x234,0},
{0x235,0},
{0x236,0},
{0x237,0},
{0x238,0},
{0x239,0},
{0x23A,0},
{0x23B,0},
{0x23C,0},
{0x23D,0},
{0x23E,0},
{0x23F,0},
{0x240,0},
{0x241,0},
{0x242,0},
{0x243,0},
{0x244,0},
{0x245,0},
{0x246,0},
{0x247,0},
{0x248,0},
{0x249,0},
{0x24A,0},
{0x24B,0},
{0x24C,0},
{0x24D,0},
{0x24E,0},
{0x24F,0},
{0x250,0},
{0x251,0},
{0x252,0},
{0x253,0},
{0x254,0},
{0x255,0},
{0x256,0},
{0x257,0},
{0x258,0},
{0x259,0},
{0x25A,0},
{0x25B,0},
{0x25C,0},
{0x25D,0},
{0x25E,0},
{0x25F,0},
{0x260,0},
{0x261,0},
{0x262,0},
{0x263,0},
{0x264,0},
{0x265,0},
{0x266,0},
{0x267,0},
{0x268,0},
{0x269,0},
{0x26A,0},
{0x26B,0},
{ 0x26C,0 } };
