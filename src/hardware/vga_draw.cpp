/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *  Copyright (C) 2016-2023 akm
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "jsupport.h"
#include "jega.h"//for AX

#include <string.h>
#include <math.h>
#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "pic.h"
#include "ps55.h"

//#undef C_DEBUG
//#define C_DEBUG 1
//#define LOG(X,Y) LOG_MSG

#define VGA_PARTS 4

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static Bit8u TempLine[SCALER_MAXWIDTH * 4];

static Bit8u * VGA_Draw_1BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u *draw = (Bit32u *)TempLine;
	for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
		Bitu val = base[(vidstart & (8 * 1024 -1))];
		*draw++=CGA_2_Table[val >> 4];
		*draw++=CGA_2_Table[val & 0xf];
	}
	return TempLine;
}

static Bit8u* VGA_PS55_Draw_1BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u* base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	//const Bit8u* base = vga.tandy.draw_base;
	//Bit32u* draw = (Bit32u*)TempLine;
	//for (Bitu x = vga.draw.blocks; x > 0; x--, vidstart++) {
	//	Bitu val = base[(vidstart & (8 * 1024 - 1))];
	//	*draw++ = CGA_2_Table[val >> 4];
	//	*draw++ = CGA_2_Table[val & 0xf];
	//}
	Bit32u* draw = (Bit32u *)TempLine;
	//Bitu end = vga.draw.blocks;
	Bitu end = 0x80;
	while (end) {
		Bitu val = base[vidstart & 0x1ffff];
		*draw++ = CGA_2_Table[val >> 4];
		*draw++ = CGA_2_Table[val & 0xf];
		vidstart++;
		end--;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val = base[vidstart & vga.tandy.addr_mask];
		vidstart++;
		*draw++=CGA_4_Table[val];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPPHiRes_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		Bitu val2 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=CGA_4_HiRes_Table[(val1>>4)|(val2&0xf0)];
		*draw++=CGA_4_HiRes_Table[(val1&0x0f)|((val2&0x0f)<<4)];
	}
	return TempLine;
}

static Bitu temp[643]={0};

static Bit8u * VGA_Draw_CGA16_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
#define CGA16_READER(OFF) (base[(vidstart +(OFF))& (8*1024 -1)])
	Bit32u * draw=(Bit32u *)TempLine;
	//There are 640 hdots in each line of the screen.
	//The color of an even hdot always depends on only 4 bits of video RAM.
	//The color of an odd hdot depends on 4 bits of video RAM in
	//1-hdot-per-pixel modes and 6 bits of video RAM in 2-hdot-per-pixel
	//modes. We always assume 6 and use duplicate palette entries in
	//1-hdot-per-pixel modes so that we can use the same routine for all
	//composite modes.
  temp[1] = (CGA16_READER(0) >> 6) & 3;
	for(Bitu x = 2; x < 640; x+=2) {
		temp[x] = (temp[x-1] & 0xf);
		temp[x+1] = (temp[x] << 2) | ((( CGA16_READER(x>>3)) >> (6-(x&6)) )&3);
	}
	temp[640] = temp[639] & 0xf;
	temp[641] = temp[640] << 2;
	temp[642] = temp[641] & 0xf;

	Bitu i = 2;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		*draw++ = 0xc0708030 | temp[i] | (temp[i+1] << 8) | (temp[i+2] << 16) | (temp[i+3] << 24);
		i += 4;
		*draw++ = 0xc0708030 | temp[i] | (temp[i+1] << 8) | (temp[i+2] << 16) | (temp[i+3] << 24);
		i += 4;
	}
	return TempLine;
#undef CGA16_READER
}

static Bit8u * VGA_Draw_4BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit8u* draw=TempLine;
	Bitu end = vga.draw.blocks*2;
	while(end) {
		Bit8u byte = base[vidstart & vga.tandy.addr_mask];
		*draw++=vga.attr.palette[byte >> 4];
		*draw++=vga.attr.palette[byte & 0x0f];
		vidstart++;
		end--;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line_Double(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit8u* draw=TempLine;
	Bitu end = vga.draw.blocks;
	while(end) {
		Bit8u byte = base[vidstart & vga.tandy.addr_mask];
		Bit8u data = vga.attr.palette[byte >> 4];
		*draw++ = data; *draw++ = data;
		data = vga.attr.palette[byte & 0x0f];
		*draw++ = data; *draw++ = data;
		vidstart++;
		end--;
	}
	return TempLine;
}

#ifdef VGA_KEEP_CHANGES
static Bit8u * VGA_Draw_Changes_Line(Bitu vidstart, Bitu line) {
	Bitu checkMask = vga.changes.checkMask;
	Bit8u *map = vga.changes.map;
	Bitu start = (vidstart >> VGA_CHANGE_SHIFT);
	Bitu end = ((vidstart + vga.draw.line_length ) >> VGA_CHANGE_SHIFT);
	for (; start <= end;start++) {
		if ( map[start] & checkMask ) {
			Bitu offset = vidstart & vga.draw.linear_mask;
			if (vga.draw.linear_mask-offset < vga.draw.line_length)
				memcpy(vga.draw.linear_base+vga.draw.linear_mask+1, vga.draw.linear_base, vga.draw.line_length);
			Bit8u *ret = &vga.draw.linear_base[ offset ];
#if !defined(C_UNALIGNED_MEMORY)
			if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
				memcpy( TempLine, ret, vga.draw.line_length );
				return TempLine;
			}
#endif
			return ret;
		}
	}
//	memset( TempLine, 0x30, vga.changes.lineWidth );
//	return TempLine;
	return 0;
}

#endif

static Bit8u * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/) {
	Bitu offset = vidstart & vga.draw.linear_mask;
	Bit8u* ret = &vga.draw.linear_base[offset];
	
	// in case (vga.draw.line_length + offset) has bits set that
	// are not set in the mask: ((x|y)!=y) equals (x&~y)
	if (GCC_UNLIKELY((vga.draw.line_length + offset)& ~vga.draw.linear_mask)) {
		// this happens, if at all, only once per frame (1 of 480 lines)
		// in some obscure games
		Bitu end = (offset + vga.draw.line_length) & vga.draw.linear_mask;
		
		// assuming lines not longer than 4096 pixels
		Bitu wrapped_len = end & 0xFFF;
		Bitu unwrapped_len = vga.draw.line_length-wrapped_len;
		
		// unwrapped chunk: to top of memory block
		memcpy(TempLine, &vga.draw.linear_base[offset], unwrapped_len);
		// wrapped chunk: from base of memory block
		memcpy(&TempLine[unwrapped_len], vga.draw.linear_base, wrapped_len);

		ret = TempLine;
	}

#if !defined(C_UNALIGNED_MEMORY)
	if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif

	//Original JEGA can be set a cursor on.
	return ret;
}

static Bit8u * VGA_Draw_Xlat16_Linear_Line(Bitu vidstart, Bitu /*line*/) {
	Bitu offset = vidstart & vga.draw.linear_mask;
	Bit8u *ret = &vga.draw.linear_base[offset];
	Bit16u* temps = (Bit16u*) TempLine;

	// see VGA_Draw_Linear_Line
	if (GCC_UNLIKELY((vga.draw.line_length + offset)& ~vga.draw.linear_mask)) {
		Bitu end = (offset + vga.draw.line_length) & vga.draw.linear_mask;
		
		// assuming lines not longer than 4096 pixels
		Bitu wrapped_len = end & 0xFFF;
		Bitu unwrapped_len = vga.draw.line_length-wrapped_len;
		
		// unwrapped chunk: to top of memory block
		for(Bitu i = 0; i < unwrapped_len; i++)
			temps[i]=vga.dac.xlat16[ret[i]];
		
		// wrapped chunk: from base of memory block
		for(Bitu i = 0; i < wrapped_len; i++)
			temps[i + unwrapped_len]=vga.dac.xlat16[vga.draw.linear_base[i]];

	} else {
		for(Bitu i = 0; i < vga.draw.line_length; i++) {
			temps[i]=vga.dac.xlat16[ret[i]];
		}
	}
	return TempLine;
}

//Test version, might as well keep it
/* static Bit8u * VGA_Draw_Chain_Line(Bitu vidstart, Bitu line) {
	Bitu i = 0;
	for ( i = 0; i < vga.draw.width;i++ ) {
		Bitu addr = vidstart + i;
		TempLine[i] = vga.mem.linear[((addr&~3)<<2)+(addr&3)];
	}
	return TempLine;
} */

static Bit8u * VGA_Draw_VGA_Line_HWMouse( Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		// HW Mouse not enabled, use the tried and true call
		return &vga.mem.linear[vidstart];

	Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		// the mouse cursor *pattern* is not on this line
		return &vga.mem.linear[ vidstart ];
	} else {
		// Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
		// 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
		// This is used when the mouse cursor partially leaves the screen.
		// It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
		// AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);
		// the index of the bit inside the cursor bitmap we start at:
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
		// convert to video memory addr and bit index
		// start adjusted to the pattern structure (thus shift address by 2 instead of 3)
		// Need to get rid of the third bit, so "/8 *2" becomes ">> 2 & ~1"
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		// stay at the right position in the pattern
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit8u* xat = &TempLine[vga.s3.hgc.originx]; // mouse data start pos. in scanline
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0; // only the first byte has some bits cut off
				if (bitsA&bit) {
					if (bitsB&bit) *xat ^= 0xFF; // Invert screen data
					//else Transparent
				} else if (bitsB&bit) {
					*xat = vga.s3.hgc.forestack[0]; // foreground color
				} else {
					*xat = vga.s3.hgc.backstack[0];
				}
				xat++;
			}
		}
		return TempLine;
	}
}

static Bit8u * VGA_Draw_LIN16_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 1) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		return &vga.mem.linear[vidstart];
	} else {
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*2);
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
 		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit16u* xat = &((Bit16u*)TempLine)[vga.s3.hgc.originx];
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0;
				if (bitsA&bit) {
					// byte order doesn't matter here as all bits get flipped
					if (bitsB&bit) *xat ^= ~0U;
					//else Transparent
				} else if (bitsB&bit) {
					// Source as well as destination are Bit8u arrays, 
					// so this should work out endian-wise?
					*xat = *(Bit16u*)vga.s3.hgc.forestack;
				} else {
					*xat = *(Bit16u*)vga.s3.hgc.backstack;
				}
				xat++;
			}
		}
		return TempLine;
	}
}

static Bit8u * VGA_Draw_LIN32_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 2) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		return &vga.mem.linear[ vidstart ];
	} else {
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*4);
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((Bit32u)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		Bit32u* xat = &((Bit32u*)TempLine)[vga.s3.hgc.originx];
		for (Bitu m = cursorMemStart; m < cursorMemEnd; (m&1)?(m+=3):m++) {
			// for each byte of cursor data
			Bit8u bitsA = vga.mem.linear[m];
			Bit8u bitsB = vga.mem.linear[m+2];
			for (Bit8u bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) { // for each bit
				cursorStartBit=0;
				if (bitsA&bit) {
					if (bitsB&bit) *xat ^= ~0U;
					//else Transparent
				} else if (bitsB&bit) {
					*xat = *(Bit32u*)vga.s3.hgc.forestack;
				} else {
					*xat = *(Bit32u*)vga.s3.hgc.backstack;
				}
				xat++;
			}
		}
		return TempLine;
	}
}

static const Bit8u* VGA_Text_Memwrap(Bitu vidstart) {
	vidstart &= vga.draw.linear_mask;
	Bitu line_end = 2 * vga.draw.blocks;
	if (GCC_UNLIKELY((vidstart + line_end) > vga.draw.linear_mask)) {
		// wrapping in this line
		Bitu break_pos = (vga.draw.linear_mask - vidstart) + 1;
		// need a temporary storage - TempLine/2 is ok for a bit more than 132 columns
		memcpy(&TempLine[sizeof(TempLine)/2], &vga.tandy.draw_base[vidstart], break_pos);
		memcpy(&TempLine[sizeof(TempLine)/2 + break_pos],&vga.tandy.draw_base[0], line_end - break_pos);
		return &TempLine[sizeof(TempLine)/2];
	} else return &vga.tandy.draw_base[vidstart];
}

static Bit32u FontMask[2]={0xffffffff,0x0};
static Bit8u * VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit32u * draw=(Bit32u *)TempLine;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);
	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu col=vidmem[cx*2+1];
		Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
		Bit32u mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
		Bit32u mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
		Bit32u fg=TXT_FG_Table[col&0xf];
		Bit32u bg=TXT_BG_Table[col>>4];
		*draw++=(fg&mask1) | (bg&~mask1);
		*draw++=(fg&mask2) | (bg&~mask2);
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x10)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit32u att=TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf];
		*draw++=att;*draw++=att;
	}
skip_cursor:
	return TempLine;
}

static Bit8u * VGA_TEXT_Herc_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit32u * draw=(Bit32u *)TempLine;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);

	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu attrib=vidmem[cx*2+1];
		if (!(attrib&0x77)) {
			// 00h, 80h, 08h, 88h produce black space
			*draw++=0;
			*draw++=0;
		} else {
			Bit32u bg, fg;
			bool underline=false;
			if ((attrib&0x77)==0x70) {
				bg = TXT_BG_Table[0x7];
				if (attrib&0x8) fg = TXT_FG_Table[0xf];
				else fg = TXT_FG_Table[0x0];
			} else {
				if (((Bitu)(vga.crtc.underline_location&0x1f)==line) && ((attrib&0x77)==0x1)) underline=true;
				bg = TXT_BG_Table[0x0];
				if (attrib&0x8) fg = TXT_FG_Table[0xf];
				else fg = TXT_FG_Table[0x7];
			}
			Bit32u mask1, mask2;
			if (GCC_UNLIKELY(underline)) mask1 = mask2 = FontMask[attrib >> 7];
			else {
				Bitu font=vga.draw.font_tables[0][chr*32+line];
				mask1=TXT_Font_Table[font>>4] & FontMask[attrib >> 7]; // blinking
				mask2=TXT_Font_Table[font&0xf] & FontMask[attrib >> 7];
			}
			*draw++=(fg&mask1) | (bg&~mask1);
			*draw++=(fg&mask2) | (bg&~mask2);
		}
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x10)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit8u attr = vga.tandy.draw_base[vga.draw.cursor.address+1];
		Bit32u cg;
		if (attr&0x8) {
			cg = TXT_FG_Table[0xf];
		} else if ((attr&0x77)==0x70) {
			cg = TXT_FG_Table[0x0];
		} else {
			cg = TXT_FG_Table[0x7];
		}
		*draw++=cg;*draw++=cg;
	}
skip_cursor:
	return TempLine;
}

//for PS/55
//The character mode addresses video memory between E0000h and E0FFFh.
// [Character drawing]
// SBCS:
//   1 2   ...       13
// 1  |   H.Grid       
//    |----------------
// 2  |    Space
// 3 V|
//   .|
//   G|  Font Pattern
//   r| (12x24 pixels)
//   i|
//   d|
//26  |________________
//27       Space       
//     ---------------- 
//28     Underscore    \
//     ---------------- Cursor Position
//29                   /
//
// DBCS:
//   1 2   ...    13 1 2  ...  12 13
// 1  |   H.Grid    |   H.Grid   |
//   -|--------------------------|-
// 2  |    Space    |    Space   |
//   -|--------------------------|-
// 3 V|             |            |S
//   .|             |            |p
//   G|        Font Pattern      |a
//   r|       (24x24 pixels)     |c
//   i|             |            |e
//   d|             |            |
//26  |_____________|____________|
//27  |    Space    |    Space
//   ------------------------------
//28  |  Underscore |  Underscore  \
//   ------------------------------ Cursor Position
//29  |             |              /
//
// [Attributes]
// Int 10h Video mode 08h:
//  7      6      5      4      3      2      1      0
//  Blink |Under |HGrid |VGrid |HLight|Revers|FntSet|DBCS/SBCS|
// 
// Int 10h Video mode 0Eh:
//  -Blue |-Green|HGrid |VGrid |-Red  |Revers|FntSet|DBCS/SBCS|
// 
// Bit 1 switches the font bank to the Extended SBCS. (In DOS K3.x, it contains APL characters.)
// DOS Bunsho Program transfers 1/2 and 1/4 fonts fron the font ROM to the Extended SBCS.
// Some applications set the bit 1 as the column is right half of DBCS,
// but it does not make any mean. (according to the IBM Technical Reference.)
// 
//[Font ROM Map (DA2)]
//Bank 0
// 4800-  *
//Bank 4
//  *  -0DB6Fh ( 4800-8DB6Fh;IBMJ 100-1F7Dh) : DBCS (24 x 24)
//10000-16D1Fh (90000-96D1Fh;IBMJ 2000-2183h) : IBM Extended Characters
//18000-1BFFFh (98000-9BFFFh;around IBMJ 21C7-22AAh) : Basic SBCS (13 x 30)
//1FD20-  *
//Bank 5
//  *  -0D09Fh (9FD20-AD09Fh;IBMJ 2384-2673h) : Gaiji 752 chs (maybe blank)
//10000-13FFFh (B0000-B3FFFh;around IBMJ 271C-27FFh) : Extended SBCS (13 x 30)
//14000-146FFh (B4000-B46FFh;IBMJ 2800-2818h) : Half-width box drawing characters (7 lines * 4 parts * 64 bytes) used by DOS Bunsho
//             (B9580-?;IBMJ 2930-295e?) : Full-width box drawing characters
//
//[Gaiji RAM Map (DA1)]
// 00000-2FFFFh: Font (0-5FFh) 1536 chs, 128 bytes / ch
// 30000-37FFFh: Extended SBCS(00-FFh, ATTR bit 1 = on)
// 38000-3FFFFh: Basic SBCS(00-FFh, ATTR bit 1 = off) 256 chs, 128 bytes / ch
// 
//[Gaiji RAM Map (DA2)]
// Bank 0 00000-1FFFFh placed between A0000h-BFFFFh
// 00000-1F7FFh(A0000-BF7FFh): Gaiji Temporary (Kuten 103-114 ku,IBM: 2674-2ADBh) 1008 chs 128 bytes
// 1F800-1FFFFh(BF800-BFFFFh): Gaiji Resident (SJIS: F040-F04Fh, IBM: 2384-2393h) 16 chs
// 
// Bank 1 20000-3FFFFh placed between A0000h-BFFFFh
// 20000-33FFFh(A0000-B3FFFh): Gaiji Resident (SJIS: F050-F39Ch, IBM: 2394-2613h) 640 chs
// 34000-37FFFh(B4000-B7FFFh): Basic SBCS(00-FFh, ATTR bit 1 = off)
// 38000-3AFFFh(B8000-BAFFFh): Gaiji Resident (SJIS: F39D-F3FCh, IBM: 2614-2673h) 96 chs
// 3C000-3FFFFh(BC000-BFFFFh): Extended SBCS(00-FFh, ATTR bit 1 = on)
//
//[IBM to Gaiji conv tbl]
// A     B     C
// 2ADC, 2C5F, +5524  --> 8000
// 2614, 2673, +90EC  --> B700
// 2384, 2613, +906C  --> B3F0
// 0682, 1F7D, +0000
// 
// 8000 - 8183h 184h(388 characters) IBM Extended Characters
// B3F0 - B75Fh 370h(880 characters) User-defined Characters

//Get character line pattern from jfont array or gaiji volatile memory
Bitu getfont_ps55dbcs(Bitu code, Bitu line) {
	Bitu font = 0;
	Bits fline = line - 2;//Start line of drawing character (line >= 1 AND line < 24 + 1 )
	// ----��for DA1 ----
	//if (code < 1536) {
	//	code *= 0x80;
	//	font = ps55.gaiji_ram[code + line * 4];
	//	font <<= 8;
	//	font |= ps55.gaiji_ram[code + line * 4 + 1];
	//	font <<= 8;
	//	font |= ps55.gaiji_ram[code + line * 4 + 2];
	//	font <<= 8;
	//	font |= ps55.gaiji_ram[code + line * 4 + 3];
	//}

	//return font;
	// ----��for DA2----
	if (code >= 0x8000 && code <= 0x8183) code -= 0x6000;//shifts code for IBM extended characters (I don't know how the real card works.)
	if (code < DBCS24_CHARS && fline >= 0 && fline < 24) {
		font = ps55font_24[code * 72 + fline * 3];				//1111 1111
		font <<= 8;													//1111 1111 0000 0000
		font |= ps55font_24[code * 72 + fline * 3 + 1] & 0xf0;	//1111 1111 2222 0000
		font >>= 1;													//0111 1111 1222 2000
		font <<= 4;													//0111 1111 1222 2000 0000
		font |= ps55font_24[code * 72 + fline * 3 + 1] & 0x0f;	//0111 1111 1222 2000 2222
		font <<= 8;													//0111 1111 1222 2000 2222 0000 0000
		font |= ps55font_24[code * 72 + fline * 3 + 2];			//0111 1111 1222 2000 2222 3333 3333
		font <<= 4;													//0111 1111 1222 2000 2222 3333 3333 0000
		//font >>= 1;//put blank at column 1 (and 26)
	}
	else if (code >= 0xb000 && code <= 0xb75f)
	{
		//convert code->address in gaiji memory
		code -= 0xb000;
		code *= 0x80;
		//code += 0xf800;
		font = ps55.gaiji_ram[code + line * 4];
		font <<= 8;
		font |= ps55.gaiji_ram[code + line * 4 + 1];
		font <<= 8;
		font |= ps55.gaiji_ram[code + line * 4 + 2];
		font <<= 8;
		font |= ps55.gaiji_ram[code + line * 4 + 3];
	}
	//else if (code >= 0x8000 && code <= 0x8183)
	//{
	//	code -= 0x8000;
	//	code *= 0x80;
	//	code += 0x10000;
	//	font = ps55.gaiji_ram[code + line * 4];
	//	font <<= 8;
	//	font |= ps55.gaiji_ram[code + line * 4 + 1];
	//	font <<= 8;
	//	font |= ps55.gaiji_ram[code + line * 4 + 2];
	//	font <<= 8;
	//	font |= ps55.gaiji_ram[code + line * 4 + 3];
	//}
	else if (code > DBCS24_CHARS)
		font = 0xffffffff;
	else
		font = 0;
	return font;
}

//Reverse the bit order of attribute code IRGB to BGRI (used in Mode 3 and Cursor Color)
inline Bit8u IRGBtoBGRI(Bit8u attr)
{
	attr = (attr & 0x01) << 7 | (attr & 0x02) << 5 | (attr & 0x04) << 3 | (attr & 0x08) << 1;
	return attr >>= 4;
}
//Get the foreground color from the attribute byte
inline Bit8u getPS55ForeColor(Bit8u attr)
{
	Bit8u foreground = ~attr & 0x08;// 0000 1000
	foreground <<= 2; //0010 0000
	foreground |= ~attr & 0xc0;// 1110 0000
	foreground >>= 4;//0000 1110
	if (ps55.attr_mode & 0x40) foreground |= 0x01;
	return foreground;
}
//Display Adapter Mode 8, E Drawing
static Bit8u* VGA_TEXT_PS55_Draw_Line(Bitu vidstart, Bitu line) {
	Bit8u* draw = ((Bit8u*)TempLine);
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart); // pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;
	Bitu background, foreground, tempcolor;
	Bitu chr_dbcs;
	bool chr_wide = false;

	if (vga.attr.disabled & 0x02) {
		// Displays the background color (palette 0) when screen is disabled
		memset(TempLine, 0, sizeof(TempLine));
		return TempLine;
	}

	while (blocks > 0) { // for each characters in a line
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		if (ps55.attr_mode & 0x80)//IO 3E8h, Index 3Dh
		{//--Parse attribute byte in color mode--
			background = 0;//fixed black (the only way to change background color is programming PAL)
			foreground = getPS55ForeColor(attr);
			if (attr & 0x04) {//reverse 0000 0100
				background = foreground;
				foreground = 0;
			}
		}
		else
		{//--Parse attribute byte in monochrome mode--
			if (attr & 0x08) foreground = 3;//Highlight 0000 1000
			else foreground = 2;
			background = 0;//Background is always color #0 (default is black)
			if (!(~attr & 0xCC))//Invisible 11xx 11xx -> 00xx 00xx 
			{
				foreground = background;
				attr &= 0x33;//disable blinkking, underscore, highlight and reverse
			}
			if (attr & 0x04) {//reverse 0000 0100
				background = foreground;
				foreground = 0;
			}
			// Blinking 1000 0000
			foreground = (vga.draw.blink || (!(attr & 0x80))) ? foreground : background;
			// choose foreground color if blinking not set for this cell or blink on
			//if (vga.draw.blinking) background &= ~0x8;
		}
		//foreground = vga.attr.palette[foreground];
		//vga.attr.palette[background];
		//Draw character
		for (Bitu n = 0; n < 13; n++) *draw++ = background;//draw blank
		draw -= 13;
		// SBCS or DBCS left half
		if (chr_wide == false) {
			if (attr & 0x01) chr_wide = true;
			// Stay drawing If the char code is DBCS and not at last column.
			if (chr_wide) {
				//Get high DBCS code from the next video address
				chr_dbcs = *vidmem;
				chr_dbcs <<= 8;
				chr_dbcs |= chr;
				// Get the font pattern
				Bitu font = getfont_ps55dbcs(chr_dbcs, line);
				// Draw 13 dots
				for (Bitu n = 0; n < 13; n++) {
					*draw++ = (font & 0x80000000) ? foreground : background;
					font <<= 1;
				}
				draw -= 13;
			}
			else {
				// the char code is SBCS (ANK)
				// Get the font pattern
				Bitu fontbase;
				// ----��for DA1----
				//if (attr & 0x02)//second map of SBCS font
				//	fontbase = 0x30000;
				//else
				//	fontbase = 0x38000;
				//Bit16u font = ps55.gaiji_ram[fontbase + chr * 128 + line * 4];// w13xh29 font
				//font <<= 8;
				//font |= ps55.gaiji_ram[fontbase + chr * 128 + line * 4 + 1];// w13xh29 font
				// ----��for DA2----
				if (attr & 0x02)//second map of SBCS font
					fontbase = 0x3c000;
				else
					fontbase = 0x34000;
				Bit16u font = ps55.gaiji_ram[fontbase + chr * 0x40 + line * 2];// w13xh29 font
				font <<= 8;
				font |= ps55.gaiji_ram[fontbase + chr * 0x40 + line * 2 + 1];// w13xh29 font
				// -----------------
				// Draw 13 dots
				for (Bitu n = 0; n < 13; n++) {
					*draw++ = (font & 0x8000) ? foreground : background;
					font <<= 1;
				}
				draw -= 13;
			}
		}
		// right half of DBCS
		else
		{
			Bitu font = getfont_ps55dbcs(chr_dbcs, line);
			// Draw 13 dots
			for (Bitu n = 0; n < 13; n++) {
				*draw++ = (font & 0x8000) ? foreground : background;
				font <<= 1;
			}
			draw -= 13;
			chr_wide = false;
		}

		//Line 28 (Underscore) Note: Draw this first to display blink + vertical + underline correctly.
		if (line == ps55.underscore_loc && attr & 0x40 && ~ps55.attr_mode & 0x80) {//Underscore only in monochrome mode
			for (Bitu n = 0; n < 13; n++)
				*draw++ = foreground;//under line (white)
			draw -= 13;
		}
		//Column 1 (Vertical Line)
		if (attr & 0x10) {
			*draw++ = (ps55.attr_mode & 0x80) ? IRGBtoBGRI(ps55.palette_line) : 2;//vertical line (white)
			draw--;
		}
		if (line == 0 && attr & 0x20 && ~ps55.attr_mode) {//HGrid
			for (Bitu n = 0; n < 13; n++)
				*draw++ = (ps55.attr_mode & 0x80) ? IRGBtoBGRI(ps55.palette_line) : 2;//horizontal line (white)
			draw -= 13;
		}
		////for debug
		//*draw++ = 2;//vertical line (blue)
		//draw--;
		//if (line == 0 && ~ps55.attr_mode) {//HGrid
		//	for (Bitu n = 0; n < 13; n++)
		//		*draw++ = 2;//horizontal line (blue)
		//	draw -= 13;
		//}
		draw += 13;
		blocks--;// Move to the next column
	}
	//draw the text mode cursor if needed (PS/55, blink speed and color need modify
	if (((vga.draw.cursor.count | 1) & ps55.cursor_blinkspeed) && (line >= vga.draw.cursor.sline) &&
		(line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
		// the address of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address - vidstart) >> 1;
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
			Bitu index = attr_addr * 13;
			draw = (Bit8u*)(&TempLine[index]);
			Bit8u cursorwidth = (ps55.cursor_wide ? 26 : 13);
			Bit8u cursorcolor = (ps55.attr_mode & 0x80) ? IRGBtoBGRI(ps55.cursor_color) : 2;//Color 2 if mode 8
			Bit8u attr = vga.tandy.draw_base[vga.draw.cursor.address + 1];
			foreground = (ps55.attr_mode & 0x80) ? getPS55ForeColor(attr) : (attr & 0x08) ? 3 : 2;
			background = 0;
			if (attr & 0x04) {//Color 0 if reverse
				background = foreground;
				foreground = 0;
			}
			for (Bitu i = 0; i < cursorwidth; i++) {
				if (vga.attr.palette[*draw] == vga.attr.palette[cursorcolor] || vga.attr.palette[background] == vga.attr.palette[cursorcolor])
					*draw++ = (vga.attr.palette[*draw] == vga.attr.palette[background]) ? foreground : background;//PEo
				else
					*draw++ = (*draw == background) ? cursorcolor : *draw;//PEx VZo MIFESo DBo
			}
		}
	}
	return TempLine;
}
//Display Adapter VGA compatible DA drawing (Undocumented?)
static Bit8u* VGA_TEXT_PS55_Mode3_Draw_Line(Bitu vidstart, Bitu line) {
	Bit8u* draw = ((Bit8u*)TempLine);
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart); // pointer to chars+attribs
	const Bit8u* extvidmem = vidmem;
	Bitu blocks = vga.draw.blocks;
	Bitu background, foreground;
	//temporary set for DBCS
	Bitu chr_dbcs;
	bool chr_wide = false;
	vidmem += 0x8000;//align to video memory base B8000h;

	if (vga.attr.disabled & 0x02) {
		// Displays the background color (palette 0) when screen is disabled
		memset(TempLine, 0, sizeof(TempLine));
		return TempLine;
	}

	while (blocks > 0) { // for each characters in a line
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		Bitu extchr = *extvidmem++;//?
		Bitu extattr = *extvidmem++;//Bit 1-0: 00=SBCS, 01=Left DBCS, 11=Right DBCS

		////Parse attribute
		background = attr >> 4; //in EGA
		//// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking) background &= ~0x8;
		foreground = (vga.draw.blink || (!(attr & 0x80))) ?	(attr & 0xf) : background;
		foreground = IRGBtoBGRI(foreground);
		background = IRGBtoBGRI(background);

		//Draw character
		for (Bitu n = 0; n < 13; n++) *draw++ = background;//draw blank
		draw -= 13;
		// SBCS or DBCS left half
		if (chr_wide == false) {
			if (extattr & 0x01) chr_wide = true;
			//if (chr_dbcs > 0x1216) chr_wide = false;
			//else if (chr_dbcs < 0x682 && chr_dbcs >= 0x400) chr_wide = false;
			//else chr_wide = true;
			// Stay drawing If the char code is DBCS and not at last column.
			if (chr_wide) {
				chr_dbcs = *vidmem;//get the next char code and determine SBCS or DBCS
				// Get high DBCS code from the next video address
				chr_dbcs <<= 8;
				chr_dbcs |= chr;
				// Get its font pattern
				Bitu font = getfont_ps55dbcs(chr_dbcs, line);
				// Draw 13 dots
				for (Bitu n = 0; n < 13; n++) {
					*draw++ = (font & 0x80000000) ? foreground : background;
					font <<= 1;
				}
				draw -= 13;
			}
			else {
				// the char code is SBCS (ANK)
				// Get its font pattern
				Bitu fontbase = 0x34000;
				Bit16u font = ps55.gaiji_ram[fontbase + chr * 0x40 + line * 2];// w13xh29 font
				font <<= 8;
				font |= ps55.gaiji_ram[fontbase + chr * 0x40 + line * 2 + 1];// w13xh29 font
				// Draw 13 dots
				for (Bitu n = 0; n < 13; n++) {
					*draw++ = (font & 0x8000) ? foreground : background;
					font <<= 1;
				}
				draw -= 13;
			}
		}
		// right half of DBCS
		else
		{
			Bitu font = getfont_ps55dbcs(chr_dbcs, line);
			// Draw 13 dots
			for (Bitu n = 0; n < 13; n++) {
				*draw++ = (font & 0x8000) ? foreground : background;
				font <<= 1;
			}
			draw -= 13;
			chr_wide = false;
		}
		draw += 13;
		blocks--;// Move to the next column
	}
	if ((line >= vga.draw.cursor.sline) &&
		(line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
		// the adress of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address - vidstart) >> 1;
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
			Bitu index = attr_addr * 13;
			draw = (Bit8u*)(&TempLine[index]);
			Bitu foreground = vga.tandy.draw_base[vga.draw.cursor.address + 1] & 0xf;
			for (Bitu i = 0; i < 8; i++) {
				*draw++ = foreground;
			}
		}
	}
	return TempLine;
}
// JEGA drawing line-----------------------------------------
static Bit8u* VGA_TEXT_JEGA_Draw_Line(Bitu vidstart, Bitu line) {
	// keep it aligned:
	Bit16u* draw = ((Bit16u*)TempLine) + 16 - vga.draw.panning;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart); // pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;
	if (vga.draw.panning) blocks++; // if the text is panned part of an 
									// additional character becomes visible
	Bitu background, foreground;
	//temporary set for DBCS
	Bitu chr_left;
	Bitu bsattr;//basic attribute (used in JEGA Attribution)
	bool chr_wide = false;

	while (blocks > 0) { // for each characters in a line
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		// SBCS or DBCS second block
		if(chr_wide == false) {
			if (!(jega.RMOD2 & 0x80))//RMOD2 bit7: First Attribute EGA/JEGA
			{//--Parse attribute byte as EGA mode--
				background = attr >> 4; //in EGA
				if (!(jega.RMOD2 & 0x20)) {//RMOD2 bit6: Select Blink or Intensity
					// choose foreground color if blinking not set for this cell or blink on
					if (vga.draw.blinking) background &= ~0x8;
					// if blinking is enabled bit7 is not mapped to attributes
					foreground = (vga.draw.blink || (!(attr & 0x80))) ?
						(attr & 0xf) : background;
				}
				else
					foreground = attr & 0x0f;
				//Clear basic attribute
				bsattr = 0;
			}
			else {//--Parse attribute byte as JEGA mode--
				//Background is always black(or transparent) in JEGA mode
				background = 0;
				foreground = (vga.draw.blink || (!(attr & 0x80))) ?
					(attr & 0xf) : background;//Blinking (bit7)
				if (attr & 0x40) {//Reverse (bit6)
					Bitu tmp = background;
					background = foreground;
					foreground = tmp;
				}
				//Set basic attribute
				bsattr = attr;
			}
			// Stay drawing If the char code is DBCS and not at last column.
			if(isKanji1(chr) && blocks > 1) {
				//Set the present char/attr code to the next loop
				chr_left=chr;
				chr_wide=true;
			}
			// the char code is ANK (8 dots width)
			else 
			{
				// Get the font pattern
				Bitu font = jfont_sbcs_19[chr*19+line];// w8xh19 font
				// Draw 8 dots
				for (Bitu n = 0; n < 8; n++) {
					*draw++ = vga.dac.xlat16[(font & 0x80) ? foreground : background];
					font <<= 1;
				}
				if (bsattr & 0x20) {
					draw -= 8;
					*draw = vga.dac.xlat16[foreground];//vertical line
					draw += 8;
				}
				if (line == 18 && bsattr & 0x10) {
					draw -= 8;
					for (Bitu n = 0; n < 8; n++)
						*draw++ = vga.dac.xlat16[foreground];//underline
				}
				chr_wide=false;
				blocks--;// Move to the next column
			}
		}
		// the char code may be in DBCS
		else
		{
			Bitu pad_y = jega.RPSSC;//Set op padding of the line
			Bitu exattr = 0;
			//Note: The second column should be applied its basic attribute.
			if (jega.RMOD2 & 0x40) {// if JEGA Extended Attribute is enabled
				exattr = attr;
				if ((exattr & 0x30) == 0x30) pad_y = jega.RPSSL;//Set top padding of lower 2x character
				else if (exattr & 0x30) pad_y = jega.RPSSU;//Set top padding of upper 2x character
			}
			if (line >= pad_y && line < 16 + pad_y) {
				// Check the char code is in Wide charset of Shift-JIS
				if (isKanji2(chr)) {
					Bitu fline = line - pad_y;//Start line of drawing 16x16 character
					chr_left <<= 8;
					// Fix vertical position
					chr |= chr_left;
					//Horizontal wide font (Extended Attribute)
					if (exattr & 0x20) {
						if (exattr & 0x10) fline = (fline >> 1) + 8;
						else fline = fline >> 1;
					}
					//Vertical wide font (Extended Attribute)
					if (exattr & 0x40) {
						// Get the font pattern
						Bitu font = jfont_dbcs_16[chr * 32 + fline * 2];
						if (!(exattr & 0x08))
							font = jfont_dbcs_16[chr * 32 + fline * 2 + 1];
						// Draw 8 dots
						for (Bitu n = 0; n < 8; n++) {
							*draw++ = vga.dac.xlat16[(font & 0x80) ? foreground : background];
							*draw++ = vga.dac.xlat16[(font & 0x80) ? foreground : background];
							font <<= 1;
						}
					}
					else {
						// Get the font pattern
						Bitu font = jfont_dbcs_16[chr * 32 + fline * 2];
						font <<= 8;
						font |= jfont_dbcs_16[chr * 32 + fline * 2 + 1];
						//Bold (Extended Attribute)
						if (exattr &= 0x80)
						{
							Bitu font2 = font;
							font2 >>= 1;
							font |= font2;
							//Original JEGA colours the last row with the next column's attribute.
						}
						// Draw 16 dots
						for (Bitu n = 0; n < 16; n++) {
							*draw++ = vga.dac.xlat16[(font & 0x8000) ? foreground : background];
							font <<= 1;
						}
					}
				}
				// Ignore a wide char code, and put blank
				else
					for (Bitu n = 0; n < 16; n++)
						*draw++ = vga.dac.xlat16[background];
			}
			else if (line == (17 + pad_y) && (bsattr & 0x10)) {
					for (Bitu n = 0; n < 16; n++) 
						*draw++ = vga.dac.xlat16[foreground];//underline
			}
			else for (Bitu n = 0; n < 16; n++) *draw++ = vga.dac.xlat16[background];//draw blank
			if (bsattr & 0x20) {// draw vertical line at last
				draw -= 16;
				*draw = vga.dac.xlat16[foreground];
				draw += 16;
			}
			chr_wide=false;
			blocks-=2;// Move to +2 columns
		}

	}
	// draw the text mode cursor if needed

	if ((vga.draw.cursor.count & 0x10) && (line >= vga.draw.cursor.sline) &&
		(line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
		// the adress of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address - vidstart) >> 1;
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
			Bitu index = attr_addr * (vga.draw.char9dot ? 18 : 16);
			draw = (Bit16u*)(&TempLine[index]) + 16 - vga.draw.panning;

			Bitu foreground = vga.tandy.draw_base[vga.draw.cursor.address + 1] & 0xf;
			for (Bitu i = 0; i < 8; i++) {
				*draw++ = vga.dac.xlat16[foreground];
			}
		}
	}
	return TempLine + 32;
}

/*
// combined 8/9-dot wide text mode 8bpp line drawing function
static Bit8u* VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
	// keep it aligned:
	Bit8u* draw = ((Bit8u*)TempLine) + 16 - vga.draw.panning;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart); // pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;
	if (vga.draw.panning) blocks++; // if the text is panned part of an 
									// additional character becomes visible
	while (blocks--) { // for each character in the line
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		// the font pattern
		Bitu font = vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];
		
		Bitu background = attr >> 4;
		// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking) background &= ~0x8;
		// choose foreground color if blinking not set for this cell or blink on
		Bitu foreground = (vga.draw.blink || (!(attr&0x80)))?
			(attr&0xf):background;
		// underline: all foreground [freevga: 0x77, previous 0x7]
		if (GCC_UNLIKELY(((attr&0x77) == 0x01) &&
			(vga.crtc.underline_location&0x1f)==line))
				background = foreground;
		if (vga.draw.char9dot) {
			font <<=1; // 9 pixels
			// extend to the 9th pixel if needed
			if ((font&0x2) && (vga.attr.mode_control&0x04) &&
				(chr>=0xc0) && (chr<=0xdf)) font |= 1;
			for (Bitu n = 0; n < 9; n++) {
				*draw++ = (font&0x100)? foreground:background;
				font <<= 1;
			}
		} else {
			for (Bitu n = 0; n < 8; n++) {
				*draw++ = (font&0x80)? foreground:background;
				font <<= 1;
			}
		}
	}
	// draw the text mode cursor if needed
	if ((vga.draw.cursor.count&0x10) && (line >= vga.draw.cursor.sline) &&
		(line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
		// the adress of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address-vidstart) >> 1;
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
			Bitu index = attr_addr * (vga.draw.char9dot? 9:8);
			draw = (Bit8u*)(&TempLine[index]) + 16 - vga.draw.panning;
			
			Bitu foreground = vga.tandy.draw_base[vga.draw.cursor.address+1] & 0xf;
			for (Bitu i = 0; i < 8; i++) {
				*draw++ = foreground;
			}
		}
	}
	return TempLine+16;
}
*/
// combined 8/9-dot wide text mode 16bpp line drawing function
static Bit8u* VGA_TEXT_Xlat16_Draw_Line(Bitu vidstart, Bitu line) {
	// keep it aligned:
	Bit16u* draw = ((Bit16u*)TempLine) + 16 - vga.draw.panning;
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart); // pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;
	if (vga.draw.panning) blocks++; // if the text is panned part of an 
									// additional character becomes visible
	while (blocks--) { // for each character in the line
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		// the font pattern
		Bitu font = vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];
		
		Bitu background = attr >> 4;
		// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking) background &= ~0x8;
		// choose foreground color if blinking not set for this cell or blink on
		Bitu foreground = (vga.draw.blink || (!(attr&0x80)))?
			(attr&0xf):background;
		// underline: all foreground [freevga: 0x77, previous 0x7]
		if (GCC_UNLIKELY(((attr&0x77) == 0x01) &&
			(vga.crtc.underline_location&0x1f)==line))
				background = foreground;
		if (vga.draw.char9dot) {
			font <<=1; // 9 pixels
			// extend to the 9th pixel if needed
			if ((font&0x2) && (vga.attr.mode_control&0x04) &&
				(chr>=0xc0) && (chr<=0xdf)) font |= 1;
			for (Bitu n = 0; n < 9; n++) {
				*draw++ = vga.dac.xlat16[(font&0x100)? foreground:background];
				font <<= 1;
			}
		} else {
			for (Bitu n = 0; n < 8; n++) {
				*draw++ = vga.dac.xlat16[(font&0x80)? foreground:background];
				font <<= 1;
			}
		}
	}
	// draw the text mode cursor if needed
	if ((vga.draw.cursor.count&0x10) && (line >= vga.draw.cursor.sline) &&
		(line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled) {
		// the adress of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address-vidstart) >> 1;
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks) {
			Bitu index = attr_addr * (vga.draw.char9dot? 18:16);
			draw = (Bit16u*)(&TempLine[index]) + 16 - vga.draw.panning;
			
			Bitu foreground = vga.tandy.draw_base[vga.draw.cursor.address+1] & 0xf;
			for (Bitu i = 0; i < 8; i++) {
				*draw++ = vga.dac.xlat16[foreground];
			}
		}
	}
	return TempLine+32;
}

#ifdef VGA_KEEP_CHANGES
static INLINE void VGA_ChangesEnd(void ) {
	if ( vga.changes.active ) {
//		vga.changes.active = false;
		Bitu end = vga.draw.address >> VGA_CHANGE_SHIFT;
		Bitu total = 4 + end - vga.changes.start;
		Bit32u clearMask = vga.changes.clearMask;
		total >>= 2;
		Bit32u *clear = (Bit32u *)&vga.changes.map[  vga.changes.start & ~3 ];
		while ( total-- ) {
			clear[0] &= clearMask;
			clear++;
		}
	}
}
#endif


static void VGA_ProcessSplit() {
	if (vga.attr.mode_control&0x20) {
		vga.draw.address=0;
		// reset panning to 0 here so we don't have to check for 
		// it in the character draw functions. It will be set back
		// to its proper value in v-retrace
		vga.draw.panning=0; 
	} else {
		// In text mode only the characters are shifted by panning, not the address;
		// this is done in the text line draw function.
		vga.draw.address = vga.draw.byte_panning_shift*vga.draw.bytes_skip;
		if ((vga.mode!=M_TEXT)&&(machine!=MCH_EGA)) vga.draw.address += vga.draw.panning;
	}
	vga.draw.address_line=0;
}

static Bit8u bg_color_index = 0; // screen-off black index
static void VGA_DrawSingleLine(Bitu /*blah*/) {
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		switch(machine) {
		case MCH_PCJR:
			// Displays the border color when screen is disabled
			bg_color_index = vga.tandy.border_color;
			break;
		case MCH_TANDY:
			// Either the PCJr way or the CGA way
			if (vga.tandy.gfx_control& 0x4) { 
				bg_color_index = vga.tandy.border_color;
			} else if (vga.mode==M_TANDY4) 
				bg_color_index = vga.attr.palette[0];
			else bg_color_index = 0;
			break;
		case MCH_CGA:
			// the background color
			bg_color_index = vga.attr.overscan_color;
			break;
		case MCH_EGA:
		case MCH_VGA:
			// DoWhackaDo, Alien Carnage, TV sports Football
			// when disabled by attribute index bit 5:
			//  ET3000, ET4000, Paradise display the border color
			//  S3 displays the content of the currently selected attribute register
			// when disabled by sequencer the screen is black "257th color"
			
			// the DAC table may not match the bits of the overscan register
			// so use black for this case too...
			//if (vga.attr.disabled& 2) {
			if (vga.dac.xlat16[bg_color_index] != 0) {
				for(Bitu i = 0; i < 256; i++)
					if (vga.dac.xlat16[i] == 0) {
						bg_color_index = i;
						break;
					}
			}
			//} else 
            //    bg_color_index = vga.attr.overscan_color;
			break;
		default:
			bg_color_index = 0;
			break;
		}
		if (vga.draw.bpp==8) {
			memset(TempLine, bg_color_index, sizeof(TempLine));
		} else if (vga.draw.bpp==16) {
			Bit16u* wptr = (Bit16u*) TempLine;
			Bit16u value = vga.dac.xlat16[bg_color_index];
			for (Bitu i = 0; i < sizeof(TempLine)/2; i++) {
				wptr[i] = value;
			}
		}
		RENDER_DrawLine(TempLine);
	} else {
		Bit8u * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );	
		RENDER_DrawLine(data);
	}

	vga.draw.address_line++;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	vga.draw.lines_done++;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawSingleLine,(float)vga.draw.delay.htotal);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawEGASingleLine(Bitu /*blah*/) {
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		memset(TempLine, 0, sizeof(TempLine));
		RENDER_DrawLine(TempLine);
	} else {
		Bitu address = vga.draw.address;
		if (vga.mode!=M_TEXT) address += vga.draw.panning;
		Bit8u * data=VGA_DrawLine(address, vga.draw.address_line );	
		RENDER_DrawLine(data);
	}

	vga.draw.address_line++;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	vga.draw.lines_done++;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawEGASingleLine,(float)vga.draw.delay.htotal);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawPart(Bitu lines) {
	while (lines--) {
		Bit8u * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		RENDER_DrawLine(data);
		vga.draw.address_line++;
		if (vga.draw.address_line>=vga.draw.address_line_total) {
			vga.draw.address_line=0;
			vga.draw.address+=vga.draw.address_add;
		}
		vga.draw.lines_done++;
		if (vga.draw.split_line==vga.draw.lines_done) {
#ifdef VGA_KEEP_CHANGES
			VGA_ChangesEnd( );
#endif
			VGA_ProcessSplit();
#ifdef VGA_KEEP_CHANGES
			vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
#endif
		}
	}
	if (--vga.draw.parts_left) {
		PIC_AddEvent(VGA_DrawPart,(float)vga.draw.delay.parts,
			 (vga.draw.parts_left!=1) ? vga.draw.parts_lines  : (vga.draw.lines_total - vga.draw.lines_done));
	} else {
#ifdef VGA_KEEP_CHANGES
		VGA_ChangesEnd();
#endif
		RENDER_EndUpdate(false);
	}
}

void VGA_SetBlinking(Bitu enabled) {
	Bitu b;
	LOG(LOG_VGA,LOG_NORMAL)("Blinking %d",enabled);
	if (enabled) {
		b=0;vga.draw.blinking=1; //used to -1 but blinking is unsigned
		vga.attr.mode_control|=0x08;
		vga.tandy.mode_control|=0x20;
	} else {
		b=8;vga.draw.blinking=0;
		vga.attr.mode_control&=~0x08;
		vga.tandy.mode_control&=~0x20;
	}
	for (Bitu i=0;i<8;i++) TXT_BG_Table[i+8]=(b+i) | ((b+i) << 8)| ((b+i) <<16) | ((b+i) << 24);
}

#ifdef VGA_KEEP_CHANGES
static void INLINE VGA_ChangesStart( void ) {
	vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
	vga.changes.last = vga.changes.start;
	if ( vga.changes.lastAddress != vga.draw.address ) {
//		LOG_MSG("Address");
		VGA_DrawLine = VGA_Draw_Linear_Line;
		vga.changes.lastAddress = vga.draw.address;
	} else if ( render.fullFrame ) {
//		LOG_MSG("Full Frame");
		VGA_DrawLine = VGA_Draw_Linear_Line;
	} else {
//		LOG_MSG("Changes");
		VGA_DrawLine = VGA_Draw_Changes_Line;
	}
	vga.changes.active = true;
	vga.changes.checkMask = vga.changes.writeMask;
	vga.changes.clearMask = ~( 0x01010101 << (vga.changes.frame & 7));
	vga.changes.frame++;
	vga.changes.writeMask = 1 << (vga.changes.frame & 7);
}
#endif

static void VGA_VertInterrupt(Bitu /*val*/) {
	if ((!vga.draw.vret_triggered) && ((vga.crtc.vertical_retrace_end&0x30)==0x10)) {
		vga.draw.vret_triggered=true;
		if (GCC_UNLIKELY(machine==MCH_EGA)) PIC_ActivateIRQ(9);
	}
}

static void VGA_Other_VertInterrupt(Bitu val) {
	if (val) PIC_ActivateIRQ(5);
	else PIC_DeActivateIRQ(5);
}

static void VGA_DisplayStartLatch(Bitu /*val*/) {
	vga.config.real_start=vga.config.display_start & (vga.vmemwrap-1);
	vga.draw.bytes_skip = vga.config.bytes_skip;
}
 
static void VGA_PanningLatch(Bitu /*val*/) {
	vga.draw.panning = vga.config.pel_panning;
}

static void VGA_VerticalTimer(Bitu /*val*/) {
	vga.draw.delay.framestart = PIC_FullIndex();
	PIC_AddEvent( VGA_VerticalTimer, (float)vga.draw.delay.vtotal );
	
	switch(machine) {
	case MCH_PCJR:
	case MCH_TANDY:
		// PCJr: Vsync is directly connected to the IRQ controller
		// Some earlier Tandy models are said to have a vsync interrupt too
		PIC_AddEvent(VGA_Other_VertInterrupt, (float)vga.draw.delay.vrstart, 1);
		PIC_AddEvent(VGA_Other_VertInterrupt, (float)vga.draw.delay.vrend, 0);
		// fall-through
	case MCH_CGA:
	case MCH_HERC:
		// MC6845-powered graphics: Loading the display start latch happens somewhere
		// after vsync off and before first visible scanline, so probably here
		VGA_DisplayStartLatch(0);
		break;
	case MCH_VGA:
		PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
		PIC_AddEvent(VGA_PanningLatch, (float)vga.draw.delay.vrend);
		// EGA: 82c435 datasheet: interrupt happens at display end
		// VGA: checked with scope; however disabled by default by jumper on VGA boards
		// add a little amount of time to make sure the last drawpart has already fired
		PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
		break;
	case MCH_EGA:
		PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrend);
		PIC_AddEvent(VGA_VertInterrupt,(float)(vga.draw.delay.vdend + 0.005));
		break;
	default:
		E_Exit("This new machine needs implementation in VGA_VerticalTimer too.");
		break;
	}

	//Check if we can actually render, else skip the rest (frameskip)
	vga.draw.cursor.count++; // Do this here, else the cursor speed depends on the frameskip
	if (vga.draw.vga_override || !RENDER_StartUpdate())
		return;

	vga.draw.address_line = vga.config.hlines_skip;
	if (IS_EGAVGA_ARCH) {
		vga.draw.split_line = (Bitu)((vga.config.line_compare+1)/vga.draw.lines_scaled);
		if ((svgaCard==SVGA_S3Trio) && (vga.config.line_compare==0)) vga.draw.split_line=0;
		vga.draw.split_line -= vga.draw.vblank_skip;
	} else {
		vga.draw.split_line = 0x10000;	// don't care
	}
	vga.draw.address = vga.config.real_start;
	vga.draw.byte_panning_shift = 0;
	// go figure...
	if (machine==MCH_EGA) {
		if (vga.draw.doubleheight) // Spacepigs EGA Megademo
			vga.draw.split_line*=2;
		vga.draw.split_line++; // EGA adds one buggy scanline
	}
//	if (machine==MCH_EGA) vga.draw.split_line = ((((vga.config.line_compare&0x5ff)+1)*2-1)/vga.draw.lines_scaled);
#ifdef VGA_KEEP_CHANGES
	bool startaddr_changed=false;
#endif
	switch (vga.mode) {
	case M_EGA:
		if (!(vga.crtc.mode_control&0x1)) vga.draw.linear_mask &= ~0x10000;
		else vga.draw.linear_mask |= 0x10000;
	case M_LIN4:
		vga.draw.byte_panning_shift = 8;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		if (machine!=MCH_EGA) vga.draw.address += vga.draw.panning;
#ifdef VGA_KEEP_CHANGES
		startaddr_changed=true;
#endif
		break;
	case M_VGA:
		if (vga.config.compatible_chain4 && (vga.crtc.underline_location & 0x40)) {
			vga.draw.linear_base = vga.fastmem;
			vga.draw.linear_mask = 0xffff;
		} else {
			vga.draw.linear_base = vga.mem.linear;
			vga.draw.linear_mask = vga.vmemwrap - 1;
		}
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		vga.draw.byte_panning_shift = 4;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.address += vga.draw.panning;
#ifdef VGA_KEEP_CHANGES
		startaddr_changed=true;
#endif
		break;
	case M_PS55_GFX:
		vga.draw.byte_panning_shift = 8;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.address += vga.draw.panning;
		//vga.draw.address = (vga.draw.address * 2) & 0x1ffff;
		//vga.draw.address &= 0x1ffff;
		//vga.draw.address += vga.draw.bytes_skip;
		//vga.draw.linear_base = vga.fastmem;
		//vga.draw.linear_base = vga.mem.linear;
		//vga.draw.linear_mask = vga.vmemwrap - 1;
		break;
	case M_PS55_GFX_MONO:
		vga.draw.linear_base = vga.mem.linear;
		vga.draw.linear_mask = 0x1ffff;
		break;
	case M_PS55_TEXT:
		//vga.draw.byte_panning_shift = 1;
		//vga.draw.address += vga.draw.bytes_skip;
		//vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.linear_mask = 0xfff; // 1 page
		vga.draw.cursor.address = vga.config.cursor_start*2;
		vga.draw.address *= 2;
		vga.draw.blink = (0x18 & vga.draw.cursor.count) ? true : false;//0.6s visible + 0.2s invisible
		//vga.draw.blink = true;
		break;
	case M_PS55_M3TEXT:
		vga.draw.linear_mask = 0xfff; // 1 page
		vga.draw.cursor.address = vga.config.cursor_start * 2;
		vga.draw.address *= 2;
		vga.draw.blink = ((vga.draw.blinking & (vga.draw.cursor.count >> 4))
			|| !vga.draw.blinking) ? true : false;
		break;
	case M_TEXT:
		vga.draw.byte_panning_shift = 2;
		vga.draw.address += vga.draw.bytes_skip;
		// fall-through
	case M_TANDY_TEXT:
	case M_HERC_TEXT:
		if (machine==MCH_HERC) vga.draw.linear_mask = 0xfff; // 1 page
		else if (IS_EGAVGA_ARCH) vga.draw.linear_mask = 0x7fff; // 8 pages
		else vga.draw.linear_mask = 0x3fff; // CGA, Tandy 4 pages
		vga.draw.cursor.address=vga.config.cursor_start*2;
		vga.draw.address *= 2;
		//vga.draw.cursor.count++; //Moved before the frameskip test.
		/* check for blinking and blinking change delay */
		FontMask[1]=(vga.draw.blinking & (vga.draw.cursor.count >> 4)) ?
			0 : 0xffffffff;
		/* if blinking is enabled, 'blink' will toggle between true
		 * and false. Otherwise it's true */
		vga.draw.blink = ((vga.draw.blinking & (vga.draw.cursor.count >> 4))
			|| !vga.draw.blinking) ? true:false;
		break;
	case M_HERC_GFX:
	case M_CGA4:case M_CGA2:
		vga.draw.address=(vga.draw.address*2)&0x1fff;
		break;
	case M_CGA16:
	case M_TANDY2:case M_TANDY4:case M_TANDY16:
		vga.draw.address *= 2;
		break;
	default:
		break;
	}
	if (GCC_UNLIKELY(vga.draw.split_line==0)) VGA_ProcessSplit();
#ifdef VGA_KEEP_CHANGES
	if (startaddr_changed) VGA_ChangesStart();
#endif

	// check if some lines at the top off the screen are blanked
	float draw_skip = 0.0;
	if (GCC_UNLIKELY(vga.draw.vblank_skip)) {
		draw_skip = (float)(vga.draw.delay.htotal * vga.draw.vblank_skip);
		vga.draw.address += vga.draw.address_add * (vga.draw.vblank_skip/(vga.draw.address_line_total));
	}

	// add the draw event
	switch (vga.draw.mode) {
	case PART:
		if (GCC_UNLIKELY(vga.draw.parts_left)) {
			LOG(LOG_VGAMISC,LOG_NORMAL)( "Parts left: %d", vga.draw.parts_left );
			PIC_RemoveEvents(VGA_DrawPart);
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		vga.draw.parts_left = vga.draw.parts_total;
		PIC_AddEvent(VGA_DrawPart,(float)vga.draw.delay.parts + draw_skip,vga.draw.parts_lines);
		break;
	case DRAWLINE:
	case EGALINE:
		if (GCC_UNLIKELY(vga.draw.lines_done < vga.draw.lines_total)) {
			LOG(LOG_VGAMISC,LOG_NORMAL)( "Lines left: %d", 
				vga.draw.lines_total-vga.draw.lines_done);
			if (vga.draw.mode==EGALINE) PIC_RemoveEvents(VGA_DrawEGASingleLine);
			else PIC_RemoveEvents(VGA_DrawSingleLine);
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		if (vga.draw.mode==EGALINE)
			PIC_AddEvent(VGA_DrawEGASingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
		else PIC_AddEvent(VGA_DrawSingleLine,(float)(vga.draw.delay.htotal/4.0 + draw_skip));
		break;
	}
}

void VGA_CheckScanLength(void) {
	switch (vga.mode) {
	case M_EGA:
	case M_LIN4:
	case M_PS55_GFX:
		vga.draw.address_add=vga.config.scan_len*16;
		break;
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		vga.draw.address_add=vga.config.scan_len*8;
		break;
	case M_PS55_GFX_MONO:
		//vga.draw.address_add = 80;//CRTC 14h Offset: 50h
		vga.draw.address_add = vga.draw.blocks;
		break;
	case M_PS55_TEXT:
	case M_PS55_M3TEXT:
		vga.draw.address_add = vga.config.scan_len * 2;//CRTC 14h Offset: 50h
		break;
	case M_TEXT:
		vga.draw.address_add=vga.config.scan_len*4;
		break;
	case M_CGA2:
	case M_CGA4:
	case M_CGA16:
		vga.draw.address_add=80;
		return;
	case M_TANDY2:
		vga.draw.address_add=vga.draw.blocks/4;
		break;
	case M_TANDY4:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY16:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_GFX:
		vga.draw.address_add=vga.draw.blocks;
		break;
	default:
		vga.draw.address_add=vga.draw.blocks*8;
		break;
	}
}

void VGA_ActivateHardwareCursor(void) {
	bool hwcursor_active=false;
	if (svga.hardware_cursor_active) {
		if (svga.hardware_cursor_active()) hwcursor_active=true;
	}
	if (hwcursor_active) {
		switch(vga.mode) {
		case M_LIN32:
			VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
			break;
		case M_LIN15:
		case M_LIN16:
			VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
			break;
		default:
			VGA_DrawLine=VGA_Draw_VGA_Line_HWMouse;
		}
	} else {
		VGA_DrawLine=VGA_Draw_Linear_Line;
	}
}

void VGA_SetupDrawing(Bitu /*val*/) {
	if (vga.mode == M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		return;
	}
	// set the drawing mode
	switch (machine) {
	case MCH_CGA:
	case MCH_PCJR:
	case MCH_TANDY:
		vga.draw.mode = DRAWLINE;
		break;
	case MCH_EGA:
		// Note: The Paradise SVGA uses the same panning mechanism as EGA
		vga.draw.mode = EGALINE;
		break;
	case MCH_VGA:
		if (svgaCard == SVGA_None) {
			vga.draw.mode = DRAWLINE;
			break;
		}
		// fall-through
	default:
		vga.draw.mode = PART;
		break;
	}

	/* Calculate the FPS for this screen */
	double fps; Bitu clock;
	Bitu htotal, hdend, hbstart, hbend, hrstart, hrend;
	Bitu vtotal, vdend, vbstart, vbend, vrstart, vrend;
	Bitu vblank_skip;
	if (IS_EGAVGA_ARCH) {
		htotal = vga.crtc.horizontal_total;
		hdend = vga.crtc.horizontal_display_end;
		hbend = vga.crtc.end_horizontal_blanking & 0x1F;
		hbstart = vga.crtc.start_horizontal_blanking;
		hrstart = vga.crtc.start_horizontal_retrace;

		vtotal = vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
		vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2) << 7);
		vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5);
		vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);

		if (IS_VGA_ARCH) {
			// additional bits only present on vga cards
			htotal |= (vga.s3.ex_hor_overflow & 0x1) << 8;
			htotal += 3;
			hdend |= (vga.s3.ex_hor_overflow & 0x2) << 7;
			hbend |= (vga.crtc.end_horizontal_retrace & 0x80) >> 2;
			hbstart |= (vga.s3.ex_hor_overflow & 0x4) << 6;
			hrstart |= (vga.s3.ex_hor_overflow & 0x10) << 4;

			vtotal |= (vga.crtc.overflow & 0x20) << 4;
			vtotal |= (vga.s3.ex_ver_overflow & 0x1) << 10;
			vdend |= (vga.crtc.overflow & 0x40) << 3;
			vdend |= (vga.s3.ex_ver_overflow & 0x2) << 9;
			vbstart |= (vga.crtc.maximum_scan_line & 0x20) << 4;
			vbstart |= (vga.s3.ex_ver_overflow & 0x4) << 8;
			vrstart |= ((vga.crtc.overflow & 0x80) << 2);
			vrstart |= (vga.s3.ex_ver_overflow & 0x10) << 6;
			vbend = vga.crtc.end_vertical_blanking & 0x7f;
		}
		else { // EGA
			vbend = vga.crtc.end_vertical_blanking & 0x1f;
		}
		htotal += 2;
		vtotal += 2;
		hdend += 1;
		vdend += 1;

		hbend = hbstart + ((hbend - hbstart) & 0x3F);
		hrend = vga.crtc.end_horizontal_retrace & 0x1f;
		hrend = (hrend - hrstart) & 0x1f;

		if (!hrend) hrend = hrstart + 0x1f + 1;
		else hrend = hrstart + hrend;

		vrend = vga.crtc.vertical_retrace_end & 0xF;
		vrend = (vrend - vrstart) & 0xF;

		if (!vrend) vrend = vrstart + 0xf + 1;
		else vrend = vrstart + vrend;

		// Special case vbstart==0:
		// Most graphics cards agree that lines zero to vbend are
		// blanked. ET4000 doesn't blank at all if vbstart==vbend.
		// ET3000 blanks lines 1 to vbend (255/6 lines).
		if (vbstart != 0) {
			vbstart += 1;
			vbend = (vbend - vbstart) & 0x7f;
			if (!vbend) vbend = vbstart + 0x7f + 1;
			else vbend = vbstart + vbend;
		}
		vbend++;

		if (svga.get_clock) {
			clock = svga.get_clock();
		}
		else if(IS_AX_ARCH) {//for AX
			//CLK0 = 14.31818 MHz (XT Bus OSC)
			//CLK1 = 16.257 MHz
			//CLK2 = 25.175 MHz
			//CLK3 = Not used
			switch ((vga.misc_output >> 2) & 3) {//Clock Select 0, 1
			case 0:
				clock = 14318180;
				break;
			case 1:
				clock = 16257000;
				break;
			case 2:
				clock = 25175000;//for 480-line mode (JEGA)
				break;
			default:
				break;
			}
		}
		else {
			switch ((vga.misc_output >> 2) & 3) {
			case 0:
				clock = (machine == MCH_EGA) ? 14318180 : 25175000;
				break;
			case 1:
			default:
				clock = (machine == MCH_EGA) ? 16257000 : 28322000;
				break;
			}
		}

		/* Check for 8 for 9 character clock mode */
		if (vga.seq.clocking_mode & 1) clock /= 8; else clock /= 9;
		/* Check for pixel doubling, master clock/2 */
		if (vga.seq.clocking_mode & 0x8) {
			htotal *= 2;
		}
		vga.draw.address_line_total = (vga.crtc.maximum_scan_line & 0x1f) + 1;
		if (IS_VGA_ARCH && (svgaCard == SVGA_None) && (vga.mode == M_EGA || vga.mode == M_VGA)) {
			// vgaonly; can't use with CGA because these use address_line for their
			// own purposes.
			// Set the low resolution modes to have as many lines as are scanned - 
			// Quite a few demos change the max_scanline register at display time
			// to get SFX: Majic12 show, Magic circle, Copper, GBU, Party91
			if (vga.crtc.maximum_scan_line & 0x80) vga.draw.address_line_total *= 2;
			vga.draw.double_scan = false;
		}
		else if (IS_VGA_ARCH) vga.draw.double_scan = (vga.crtc.maximum_scan_line & 0x80) > 0;
		else vga.draw.double_scan = (vtotal == 262);
	}
	else {
		htotal = vga.other.htotal + 1;
		hdend = vga.other.hdend;
		hbstart = hdend;
		hbend = htotal;
		hrstart = vga.other.hsyncp;
		hrend = hrstart + vga.other.hsyncw;

		vga.draw.address_line_total = vga.other.max_scanline + 1;
		vtotal = vga.draw.address_line_total * (vga.other.vtotal + 1) + vga.other.vadjust;
		vdend = vga.draw.address_line_total * vga.other.vdend;
		vrstart = vga.draw.address_line_total * vga.other.vsyncp;
		vrend = vrstart + 16; // vsync width is fixed to 16 lines on the MC6845 TODO Tandy
		vbstart = vdend;
		vbend = vtotal;
		vga.draw.double_scan = false;
		switch (machine) {
		case MCH_CGA:
		case TANDY_ARCH_CASE:
			clock = ((vga.tandy.mode_control & 1) ? 14318180 : (14318180 / 2)) / 8;
			break;
		case MCH_HERC:
			if (vga.herc.mode_control & 0x2) clock = 16000000 / 16;
			else clock = 16000000 / 8;
			break;
		default:
			clock = 14318180;
			break;
		}
		vga.draw.delay.hdend = hdend*1000.0 / clock; //in milliseconds
	}
#if C_DEBUG
	LOG(LOG_VGA, LOG_NORMAL)("h total %d end %d blank (%d/%d) retrace (%d/%d)",
		htotal, hdend, hbstart, hbend, hrstart, hrend);
	LOG(LOG_VGA, LOG_NORMAL)("v total %d end %d blank (%d/%d) retrace (%d/%d)",
		vtotal, vdend, vbstart, vbend, vrstart, vrend);
#endif
	if (!htotal) return;
	if (!vtotal) return;

	// The screen refresh frequency
	fps = (double)clock / (vtotal*htotal);
	// Horizontal total (that's how long a line takes with whistles and bells)
	vga.draw.delay.htotal = htotal*1000.0 / clock; //in milliseconds
	// Start and End of horizontal blanking
	vga.draw.delay.hblkstart = hbstart*1000.0 / clock; //in milliseconds
	vga.draw.delay.hblkend = hbend*1000.0 / clock;
	// Start and End of horizontal retrace
	vga.draw.delay.hrstart = hrstart*1000.0 / clock;
	vga.draw.delay.hrend = hrend*1000.0 / clock;
	// Start and End of vertical blanking
	vga.draw.delay.vblkstart = vbstart * vga.draw.delay.htotal;
	vga.draw.delay.vblkend = vbend * vga.draw.delay.htotal;
	// Start and End of vertical retrace pulse
	vga.draw.delay.vrstart = vrstart * vga.draw.delay.htotal;
	vga.draw.delay.vrend = vrend * vga.draw.delay.htotal;

	// Vertical blanking tricks
	vblank_skip = 0;
	if (IS_VGA_ARCH) { // others need more investigation
		if (vbstart < vtotal) { // There will be no blanking at all otherwise
			if (vbend > vtotal) {
				// blanking wraps to the start of the screen
				vblank_skip = vbend & 0x7f;

				// on blanking wrap to 0, the first line is not blanked
				// this is used by the S3 BIOS and other S3 drivers in some SVGA modes
				if ((vbend & 0x7f) == 1) vblank_skip = 0;

				// it might also cut some lines off the bottom
				if (vbstart < vdend) {
					vdend = vbstart;
				}
				LOG(LOG_VGA, LOG_WARN)("Blanking wrap to line %d", vblank_skip);
			}
			else if (vbstart <= 1) {
				// blanking is used to cut lines at the start of the screen
				vblank_skip = vbend;
				LOG(LOG_VGA, LOG_WARN)("Upper %d lines of the screen blanked", vblank_skip);
			}
			else if (vbstart < vdend) {
				if (vbend < vdend) {
					// the game wants a black bar somewhere on the screen
					LOG(LOG_VGA, LOG_WARN)("Unsupported blanking: line %d-%d", vbstart, vbend);
				}
				else {
					// blanking is used to cut off some lines from the bottom
					vdend = vbstart;
				}
			}
			vdend -= vblank_skip;
		}
	}
	// Display end
	vga.draw.delay.vdend = vdend * vga.draw.delay.htotal;

	// EGA frequency dependent monitor palette
	if (machine == MCH_EGA) {
		if (vga.misc_output & 1) {
			// EGA card is in color mode
			if ((1.0f / vga.draw.delay.htotal) > 19.0f) {
				// 64 color EGA mode
				VGA_ATTR_SetEGAMonitorPalette(EGA);
			}
			else {
				// 16 color CGA mode compatibility
				VGA_ATTR_SetEGAMonitorPalette(CGA);
			}
		}
		else {
			// EGA card in monochrome mode
			// It is not meant to be autodetected that way, you either
			// have a monochrome or color monitor connected and
			// the EGA switches configured appropriately.
			// But this would only be a problem if a program sets 
			// the adapter to monochrome mode and still expects color output.
			// Such a program should be shot to the moon...
			VGA_ATTR_SetEGAMonitorPalette(MONO);
		}
	}

	vga.draw.parts_total = VGA_PARTS;
	/*
	  6  Horizontal Sync Polarity. Negative if set
	  7  Vertical Sync Polarity. Negative if set
		 Bit 6-7 indicates the number of lines on the display:
			1:  400, 2: 350, 3: 480
	*/
	//Try to determine the pixel size, aspect correct is based around square pixels

	//Base pixel width around 100 clocks horizontal
	//For 9 pixel text modes this should be changed, but we don't support that anyway :)
	//Seems regular vga only listens to the 9 char pixel mode with character mode enabled
	double pwidth = (machine == MCH_EGA) ? (114.0 / htotal) : (100.0 / htotal);
	//Base pixel height around vertical totals of modes that have 100 clocks horizontal
	//Different sync values gives different scaling of the whole vertical range
	//VGA monitor just seems to thighten or widen the whole vertical range
	double pheight;
	double target_total = (machine == MCH_EGA) ? 262.0 : 449.0;
	Bitu sync = vga.misc_output >> 6;
	switch (sync) {
	case 0:		// This is not defined in vga specs,
				// Kiet, seems to be slightly less than 350 on my monitor
		//340 line mode, filled with 449 total
		pheight = (480.0 / 340.0) * (target_total / vtotal);
		break;
	case 1:		//400 line mode, filled with 449 total
		pheight = (480.0 / 400.0) * (target_total / vtotal);
		break;
	case 2:		//350 line mode, filled with 449 total
		//This mode seems to get regular 640x400 timing and goes for a loong retrace
		//Depends on the monitor to stretch the screen
		pheight = (480.0 / 350.0) * (target_total / vtotal);
		break;
	case 3:		//480 line mode, filled with 525 total
	default:
		//Allow 527 total ModeX to have exact 1:1 aspect
		target_total = (vga.mode == M_VGA && vtotal == 527) ? 527.0 : 525.0;
		pheight = (480.0 / 480.0) * (target_total / vtotal);
		break;
	}

	double aspect_ratio = pheight / pwidth;

	vga.draw.delay.parts = vga.draw.delay.vdend / vga.draw.parts_total;
	vga.draw.resizing = false;
	vga.draw.vret_triggered = false;

	//Check to prevent useless black areas
	if (hbstart < hdend) hdend = hbstart;
	if ((!IS_VGA_ARCH) && (vbstart < vdend)) vdend = vbstart;


	Bitu width = hdend;
	Bitu height = vdend;
	bool doubleheight = false;
	bool doublewidth = false;

	//Set the bpp
	Bitu bpp;
	switch (vga.mode) {
	case M_LIN15:
		bpp = 15;
		break;
	case M_LIN16:
		bpp = 16;
		break;
	case M_LIN32:
		bpp = 32;
		break;
	default:
		bpp = 8;
		break;
	}
	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = vga.vmemwrap - 1;
	switch (vga.mode) {
	case M_VGA:
		doublewidth = true;
		width <<= 2;
		if ((IS_VGA_ARCH) && (svgaCard == SVGA_None)) {
			bpp = 16;
			VGA_DrawLine = VGA_Draw_Xlat16_Linear_Line;
		}
		else VGA_DrawLine = VGA_Draw_Linear_Line;
		break;
	case M_LIN8:
		if (vga.crtc.mode_control & 0x8)
			width >>= 1;
		else if (svgaCard == SVGA_S3Trio && !(vga.s3.reg_3a & 0x10)) {
			doublewidth = true;
			width >>= 1;
		}
		// fall-through
	case M_LIN32:
		width <<= 3;
		if (vga.crtc.mode_control & 0x8)
			doublewidth = true;
			if (vga.mode == M_LIN32) {
				// vesa modes 10f/190/191/192
				aspect_ratio *= 2.0;
			}
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN15:
	case M_LIN16:
		// 15/16 bpp modes double the horizontal values
		width <<= 2;
		if ((vga.crtc.mode_control & 0x8) || (svgaCard == SVGA_S3Trio && (vga.s3.pll.cmd & 0x10)))
			doublewidth = true;
		else {
			// vesa modes 165/175
			aspect_ratio /= 2.0;
		}
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN4:
		doublewidth = (vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width <<= 3;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap << 1) - 1;
		break;
	case M_EGA:
		doublewidth = (vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width <<= 3;
		if ((IS_VGA_ARCH) && (svgaCard == SVGA_None)) {
			// This would also be required for EGA in Spacepigs Megademo
			bpp = 16;
			VGA_DrawLine = VGA_Draw_Xlat16_Linear_Line;
		}
		else VGA_DrawLine = VGA_Draw_Linear_Line;

		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap << 1) - 1;
		break;
	case M_CGA16:
		aspect_ratio = 1.2;
		doubleheight = true;
		vga.draw.blocks = width * 2;
		width <<= 4;
		VGA_DrawLine = VGA_Draw_CGA16_Line;
		break;
	case M_CGA4:
		doublewidth = true;
		vga.draw.blocks = width * 2;
		width <<= 3;
		VGA_DrawLine = VGA_Draw_2BPP_Line;
		break;
	case M_CGA2:
		doubleheight = true;
		vga.draw.blocks = 2 * width;
		width <<= 3;
		VGA_DrawLine = VGA_Draw_1BPP_Line;
		break;
	case M_PS55_GFX:
		vga.draw.blocks = width * 2;
		doublewidth = false;
		width *= 16;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		//bpp = 8;
		//vga.draw.linear_base = vga.fastmem;
		//vga.draw.linear_mask = vga.vmemwrap;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap << 1) - 1;
		break;
	case M_PS55_GFX_MONO:
		vga.draw.blocks = width * 2;
		doublewidth = false;
		width *= 16;
		VGA_DrawLine = VGA_PS55_Draw_1BPP_Line;
		//bpp = 8;
		//vga.draw.linear_base = vga.fastmem;
		//vga.draw.linear_mask = vga.vmemwrap;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap << 1) - 1;
		break;
	case M_PS55_TEXT:
		vga.draw.blocks = width;
		doublewidth = false;
		width *= 13;
		VGA_DrawLine = VGA_TEXT_PS55_Draw_Line;
		bpp = 8;
		break;
	case M_PS55_M3TEXT:
		vga.draw.blocks = width;
		doublewidth = false;
		width *= 13;
		VGA_DrawLine = VGA_TEXT_PS55_Mode3_Draw_Line;
		bpp = 8;
		break;
	case M_TEXT:
		vga.draw.blocks = width;
		doublewidth = (vga.seq.clocking_mode & 0x8) > 0;
		if (isJEGAEnabled()) {//if Kanji Text Disable is off
			vga.draw.char9dot = false;
			width *= 8;
			bpp = 16;
			VGA_DrawLine = VGA_TEXT_JEGA_Draw_Line;
		}
		else
		{
			if ((IS_VGA_ARCH) && (svgaCard == SVGA_None)) {
				// vgaonly: allow 9-pixel wide fonts
				if (vga.seq.clocking_mode & 0x01) {
					vga.draw.char9dot = false;
					width *= 8;
				}
				else {
					vga.draw.char9dot = true;
					width *= 9;
					aspect_ratio *= 1.125;
				}
				VGA_DrawLine = VGA_TEXT_Xlat16_Draw_Line;
				bpp = 16;
			}
			else {
				// not vgaonly: force 8-pixel wide fonts
				width *= 8; // 8 bit wide text font
				vga.draw.char9dot = false;
				VGA_DrawLine = VGA_TEXT_Draw_Line;
			}
		}
		break;
	case M_HERC_GFX:
		vga.draw.blocks = width * 2;
		width *= 16;
		aspect_ratio = ((double)width / (double)height)*(3.0 / 4.0);
		VGA_DrawLine = VGA_Draw_1BPP_Line;
		break;
	case M_TANDY2:
		aspect_ratio = 1.2;
		doubleheight = true;
		if (machine == MCH_PCJR) doublewidth = (vga.tandy.gfx_control & 0x8) == 0x00;
		else doublewidth = (vga.tandy.mode_control & 0x10) == 0;
		vga.draw.blocks = width * (doublewidth ? 4 : 8);
		width = vga.draw.blocks * 2;
		VGA_DrawLine = VGA_Draw_1BPP_Line;
		break;
	case M_TANDY4:
		aspect_ratio = 1.2;
		doubleheight = true;
		if (machine == MCH_TANDY) doublewidth = (vga.tandy.mode_control & 0x10) == 0;
		else doublewidth = (vga.tandy.mode_control & 0x01) == 0x00;
		vga.draw.blocks = width * 2;
		width = vga.draw.blocks * 4;
		if ((machine == MCH_TANDY && (vga.tandy.gfx_control & 0x8)) ||
			(machine == MCH_PCJR && (vga.tandy.mode_control == 0x0b)))
			VGA_DrawLine = VGA_Draw_2BPPHiRes_Line;
		else VGA_DrawLine = VGA_Draw_2BPP_Line;
		break;
	case M_TANDY16:
		aspect_ratio = 1.2;
		doubleheight = true;
		vga.draw.blocks = width * 2;
		if (vga.tandy.mode_control & 0x1) {
			if ((machine == MCH_TANDY) && (vga.tandy.mode_control & 0x10)) {
				doublewidth = false;
				vga.draw.blocks *= 2;
				width = vga.draw.blocks * 2;
			}
			else {
				doublewidth = true;
				width = vga.draw.blocks * 2;
			}
			VGA_DrawLine = VGA_Draw_4BPP_Line;
		}
		else {
			doublewidth = true;
			width = vga.draw.blocks * 4;
			VGA_DrawLine = VGA_Draw_4BPP_Line_Double;
		}
		break;
	case M_TANDY_TEXT:
		doublewidth = (vga.tandy.mode_control & 0x1) == 0;
		aspect_ratio = 1.2;
		doubleheight = true;
		vga.draw.blocks = width;
		width <<= 3;
		VGA_DrawLine = VGA_TEXT_Draw_Line;
		break;
	case M_HERC_TEXT:
		aspect_ratio = ((double)480) / ((double)350);
		vga.draw.blocks = width;
		width <<= 3;
		VGA_DrawLine = VGA_TEXT_Herc_Draw_Line;
		break;
	default:
		LOG(LOG_VGA, LOG_ERROR)("Unhandled VGA mode %d while checking for resolution", vga.mode);
		break;
	}
	VGA_CheckScanLength();
	if (vga.draw.double_scan) {
		if (IS_VGA_ARCH) {
			vga.draw.vblank_skip /= 2;
			height /= 2;
		}
		doubleheight = true;
	}
	vga.draw.vblank_skip = vblank_skip;

	if (!(IS_VGA_ARCH && (svgaCard == SVGA_None) && (vga.mode == M_EGA || vga.mode == M_VGA))) {
		//Only check for extra double height in vga modes
		//(line multiplying by address_line_total)
		if (!doubleheight && (vga.mode < M_TEXT) && !(vga.draw.address_line_total & 1)) {
			vga.draw.address_line_total /= 2;
			doubleheight = true;
			height /= 2;
		}
	}
	vga.draw.lines_total = height;
	vga.draw.parts_lines = vga.draw.lines_total / vga.draw.parts_total;
	vga.draw.line_length = width * ((bpp + 1) / 8);
#ifdef VGA_KEEP_CHANGES
	vga.changes.active = false;
	vga.changes.frame = 0;
	vga.changes.writeMask = 1;
#endif
	/*
	   Cheap hack to just make all > 640x480 modes have square pixels
	*/
	if (width >= 640 && height >= 480) {
		aspect_ratio = 1.0;
		//aspect_ratio = ((float)width / (float)height) * (3.0 / 4.0);
	}
	LOG(LOG_VGA, LOG_NORMAL)("ht %d vt %d ratio %f", htotal, vtotal, aspect_ratio );

	bool fps_changed = false;
	// need to change the vertical timing?
	if (fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001) {
		fps_changed = true;
		vga.draw.delay.vtotal = 1000.0 / fps;
		VGA_KillDrawing();
		PIC_RemoveEvents(VGA_Other_VertInterrupt);
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		VGA_VerticalTimer(0);
	}
	//debug for PS/55
	//LOG(LOG_VGA, LOG_NORMAL)("%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f"
	//	, vga.draw.delay.framestart
	//	, vga.draw.delay.hblkend
	//	, vga.draw.delay.hblkstart
	//	, vga.draw.delay.hdend
	//	, vga.draw.delay.hrend
	//	, vga.draw.delay.hrstart
	//	, vga.draw.delay.htotal
	//	, vga.draw.delay.parts
	//	, vga.draw.delay.vblkend
	//	, vga.draw.delay.vblkstart
	//	, vga.draw.delay.vdend
	//	, vga.draw.delay.vrend
	//	, vga.draw.delay.vrstart
	//	, vga.draw.delay.vtotal
	//	);
	//for (int i = 0; i <= 0x18; i++) {

	//	vga.crtc.index = i;
	//	LOG(LOG_VGA, LOG_NORMAL)("CRTC (0x%2X): %X (%d)", i, vga_read_p3d5(0x3d5, 1), vga_read_p3d5(0x3d5, 1));
	//}

#if C_DEBUG
	LOG(LOG_VGA, LOG_NORMAL)("h total %2.5f (%3.2fkHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
		vga.draw.delay.htotal, (1.0 / vga.draw.delay.htotal),
		vga.draw.delay.hblkstart, vga.draw.delay.hblkend,
		vga.draw.delay.hrstart, vga.draw.delay.hrend);
	LOG(LOG_VGA, LOG_NORMAL)("v total %2.5f (%3.2fHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
		vga.draw.delay.vtotal, (1000.0 / vga.draw.delay.vtotal),
		vga.draw.delay.vblkstart, vga.draw.delay.vblkend,
		vga.draw.delay.vrstart, vga.draw.delay.vrend);
#endif

	// need to resize the output window?
	if ((width != vga.draw.width) ||
		(height != vga.draw.height) ||
		(vga.draw.doublewidth != doublewidth) ||
		(vga.draw.doubleheight != doubleheight) ||
		(fabs(aspect_ratio - vga.draw.aspect_ratio) > 0.0001) ||
		(vga.draw.bpp != bpp) || fps_changed) {

		VGA_KillDrawing();

		vga.draw.width = width;
		vga.draw.height = height;
		vga.draw.doublewidth = doublewidth;
		vga.draw.doubleheight = doubleheight;
		vga.draw.aspect_ratio = aspect_ratio;
		vga.draw.bpp = bpp;
		if (doubleheight) vga.draw.lines_scaled = 2;
		else vga.draw.lines_scaled = 1;
#if C_DEBUG
		LOG(LOG_VGA, LOG_NORMAL)("Width %d, Height %d, fps %f", width, height, fps);
		LOG(LOG_VGA, LOG_NORMAL)("%s width, %s height aspect %f",
			doublewidth ? "double" : "normal", doubleheight ? "double" : "normal", aspect_ratio);
#endif
		if (!vga.draw.vga_override)
			RENDER_SetSize(width, height, bpp, (float)fps, aspect_ratio,
				doublewidth, doubleheight);
	}
}

void VGA_KillDrawing(void) {
	PIC_RemoveEvents(VGA_DrawPart);
	PIC_RemoveEvents(VGA_DrawSingleLine);
	PIC_RemoveEvents(VGA_DrawEGASingleLine);
	vga.draw.parts_left = 0;
	vga.draw.lines_done = ~0;
	if (!vga.draw.vga_override) RENDER_EndUpdate(true);
}

void VGA_SetOverride(bool vga_override) {
	if (vga.draw.vga_override!=vga_override) {
		
		if (vga_override) {
			VGA_KillDrawing();
			vga.draw.vga_override=true;
		} else {
			vga.draw.vga_override=false;
			vga.draw.width=0; // change it so the output window gets updated
			VGA_SetupDrawing(0);
		}
	}
}
