/*
Copyright (c) 2021-2022 akm
All rights reserved.
This content is under the MIT License.

Note:
	There are some variants of the Display Adapter, but all adapters
	doesn't have the BIOS. Instead, the Japanese DOS provides
	BIOS functions, and absorbs differences in individual hardwares.
	This means there are some special versions of Japanese DOS, and
	they won't run on this.

	This code emulates PS/55 Model 5550-S, T and V.
	Please check the system requirements of your operating system and software,
	and it must support above hardwares.

	You must disable Disney Sound Source emulation to run DOS J4.0 and later.

*/
#include "dosbox.h"
#include "setup.h"
#include "vga.h"
#include "paging.h"
#include "inout.h"

#include "ps55.h"//for PS/55
#include "jsupport.h"

PS55Registers ps55;
MCARegisters mca;

extern Bitu vga_read_p3da(Bitu port, Bitu iolen);

extern void vga_write_p3d4(Bitu port, Bitu val, Bitu iolen);
extern Bitu vga_read_p3d4(Bitu port, Bitu iolen);
extern void vga_write_p3d5(Bitu port, Bitu val, Bitu iolen);
extern Bitu vga_read_p3d5(Bitu port, Bitu iolen);

extern void write_p3ce(Bitu port, Bitu val, Bitu iolen);
extern Bitu read_p3ce(Bitu port, Bitu iolen);
extern void write_p3cf(Bitu port, Bitu val, Bitu iolen);
extern Bitu read_p3cf(Bitu port, Bitu iolen);

extern void write_p3c0(Bitu port, Bitu val, Bitu iolen);//data
extern Bitu read_p3c0(Bitu port, Bitu iolen);//idx
extern Bitu read_p3c1(Bitu port, Bitu iolen);//data

extern void write_p3c4(Bitu /*port*/, Bitu val, Bitu /*iolen*/);
extern void write_p3c5(Bitu /*port*/, Bitu val, Bitu iolen);


void PS55_GC_Index_Write(Bitu port, Bitu val, Bitu len);

//#define VGA_PAGES		(128/4)
#define VGA_PAGE_A0		(0xA0000/4096)
#define VGA_PAGE_B0		(0xB0000/4096)
#define VGA_PAGE_B8		(0xB8000/4096)
//#define PS55_BASE_E0	(0xE0000/4096)

class PS55_Memory_Handler : public PageHandler {
public:
	PS55_Memory_Handler() {
		flags = PFLAG_NOCODE;
	}
	//Font ROM and Gaiji RAM placed 2-4 banks x 128 KB of its memory between 0xA0000 - 0xBFFFF.
	// Address Bits: ab bbbb bbbb bbbb bbbb (a = bank select, b = addr)
	Bitu readHandler(PhysPt addr) {
		if(ps55.data3e3_08 == 0x80)
			addr |= ((Bitu)ps55.mem_bank << 17);
		else
		{
			Bitu index = ps55.mem_select & 0x0f;
			index <<= 8;
			index |= ps55.mem_bank;
			addr += index * 0x40;
		}
		//LOG_MSG("PS55_MemHnd: Read from mem %x, bank %x, addr %x", ps55.mem_select, ps55.mem_bank, addr);
		switch (ps55.mem_select & 0xf0) {
		case 0xb0://Gaiji RAM
			//LOG_MSG("PS55_MemHnd: Read from mem %x, bank %x, addr %x (%x)", ps55.mem_select, ps55.mem_bank, addr / 128, addr);
			addr &= 0x3FFFF;//safety access
			return ps55.gaiji_ram[addr];
			break;
		case 0x10://Font ROM
			//LOG_MSG("PS55_MemHnd: Read from mem %x, bank %x, addr %x (%x)", ps55.mem_select, ps55.mem_bank, addr / 72, addr);
			if (addr > DBCS24_LEN) return 0xff;
			return ps55font_24[addr];
			break;
		default:
			return 0xff;//invalid memory access
			break;
		}
	}
	void writeHandler(PhysPt addr, Bit8u val) {
		if (!(ps55.gaijiram_access & 0x10))
		{
			LOG_MSG("PS55_MemHnd: Failure to write to mem %x, addr %x, val %x", ps55.mem_select, addr, val);
			return;//safety
		}
		if (ps55.data3e3_08 == 0x80)
			addr |= ((Bitu)ps55.mem_bank << 17);
		else
		{
			Bitu index = ps55.mem_select & 0x0f;
			index <<= 8;
			index |= ps55.mem_bank;
			addr += index * 0x40;
		}
		switch (ps55.mem_select) {
		case 0xb0://Gaiji RAM
			//LOG_MSG("PS55_MemHnd: Write to mem %x, addr %x (%x), val %x", ps55.mem_select, addr / 128, addr, val);
			addr &= 0x3FFFF;//safety access
			ps55.gaiji_ram[addr] = val;
			break;
		case 0x10://Font ROM
			//Read-Only
			break;
		default:
			break;
		}
	}
	void writeb(PhysPt addr, Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0x1FFFF;
		writeHandler(addr + 0, (Bit8u)(val >> 0));
	}
	void writew(PhysPt addr, Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0x1FFFF;
		writeHandler(addr + 0, (Bit8u)(val >> 0));
		writeHandler(addr + 1, (Bit8u)(val >> 8));
	}
	Bitu readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0x1FFFF;
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0x1FFFF;
		Bitu ret = (readHandler(addr + 0) << 0);
		ret |= (readHandler(addr + 1) << 8);
		return ret;
	}
};

PS55_Memory_Handler ps55mem_handler;

//I/O Card Selected Feedback Register (Hex 0091) : Unused
//I/O System Board Enable/Setup Regiseter (Hex 0094)
void write_p94(Bitu port, Bitu val, Bitu len) {
	//LOG_MSG("MCA_p94: Write to port %x, val %x, len %x", port, val, len);
	if (val & 0x80)//Bit 7: Enable/-Setup System Board Functions
	{
		mca.mothersetupen = false;
	}
	else
	{
		mca.mothersetupen = true;
	}
	if (val & 0x20)//Bit 5: Enable/-Setup Video SubSystem
	{
		mca.vgasetupen = false;
	}
	else
	{
		mca.vgasetupen = true;
	}
}

Bitu read_p94(Bitu port, Bitu len) {
	//LOG_MSG("MCA_p94: Read from port %x, len %x", port, len);
	return 0;
}

//I/O Adapter Enable/Setup Register (Hex 0096)
void write_p96(Bitu port, Bitu val, Bitu len) {
	//LOG_MSG("MCA_p96: Write to port %x, val %x, len %x", port, val, len);
	if (val & 0x08)//Bit 3: Card Setup Enable
	{
		mca.cardsetupen = true;
		mca.cardsel = val & 0x07;//Bit 2-0: Card Select 0-7
	}
	else
	{
		mca.cardsetupen = false;
	}
}

Bitu read_p96(Bitu port, Bitu len) {
	LOG_MSG("MCA_p96: Read from port %x, len %x", port, len);
	return 0;
}


void DisableVGA(void) {
	IO_FreeWriteHandler(0x3c0, IO_MB);
	IO_FreeReadHandler(0x3c0, IO_MB);
	IO_FreeWriteHandler(0x3c1, IO_MB);
	IO_FreeReadHandler(0x3c1, IO_MB);

	for (Bitu i = 0; i <= 3; i++) {
		IO_FreeWriteHandler(0x3d0 + i * 2, IO_MB);
		IO_FreeReadHandler(0x3d0 + i * 2, IO_MB);
		IO_FreeWriteHandler(0x3d0 + i * 2 + 1, IO_MB);
		IO_FreeReadHandler(0x3d0 + i * 2 + 1, IO_MB);
		IO_FreeWriteHandler(0x3b0 + i * 2, IO_MB);
		IO_FreeReadHandler(0x3b0 + i * 2, IO_MB);
		IO_FreeWriteHandler(0x3b0 + i * 2 + 1, IO_MB);
		IO_FreeReadHandler(0x3b0 + i * 2 + 1, IO_MB);
	}

	//IO_FreeReadHandler(0x3ba, IO_MB);
	//IO_FreeReadHandler(0x3da, IO_MB);//DOSSHELL reads Input Status Register 1

	MEM_ResetPageHandler(0xc0000 / 4096, 8);//setup ram at 0xc0000-0xc8000
	PAGING_ClearTLB();
	for (Bitu i = 0; i < 0x8000; i++) {
		mem_writeb(0xc0000 + i, (Bit8u)0);
	}

	LOG_MSG("VGA I/O handlers disabled!!");
}

/*
POS Address Map (see IBM PS/55 Model 5550-S/T/V Technical Reference ISBN4-274-07624-5 N:SA18-2901-2)
   100h: POS Register 0 - Adapter Identification Byte (Low Byte)
   101h: POS Register 1 - Adapter Identification Byte (High Byte)
   102h: POS Register 2 - Option Select Data Byte 1
   103h: POS Register 3 - Option Select Data Byte 2
   104h: POS Register 4 - Option Select Data Byte 3
   105h: POS Register 5 - Option Select Data Byte 4
   106h: POS Register 6 - Subaddress Extension (Low Byte)
   107h: POS Register 7 - Subaddress Extension (High Byte)
*/
void MCA_Card_Reg_Write(Bitu port, Bitu val, Bitu len) {
	//LOG_MSG("MCA_p10x: Write to port %x, val %x, len %x", port, val, len);
	if (mca.vgasetupen) {//I/O 94h bit 5 is on
		switch (port) {
		case 0x102://POS Register 2 (Hex 102)
			if (!(val & 0x01))
				DisableVGA();//disable VGA original I/O port handler
			break;
		default:
			break;
		}
	}
	if((mca.cardsetupen) && (mca.cardsel == 0)) {//card slot 0 is selected
		switch (port) {
		case 0x102://POS Register 2 (Hex 102)
			if (val & 0x01)//Bit 0: Card Enable, Bit 1-2: ??? *** Dependent on the card! ***
				PS55_WakeUp();//enable PS/55 DA
			//else
			//	PS55_ShutDown();//disable PS/55 DA
			break;
		default:
			break;
		}
	}
	if (mca.mothersetupen) {//I/O 94h bit 7 is unset
		switch (port) {
		case 0x104://System Board POS Register 4 (Hex 104)
			//if (val & 0x01) ;//Bit 0: Disable/-Enable 4 KB Hex E0000-E0FFF); do nothing here
			break;
		default:
			break;
		}
	}
}
/*
	POS ID	SYS ID	
	EFFFh	*		Display Adapter
	EFFEh	*		Display Adapter II (3E0:0A = xx0x xxxx)
	|-		FFF2h	Display Adapter B2 (3E0:0A = xx1x xxxx)
	|-		FDFEh	Display Adapter B2 (3E0:0A = xx1x xxxx)
	 -		*		Display Adapter III,V (3E0:0A = xx1x xxxx)
	ECECh	FF4Fh	Display Adapter B1
	 -		*		Display Adapter IV
	901Fh	*		Display Adapter A2
	901Dh	*		Display Adapter A1
	901Eh	*		Plasma Display Adapter
*/

//I/O Read POS Regs to return card ID 0xECEC (Display Adapter IV)
Bitu MCA_Card_Reg_Read(Bitu port, Bitu len) {
	Bitu ret = 0xff;
	if ((mca.cardsetupen) && (mca.cardsel == 0)) {
		switch (port) {
		case 0x100://POS Register 0 - Adapter Identification Byte (Low byte)
			ret = 0xfe;
			break;
		case 0x101://POS Register 1 - Adapter Identification Byte (High byte)
			ret = 0xef;
			break;
		case 0x102://POS Register 2
			ret = (ps55.carden) ? 1 : 0;
			break;
		default:
			break;
		}
	}
	if ((mca.mothersetupen)) {//I/O 94h bit 7 is on
		switch (port) {
		case 0x102://System Board POS Register 2 (Hex 102)
			ret = 0x0f;//0010 1111
			break;
		default:
			break;
		}
	}
	//LOG_MSG("MCA_p10x: Read from port %x, len %x, ret %x", port, len, ret);
	return ret;
}
#ifdef _DEBUG
void PS55_CRTC_Write(Bitu port, Bitu val, Bitu len) {
	if (len == 2) LOG_MSG("PS55_CRTC: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
	else LOG_MSG("!!PS55_CRTC: Write to port %x, val %02xh (%d), len %x", port,val, val, len);
}

Bitu PS55_CRTC_Read(Bitu port, Bitu len) {
	LOG_MSG("!!PS55_CRTC: Read from port %x, len %x", port, len);
	return 0;
}
#endif
void PS55_GC_Data_Write(Bitu port, Bitu val, Bitu len) {
	//if (len == 2) LOG_MSG("PS55: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
	//else LOG_MSG("PS55: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
	if (ps55.prevdata != NOAHDATA && len == 1)
	{
		val <<= 8;
		val |=ps55.prevdata;
		len = 2;
		ps55.prevdata = NOAHDATA;
	}
	else if (len == 1)
		ps55.prevdata = val;
	switch (port) {
	case 0x3e1://?
		if (ps55.idx_3e1 < 0x20) ps55.p3e1_reg[ps55.idx_3e1] = val;//save value for 0x1f function
		switch (ps55.idx_3e1) {
		case 0x00:
			//LOG_MSG("PS55_??: 3E1:%X Accessed !!!!!!!!!!!!! (val: %X, %dd)", ps55.idx_3e1, val, val);
			break;
		case 0x02:
			//Bit 4: Graphics / Text mode ? 
			//Bit 2: Graphics Color / Mono ?
			//LOG_MSG("PS55_??: 3E1:%X Accessed !!!!!!!!!!!!! (val: %X, %dd)", ps55.idx_3e1, val, val);
			if (ps55.data3e1_02 ^ val) {
				ps55.data3e1_02 = val;
				VGA_DetermineMode();
			}
			break;
		case 0x08://bit 4: Gaiji RAM Access Enabled
			if((ps55.gaijiram_access & 0x10) ^ (val & 0x10)){
				if (val & 0x10) {
					//store current pagehandlers
					for (Bitu j = 0; j < 32; j++)
					{
						ps55.pagehndl_backup[j] = MEM_GetPageHandler(GAIJI_RAMBASE + j);
						MEM_SetPageHandler(GAIJI_RAMBASE + j, 1, &ps55mem_handler);
						//LOG_MSG("PS55_MemHnd: Setup page handler %X" , GAIJI_RAMBASE + j);
					}
					PAGING_ClearTLB();
					//LOG_MSG("PS55_MemHnd: Setup page handlers");
				}
				else
				{
					//LOG_MSG("PS55_MemHnd: Page handler is restoring.");
					//restore pagehandlers
					for (Bitu j = 0; j < 32; j++)
					{
						if (MEM_GetPageHandler(GAIJI_RAMBASE + j) == &ps55mem_handler)//validate it's not changed
						{
							MEM_SetPageHandler(GAIJI_RAMBASE + j, 1, ps55.pagehndl_backup[j]);
							//LOG_MSG("PS55_MemHnd: Page handler restored +");
						}
						else
						{
							LOG_MSG("PS55_MemHnd: Page handler not restored -");
						}
					}
					PAGING_ClearTLB();
				}
			}
			ps55.gaijiram_access = val;
			break;
		default:
			//LOG_MSG("PS55_??: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e1, val, val);
			break;
		}
		//LOG_MSG("PS55_??: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e1, val, val);
		break;
	case 0x3e3://Font Buffer Registers (undoc) : TODO IBM 5550 BASIC uses many unknown regs (Index 20-2Fh)
		switch (ps55.idx_3e3) {
		case 0x00://video memory B8000h
			//LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			if ((ps55.mem_conf & 0x40) ^ (val & 0x40)) {
				ps55.mem_conf = val;
				VGA_DetermineMode();//setup vga drawing if the value is changed
			}
			ps55.mem_conf = val;//for DEBUG
			break;
		case 0x08://??? length of font data? 80h latching? 10h
			ps55.data3e3_08 = val;
			//if(val != 0x80) LOG_MSG("PS55_??: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			break;
		case 0x0a://Number of the memory page to access
			//if (val > 0x04) LOG_MSG("PS55_??: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			ps55.mem_bank = val;
			break;
		case 0x0b://??? Select memory to locate on the system bus
			ps55.mem_select = val;
			//if (!(val == 0xb0 || val == 0x10)) LOG_MSG("PS55_??: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			break;
		case 0x18://??? Memory refresh? 4->0
			break;
		default:
			//LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			break;
		}
		//LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
		break;
	case 0x3e5://CRT Controller Registers (undocumented)
		if (ps55.idx_3e5 < 0x20) ps55.crtc_reg[ps55.idx_3e5] = val;//save value for 0x1f function
#ifdef C_HEAVY_DEBUG
		if (!(ps55.idx_3e5 == 0xe || ps55.idx_3e5 == 0xf)) LOG_MSG("PS55_CRTC: Write to port %x, idx %x, val %02xh (%d), m%d, len %d", port, ps55.idx_3e5, val, val, ps55.crtc_reg[0x19], len);
#endif
		switch (ps55.idx_3e5) {
		//PS/55 actually has 16-bit registers, but in DOSBox, this uses the overflow register to overlap the VGA. Arkward:-<
		//6, 0c, 0e, 10, 12, 15, 16, 18
		case 0x00:
			//val -= 5;//for VGA: Horizontal Total Character Clocks-5
			break;
		case 0x01://Horizontal Display End Register
			if (ps55.crtc_cmode) val-=2;
			else val--;
			break;
		case 0x02://Start Horizontal Blanking
			if (ps55.crtc_cmode) val-=2;
			else val--;
			break;
		case 0x04://Start Horizontal Retrace Pulse
			if (ps55.crtc_cmode) val-=2;
			else val--;
			break;
		case 0x05://End Horizontal Retrace
			if (ps55.crtc_cmode) val-=2;
			else val--;
			break;
		case 0x06://Vertical Total
			val -= 2;//for VGA the number of scan lines in the display - 2.
			if (val > 0xff) {//for PS/55
				Bit8u new_overflow = (val & 0x100) >> 8;//(01 0000 0000 -> 0000 0001)
				new_overflow |= (val & 0x200) >> 4;//(10 0000 0000 -> 0010 0000)
				new_overflow ^= vga.crtc.overflow & 0x21;//CRTC 07h Overflow Register
				vga.crtc.overflow ^= new_overflow;
				val &= 0xff;
			}
			else {
				vga.crtc.overflow &= ~0x21;
			}
			break;
		case 0x07://DA never handle overflow register.
			return;
			break;
		case 0x09://Maximum Scan line
			if(!(ps55.data3e1_02 & 0x01))//in graphics mode, this duplicates scanlines
				val=0;
			break;
		case 0x0c://Stard Address
			if (ps55.crtc_reg[0x1c] & 0x40)
				return;//ignore write (0xff) if reg 1C & 0x04
			//val &= 0xff;
			break;
		case 0x10://Vertical Retrace Start Register
			if (val > 0xff) {
				Bit8u new_overflow = (val & 0x100) >> 6;//(01 0000 0000 -> 0000 0100)
				new_overflow |= (val & 0x200) >> 2;//(10 0000 0000 -> 1000 0000)
				new_overflow ^= vga.crtc.overflow & 0x84;
				vga.crtc.overflow ^= new_overflow;
				val &= 0xff;
			}
			else {
				vga.crtc.overflow &= ~0x84;
			}
			break;
		case 0x12://Vertical Display End Register
			if (ps55.crtc_cmode) val -= 31;
			else val-=2;//for VGA
			if (val > 0xff) {
				Bit8u new_overflow = (val & 0x100) >> 7;//(01 0000 0000 -> 0000 0010)
				new_overflow |= (val & 0x200) >> 3;//(10 0000 0000 -> 0100 0000)
				new_overflow ^= vga.crtc.overflow & 0x42;
				vga.crtc.overflow ^= new_overflow;
				val &= 0xff;
			}
			else {
				vga.crtc.overflow &= ~0x42;
			}
			break;
		case 0x15://Start Vertical Blank Register
			if (val > 0xff) {
				Bit8u new_overflow = (val & 0x100) >> 5;//(01 0000 0000 -> 0000 1000)
				new_overflow ^= vga.crtc.overflow & 0x08;
				vga.crtc.overflow ^= new_overflow;

				new_overflow = (val & 0x200) >> 4;//(10 0000 0000 -> 0010 0000)
				new_overflow ^= vga.crtc.maximum_scan_line & 0x20;
				vga.crtc.maximum_scan_line ^= new_overflow;

				val &= 0xff;
			}
			else {
				vga.crtc.overflow &= ~0x08;
				vga.crtc.maximum_scan_line &= ~0x20;
			}
			break;
		case 0x18://Line Compare Register
			if (val > 0xff) {
				Bit8u new_overflow = (val & 0x100) >> 4;//(01 0000 0000 -> 0001 0000)
				new_overflow ^= vga.crtc.overflow & 0x10;
				vga.crtc.overflow ^= new_overflow;

				new_overflow = (val & 0x200) >> 3;//(10 0000 0000 -> 0100 0000)
				new_overflow ^= vga.crtc.maximum_scan_line & 0x40;//CRTC 09h Maximum Scan Line Register
				vga.crtc.maximum_scan_line ^= new_overflow;

				val &= 0xff;
			}
			else {
				vga.crtc.overflow &= ~0x10;
				vga.crtc.maximum_scan_line &= ~0x40;
			}
			break;
		case 0x19:
			bool new_crtcmode;
			new_crtcmode = (val & 0x01) ? true : false;//TODO
			if (ps55.crtc_cmode != new_crtcmode) {//if changed
				ps55.crtc_cmode = new_crtcmode;
				//LOG_MSG("Reprogramming CRTC timing table.");
				//reconfigure crtc with modified values
				for (Bitu i = 0; i < 0x1f; i++)
				{
					PS55_GC_Index_Write(0x3e4, i, 1);
					PS55_GC_Data_Write(0x3e5, ps55.crtc_reg[i], 2);
				}
			}
			return;
		case 0x1c://? Bit 7: Graphics mode?, Bit 6: set 1 when idx c is set to ffh
			return;
		case 0x1f:
			//bit 5: cursor width 2x, bit 1: ?
			ps55.cursor_wide = (val & 0x20) ? true : false;
			return;
		}
		vga_write_p3d5(port, val, len);
		//if (!(ps55.idx_3e5 == 0xe || ps55.idx_3e5 == 0xf)) LOG_MSG("PS55_ATTR: Write to port %x, idx %x, val %02xh (%d)", 0x3d5, ps55.idx_3e5, val, val);
		break;
	case 0x3eb://Graphics Controller Registers
	case 0x3ec:
		switch (ps55.idx_3eb) {
		case 0x00://Set/Reset
			ps55.set_reset = val & 0xff;
			ps55.full_set_reset_low = FillTable[val & 0x0f];
			ps55.full_enable_and_set_reset_low = ps55.full_set_reset_low & ps55.full_enable_set_reset_low;
			ps55.full_set_reset_high = FillTable[(val >> 4) & 0x0f];
			ps55.full_enable_and_set_reset_high = ps55.full_set_reset_high & ps55.full_enable_set_reset_high;
			break;
		case 0x01://Enable Set/Reset
			//LOG_MSG("PS55_GC Enable Set/Reset (01h) val %02xh (%d)", val, val);
			ps55.enable_set_reset = val & 0xff;
			ps55.full_enable_set_reset_low = FillTable[val & 0x0f];
			ps55.full_not_enable_set_reset_low = ~ps55.full_enable_set_reset_low;
			ps55.full_enable_and_set_reset_low = ps55.full_set_reset_low & ps55.full_enable_set_reset_low;
			ps55.full_enable_set_reset_high = FillTable[(val >> 4) & 0x0f];
			ps55.full_not_enable_set_reset_high = ~ps55.full_enable_set_reset_high;
			ps55.full_enable_and_set_reset_high = ps55.full_set_reset_high & ps55.full_enable_set_reset_high;
			break;
		case 0x03://Data Rotate
			ps55.data_rotate = val & 0x0f;
			break;
		case 0x04://Read Map Select
			//LOG_MSG("PS55_GC: Read Map Select (04h) val %02xh (%d)", val, val);
			vga.gfx.read_map_select = val & 0x07;
			vga.config.read_map_select = val & 0x07;
			break;
		case 0x06://Reserved (VGA: Miscellaneous)
		case 0x07://Reserved (VGA: Color Don't Care)
			//do nothing
			break;
		case 0x08://Bit Mask (Low)
			ps55.bit_mask_low = val & 0xff;
			ps55.full_bit_mask_low = ExpandTable[val & 0xff];
			break;
		case 0x09://Bit Mask (High)
			ps55.bit_mask_high = val & 0xff;
			ps55.full_bit_mask_high = ExpandTable[val & 0xff];
			break;
		case 0x0a://Map Mask
			//LOG_MSG("PS55_GC: Map Mask (0Ah) val %02xh (%d)", val, val);
			ps55.map_mask = val & 0xff;
			ps55.full_map_mask_low = FillTable[val & 0x0f];
			ps55.full_not_map_mask_low = ~ps55.full_map_mask_low;
			ps55.full_map_mask_high = FillTable[(val >> 4) & 0x0f];
			ps55.full_not_map_mask_high = ~ps55.full_map_mask_high;
			break;
		case 0x0b://Command (must be 08h according to the IBM reference) but J-DOS uses 0Bh
			// 08 0000 1000
			// 0B 0000 1011
			//LOG_MSG("PS55_GC: Command (0Bh) val %02xh (%d)", val, val);
			vga.config.raster_op = val & 3;//?
			break;
		case 0x05://Mode
			//LOG_MSG("PS55_GC: Graphics Mode (05h) val %02xh (%d)", val, val);
			//val &= 0xf0;
			//through!!!
		default:
			//LOG_MSG("PS55_GC: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3eb, val, val);
			write_p3cf(port, val, len);
			break;
		}
		break;
	case 0x3e9:
		//dummy to avoid logmsg duplication
		break;
	default:
		//if (len == 2) LOG_MSG("PS55_??: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
		//else LOG_MSG("PS55_??: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
		break;
	}
}

void PS55_ATTR_Write(Bitu port, Bitu val, Bitu len) {
	Bitu data = 0;
	Bit8u chgbits;
	if (len == 1 && ps55.latch_3e8 == false && port == 0x3e8)
	{
#ifdef C_HEAVY_DEBUG
		LOG_MSG("PS55_ATTR: Set index %02xh (%d), not latched", val, val);
#endif
		ps55.idx_3e8 = val;
		ps55.latch_3e8 = true;
		if (val & 0x20) vga.attr.disabled &= ~1;
		else vga.attr.disabled |= 1;
	}
	else {
		if (len == 2) {
#ifdef C_HEAVY_DEBUG
			LOG_MSG("PS55_ATTR: Write val %04xh (%d), previdx %x", val, val, ps55.idx_3e8);
#endif
			ps55.idx_3e8 = val & 0xff;
			data = val >> 8;
		}
		else
		{
			data = val;
#ifdef C_HEAVY_DEBUG
			LOG_MSG("PS55_ATTR: Write val %02xh (%d), idx %x", val, val, ps55.idx_3e8);
#endif
		}
		ps55.latch_3e8 = false;
		//!!!!!  ps55.idx_3e8=Index, data=Data   !!!!!
		switch (ps55.idx_3e8 & 0x1f) {
		case 0x0a://Cursor start and options
			data &= 0x3f;//mask input for VGA
			//write_p3c0(port, ps55.idx_3e8, 1);
			//write_p3c0(port, data, 1);
			break;
		case 0x14://Set line color for character mode
			ps55.palette_line = data & 0x0f;
			//ps55.palette_line = data;
			return;
		case 0x18://attribute mode 03 compatible?
			if ((ps55.text_mode03 & 0x01) ^ (data & 0x01)) {
				ps55.text_mode03 = data;
				//VGA_DetermineMode();//setup vga drawing if the value is changed
			}
			return;
		case 0x1a://Cursor color
#ifdef C_HEAVY_DEBUG
			if ((ps55.cursor_color & 0x0f) ^ (data & 0x0f)) {
				LOG_MSG("PS55_ATTR: The cursor color has changed to %02xh (%d)", data & 0x0f, data & 0x0f);
			}
#endif
			ps55.cursor_color = data & 0x0f;
			return;
		case 0x1b://Cursor blinking speed
#ifdef C_HEAVY_DEBUG
			if (ps55.cursor_options ^ data) {
				LOG_MSG("PS55_ATTR: The cursor option has changed to %02xh (%d)", data, data);
			}
#endif
			ps55.cursor_options = data;
			if (data & 0x01)
				vga.draw.cursor.enabled = true;
			else
				vga.draw.cursor.enabled = false;
			switch (data & 0x18) {
			case 0x08://fast blinking
				ps55.cursor_blinkspeed = 0x10;
				break;
			case 0x18://slow blinking
				ps55.cursor_blinkspeed = 0x20;
				break;
			default://don't blinking
				ps55.cursor_blinkspeed = 0xFF;
			}
			return;
		case 0x1d://bit 7: Set color/mono attribute, bit 6: Set light-color characters
			ps55.attr_mode = data & 0xe0;
			return;
		case 0x1f://????
			chgbits = ps55.data3e8_1f ^ data;
			ps55.data3e8_1f = data;
			if (chgbits & 0x02)
				VGA_DetermineMode();//setup vga drawing if the value is changed
			if (data & 0x08) vga.attr.disabled = 0;
			else vga.attr.disabled = 2;
			//LOG_MSG("PS55_ATTR: Write to port %x, idx %x, val %02xh (%d)", port, ps55.idx_3e8, data, data);
			return;
		default:
			break;
		}
		//LOG_MSG("PS55_ATTR: Write to port %x, idx %x, val %02xh (%d)", 0x3c0, ps55.idx_3e8, data, data);
		write_p3c0(port, ps55.idx_3e8, 1);
		write_p3c0(port, data, 1);
	}
}

void PS55_GC_Index_Write(Bitu port, Bitu val, Bitu len) {
	//if (len == 2) LOG_MSG("PS55_GC: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
	//else LOG_MSG("PS55_GC: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
	Bitu data = 0;
	if (len == 2) {//data addr
		data = val >> 8 ;
		val &= 0xff;//idx
	}
	switch (port) {
	case 0x3e0://?
		//LOG_MSG("PS55: Write to port %x, idx %x, val %02xh (%d)", port, val, data, data);
		ps55.idx_3e1 = val;
		break;
	case 0x3e2://Font Buffer Registers (undoc)
		//LOG_MSG("PS55: Write to port %x, idx %x, val %02xh (%d)", port, val, data, data);
		ps55.idx_3e3 = val;
		break;
	case 0x3e4://CRT Controller Registers (undoc)
		//LOG_MSG("PS55_CRTC: Write to port %x, idx %x, val %02xh (%d)", port, ps55.idx_3e5, data, data);
		ps55.idx_3e5 = val;
		vga_write_p3d4(port, val, 1);
		break;
	//case 0x3e8://Attribute Controller Registers (undoc)
	// Moved to PS55_ATTR_Write function
	//	break;
	case 0x3ea://Graphics Controller Registers
		//LOG_MSG("PS55_GC: Write to port %x, idx %x, val %02xh (%d)", port, ps55.idx_3eb, data, data);
		ps55.idx_3eb = val;
		write_p3ce(port, val, 1);
		break;
	}
	ps55.prevdata = NOAHDATA;
	ps55.nextret = NOAHDATA;
	if (len == 2) PS55_GC_Data_Write(port + 1, data, 1);
}

Bitu PS55_GC_Read(Bitu port, Bitu len) {
	Bitu ret = 0x00;
	switch (port) {
	case 0x3e0://?
		ret = ps55.idx_3e1;
		break;
	case 0x3e2://Font Buffer Registers (undoc)
		ret = ps55.idx_3e3;
		break;
	case 0x3e4://CRT Controller Registers (undoc)
		ret = ps55.idx_3e5;
		break;
	case 0x3e8://Attribute Controller Registers (undoc)
		ret = ps55.idx_3e8;
		break;
	case 0x3ea://Graphics Controller Registers
		ret = ps55.idx_3eb;
		break;
		//3E0: bit 3 Configuration (?)
		//     bit 0 In Operation / -Access
	case 0x3e1://??? (undocumented)
		switch (ps55.idx_3e1) {
		case 02://?
			ret = ps55.data3e1_02;
			break;
		case 0x03://bit 7: Display supports color/mono, bit 0: busy?
			ret = 0x80;//1000 0000
			break;
		case 0x08://bit 4: Gaiji RAM Access Enabled
			ret = ps55.gaijiram_access;
			break;
		case 0x0a://flags?
			ret = 0x07;//Bit2-0=(110 or 111): Display Adapter Memory Expansion Kit is not installed.
			break;
		case 0x0b://flags?
			ret = 0x00;
			break;
		default:
			//LOG_MSG("PS55: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3e1, len, ret);
			break;
		}
		//LOG_MSG("PS55: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3e1, len, ret);
		break;
	case 0x3e3://Font Buffer Registers (undoc)
		switch (ps55.idx_3e3) {
		case 0x0a://bit 0: Gaiji RAM Page 1/0
			ret = ps55.mem_bank;
			break;
		default:
			break;
		}
		//LOG_MSG("PS55_FONT: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3e3, len, ret);
		break;
	case 0x3e5://CRT Controller Registers (undocumented)
		
		ret = ps55.crtc_reg[ps55.idx_3e5 & 0x1f];
		//LOG_MSG("PS55_CRTC: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3e5, len, ret);
		break;
	case 0x3e9://Attribute Controller Registers (undocumented)
		switch (ps55.idx_3e8 & 0x1f) {
		case 0x14://line color for character mode
			ret = ps55.palette_line;
			break;
		case 0x1a://Cursor color
			ret = ps55.cursor_color;
			break;
		case 0x1b://Cursor blinking speed / invisible
			ret = ps55.cursor_options;
			break;
		case 0x1c://bit 3: input latched??? (DOS J4.0 and J5.0 use this)
			ret = (ps55.latch_3e8) ? 0x08 : 0x00;
			break;
		case 0x1e://???
			ret = 0x20;
			break;
		case 0x1f:
			ret = ps55.data3e8_1f;
			break;
		default:
			ret = read_p3c1(port, len);
			break;
		}
#ifdef C_HEAVY_DEBUG
		LOG_MSG("PS55_ATTR: Read from port %x, idx %2x, len %x, ret %2x", port, ps55.idx_3e8, len, ret);
#endif
		//ps55.latch_3e8 = (ps55.latch_3e8) ? false : true;//toggle latch
		//ps55.latch_3e8 = false;//reset latch
		break;
	case 0x3eb://Graphics Controller Registers
	case 0x3ec:
		switch (ps55.idx_3eb) {
		case 0x00://Set/Reset
			ret = ps55.set_reset;
		case 0x01://Enable Set/Reset
			ret = ps55.enable_set_reset;
		case 0x03://Data Rotate
			ret = vga.gfx.data_rotate;
			break;
		case 0x04://Read Map Select
			ret = vga.gfx.read_map_select;
			break;
		case 0x08://Bit Mask (Low)
			ret = ps55.bit_mask_low;
			break;
		case 0x09://Bit Mask (High)
			ret = ps55.bit_mask_high;
			break;
		case 0x0a://Map Mask
			ret = ps55.map_mask;
			break;
		default:
			ret = read_p3cf(port, len);
			//LOG_MSG("PS55_GC: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3eb, len, ret);
			break;
		}
		break;
	default:
		//LOG_MSG("PS55: Read from port %x, len %x, ret %x", port, len, ret);
		break;
	}
	if (len == 1 && ps55.nextret == NOAHDATA) {
		ps55.nextret = ret >> 8;
		ret &= 0xff;
	}
	else if (len == 1) {
		ret = ps55.nextret;
		ps55.nextret = NOAHDATA;
	}
	return ret;
}

void FinishSetMode_PS55(Bitu /*crtc_base*/, VGA_ModeExtraData* modeData) {
	VGA_SetupHandlers();
}

void DetermineMode_PS55() {
#ifdef C_HEAVY_DEBUG
	LOG_MSG("Mode? 3e1_2 %02X, 3e3_0 %02X, 3e5_1c %02X, 3e5_1f %02X, 3e8_38 %02X, 3e8_3f %02X",
		ps55.data3e1_02, ps55.mem_conf, ps55.crtc_reg[0x1c], ps55.crtc_reg[0x1f], ps55.text_mode03, ps55.data3e8_1f);
	LOG_MSG("3E1 0x00-f: %02X %02X %02X %02X %02X %02X %02X",
		ps55.p3e1_reg[0x00],
		ps55.p3e1_reg[0x01],
		ps55.p3e1_reg[0x02],
		ps55.p3e1_reg[0x03],
		ps55.p3e1_reg[0x08],
		ps55.p3e1_reg[0x0d],
		ps55.p3e1_reg[0x0e]);
	LOG_MSG("CRTC 0x0c-1f: %02X %02X %02X %02X %02X %02X %02X %02X",
		ps55.crtc_reg[0x0c],
		ps55.crtc_reg[0x19],
		ps55.crtc_reg[0x1a],
		ps55.crtc_reg[0x1b],
		ps55.crtc_reg[0x1c],
		ps55.crtc_reg[0x1d],
		ps55.crtc_reg[0x1e],
		ps55.crtc_reg[0x1f]);
#endif
	if (ps55.carden)//TODO: need to examine how to determine the mode
	{
		vga.vmemwrap = 512 * 1024;
		if (!(ps55.data3e1_02 & 0x01)) {
		//if (ps55.data3e8_3f & 0x02) {//this glitches the screen because the function is called after DOS clears the vmem.
		//if(1){//for debug
			if (~ps55.data3e1_02 & 0x04) {
			//if(1){
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 monochrome graphics.");
					VGA_SetMode(M_PS55_GFX_MONO);
			}
			else {
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 8-color graphics.");
					VGA_SetMode(M_PS55_GFX);
			}
		}
		else {
			if (ps55.mem_conf & 0x40) {
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 Mode 03 text.");
				VGA_SetMode(M_PS55_M3TEXT);
			}
			else {
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 text.");
				VGA_SetMode(M_PS55_TEXT);
			}
		}
	}
	else
	{//replica of VGA_DetermineMode() without S3 dependencies
		LOG_MSG("PS55_DetermineMode: Set videomode to VGA.");
		//Video memory wrapping for the display driver of DOS/V
		vga.vmemwrap = 256 * 1024;
		if (vga.attr.mode_control & 1) { // graphics mode
			if (vga.gfx.mode & 0x40)
				VGA_SetMode(M_VGA);
			else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
			else if ((vga.gfx.miscellaneous & 0x0c) == 0x0c) VGA_SetMode(M_CGA2);
			else VGA_SetMode(M_EGA);
		}
		else {
			VGA_SetMode(M_TEXT);
		}
	}
}
//backup
//vga.vmemwrap = 1024 * 1024;
//if (!(ps55.data3e1_02 & 0x01)) {
////if(1){
//	//if (ps55.data3e1_02 & 0x04) {
//	if(1){
//		LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 8-color graphics.");
//		VGA_SetMode(M_PS55_GFX);
//	}
//	else {
//		LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 monochrome graphics.");
//		VGA_SetMode(M_PS55_GFX_MONO);
//	}
//}
//else {
//	if (ps55.mem_conf & 0x40) {
//		LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 Mode 03 text.");
//		VGA_SetMode(M_PS55_M3TEXT);
//	}
//	else {
//		LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 text.");
//		VGA_SetMode(M_PS55_TEXT);
//	}
//}
/*
DOS K3.3
Mode 0 C8 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C
Mode 1 GA 3e1_2 10, 3e3_0 00, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E
Mode 3 CE 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C
Mode 4 GD 3e1_2 10, 3e3_0 2B, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E

DOS J4.0
Mode 0 C8 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C
Mode 1 GA 3e1_2 10, 3e3_0 2B, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E
Mode 3 CE 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C
Mode 4 GD 3e1_2 14, 3e3_0 2B, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E
VMX  3 C3 3e1_2 15, 3e3_0 6B, 3e5_1c 00, 3e5_1f 02, 3e8_38 01, 3e8_3f 0C
*/

//The PS/55 DA have half-width DBCS fonts placed at font code 400-4A5h (or more).
//This function generates them from DBCS fonts. 
//These fonts are used by DOS Bunsho Program.
void generate_HalfDBCS() {
	Bitu i = 0;
	while(tbl_halfdbcs[i][0] != 0)
	{
		for (int line = 0; line < 24; line++)
		{
			FontPattern dwpat;
			Bit8u pattern = 0;
			Bit8u pathalf = 0;
			Bit8u lineto = 0;
			Bitu copyto = (Bitu)tbl_halfdbcs[i][0];
			Bitu copyfrom = (Bitu)tbl_halfdbcs[i][1];
			dwpat.d = 0;
			if (tbl_halfdbcs[i][2] & 0x80) {
				//read SBCS font
				dwpat.b[2] = ps55font_24[0x98000 + copyfrom * 64 + line * 2 + 4];
				dwpat.b[1] = ps55font_24[0x98000 + copyfrom * 64 + line * 2 + 5];
				dwpat.d <<= 1;//shift left to remove space
			}
			else {
				//read DBCS font
				dwpat.b[2] = ps55font_24[copyfrom * 72 + line * 3];
				dwpat.b[1] = ps55font_24[copyfrom * 72 + line * 3 + 1];
				dwpat.b[0] = ps55font_24[copyfrom * 72 + line * 3 + 2];
			}
			switch (tbl_halfdbcs[i][2] & 0xf0) {
			case 0://shrink entire font bitmap
				lineto = line;
				//clear left half of font bitmap
				ps55font_24[copyto * 72 + lineto * 3] = 0;
				ps55font_24[copyto * 72 + lineto * 3 + 1] &= 0x0f;
				pattern = dwpat.b[2];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3] = pathalf;
				pathalf = 0;
				pattern = dwpat.b[1];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3] |= pathalf >> 4;
				pathalf = 0;
				pattern = dwpat.b[0];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3 + 1] |= pathalf;
				break;
			case 0x10://copy half of font bitmap
			case 0x90:
				lineto = line;
				//clear left half of font bitmap
				ps55font_24[copyto * 72 + lineto * 3] = 0;
				ps55font_24[copyto * 72 + lineto * 3 + 1] &= 0x0f;
				dwpat.d <<= tbl_halfdbcs[i][2] & 0x0f;
				ps55font_24[copyto * 72 + lineto * 3] = dwpat.b[2];
				ps55font_24[copyto * 72 + lineto * 3 + 1] |= dwpat.b[1] & 0xf0;
				break;
			case 0x20://1/4 DBCS font copy to left
				//set y baseline
				lineto = (line / 2 + (tbl_halfdbcs[i][2] & 0x0f));
				if (!(line & 0x01)) {
					//clear left half of font bitmap
					ps55font_24[copyto * 72 + lineto * 3] = 0;
					ps55font_24[copyto * 72 + lineto * 3 + 1] &= 0x0f;
				}
				//shrink entire font bitmap
				pattern = dwpat.b[2];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3] |= pathalf;
				pathalf = 0;
				pattern = dwpat.b[1];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3] |= pathalf >> 4;
				pathalf = 0;
				pattern = dwpat.b[0];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3 + 1] |= pathalf;
				break;
			case 0x30://1/4 DBCS font copy to right
				lineto = line / 2;
				if (!(line & 0x01)) {
					//clear right half of font bitmap
					ps55font_24[copyto * 72 + lineto * 3 + 1] &= 0xf0;
					ps55font_24[copyto * 72 + lineto * 3 + 2] = 0x0;
				}
				//shrink entire font bitmap
				pattern = dwpat.b[2];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3 + 1] |= pathalf >> 4;
				pathalf = 0;
				pattern = dwpat.b[1];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3 + 2] |= pathalf;
				pathalf = 0;
				pattern = dwpat.b[0];
				for (int i = 0; i < 4; i++) {
					pathalf |= (pattern << i) & (0x80 >> i);
				}
				ps55font_24[copyto * 72 + lineto * 3 + 2] |= pathalf >> 4;
				break;
			case 0xa0://1/4 SBCS font copy to left
				//set y baseline
				lineto = (line / 2 + (tbl_halfdbcs[i][2] & 0x0f));
				if (!(line & 0x01)) {
					//clear right half of font bitmap
					ps55font_24[copyto * 72 + lineto * 3] = 0;
					ps55font_24[copyto * 72 + lineto * 3 + 1] &= 0x0f;
				}
				//copy left half of bitmap
				ps55font_24[copyto * 72 + lineto * 3] |= dwpat.b[2];
				ps55font_24[copyto * 72 + lineto * 3 + 1] |= dwpat.b[1] & 0xf0;
				break;
			case 0xb0://1/4 SBCS font copy to right
				//set y baseline
				lineto = (line / 2 + (tbl_halfdbcs[i][2] & 0x0f));
				if (!(line & 0x01)) {
					//clear right half of font bitmap
					ps55font_24[copyto * 72 + lineto * 3 + 1] &= 0xf0;
					ps55font_24[copyto * 72 + lineto * 3 + 2] = 0;
				}
				//copy left half of bitmap
				ps55font_24[copyto * 72 + lineto * 3 + 1] |= dwpat.b[2] >> 4;
				ps55font_24[copyto * 72 + lineto * 3 + 2] |= dwpat.b[2] << 4;
				ps55font_24[copyto * 72 + lineto * 3 + 2] |= dwpat.b[1] >> 4;
				break;
			}
		}
		i++;
	}
}

void SetClock_PS55(Bitu which, Bitu target) {
	VGA_StartResize();
}

Bitu GetClock_PS55() {
	if (ps55.carden)
		return 58000000 / 2;//58.000MHz (interlaced)
	Bitu clock = (vga.misc_output >> 2) & 3;
	if (clock == 0)//replication
		clock = 25175000;
	else// if (clock == 1)
		clock = 28322000;
	return clock;
}

bool AcceptsMode_PS55(Bitu mode) {
	if(!ps55.carden) return VideoModeMemSize(mode) < vga.vmemsize;
	//only character mode is supported
	if (mode == 0x08 || mode == 0x0e)
		return VideoModeMemSize(mode) < vga.vmemsize;
	else
		return false;
}

void PS55_WakeUp(void) {
	ps55.carden = true;
	//DisableVGA();
	//Clear CRTC registers that are ignored(CRTC index 7, 8?, 18)
	vga.crtc.overflow = 0;
	vga.config.line_compare = 0xFFF;
	MEM_ResetPageHandler(VGA_PAGE_A0, 32);//release VGA handlers
	//Never used, but they are reserved according to the reference
	//IO_RegisterWriteHandler(0x3d0, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d0, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d1, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d1, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d2, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d2, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d3, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d3, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d4, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d4, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d5, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d5, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d6, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d6, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d7, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d7, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d8, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d8, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3d9, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3d9, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3da, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3da, &PS55_CRTC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3db, &PS55_CRTC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3db, &PS55_CRTC_Read, IO_MB | IO_MW);

	IO_RegisterWriteHandler(0x3e0, &PS55_GC_Index_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3e0, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3e1, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3e1, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3e2, &PS55_GC_Index_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3e2, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3e3, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3e3, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3e4, &PS55_GC_Index_Write, IO_MB | IO_MW); //CRTC (Index)
	IO_RegisterReadHandler(0x3e4, &PS55_GC_Read, IO_MB | IO_MW); //CRTC
	IO_RegisterWriteHandler(0x3e5, &PS55_GC_Data_Write, IO_MB | IO_MW); //CRTC (Data)
	IO_RegisterReadHandler(0x3e5, &PS55_GC_Read, IO_MB | IO_MW); //CRTC
	//IO_RegisterWriteHandler(0x3e6, &PS55_GC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3e6, &PS55_GC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3e7, &PS55_GC_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3e7, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3e8, &PS55_ATTR_Write, IO_MB | IO_MW);//Attribute (Index, Data)
#ifdef C_HEAVY_DEBUG
	IO_RegisterReadHandler(0x3e8, &PS55_CRTC_Read, IO_MB | IO_MW);//FOR DEBUG
#endif
	IO_RegisterWriteHandler(0x3e9, &PS55_ATTR_Write, IO_MB | IO_MW);//Attribute (Data)
	IO_RegisterReadHandler(0x3e9, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3ea, &PS55_GC_Index_Write, IO_MB | IO_MW); //GFX (Index)
	IO_RegisterReadHandler(0x3ea, &PS55_GC_Read, IO_MB | IO_MW); //GFX
	IO_RegisterWriteHandler(0x3eb, &PS55_GC_Data_Write, IO_MB | IO_MW); //GFX (Data)
	IO_RegisterReadHandler(0x3eb, &PS55_GC_Read, IO_MB | IO_MW); //GFX
	//IO_RegisterWriteHandler(0x3eb, &write_p3cf, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3eb, &read_p3cf, IO_MB | IO_MW);

	IO_RegisterWriteHandler(0x3ec, &PS55_GC_Index_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ec, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3ed, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ed, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3ee, &PS55_GC_Index_Write, IO_MB | IO_MW);//Attribute?
	IO_RegisterReadHandler(0x3ee, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3ef, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ef, &PS55_GC_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3ef, write_p3c0, IO_MB); // for PS/55
	//IO_RegisterReadHandler(0x3ef, read_p3c0, IO_MB); // for PS/55
	//IO_RegisterReadHandler(0x3ee, read_p3c1, IO_MB); // for PS/55

	DetermineMode_PS55();
	LOG_MSG("PS/55 wakes up!!");
}

//Setup MicroChannel(tm) bus and adapters
void SVGA_Setup_PS55(void) {
	ps55.prevdata = NOAHDATA;
	ps55.nextret = NOAHDATA;
	generate_HalfDBCS();
	ps55.gaiji_ram = new Bit8u[256 * 1024];//256 KB for Gaiji RAM

	IO_RegisterWriteHandler(0x94, &write_p94, IO_MB);
	IO_RegisterReadHandler(0x94, &read_p94, IO_MB);
	IO_RegisterWriteHandler(0x96, &write_p96, IO_MB);
	IO_RegisterReadHandler(0x96, &read_p96, IO_MB);
	IO_RegisterWriteHandler(0x100, &MCA_Card_Reg_Write, IO_MB);
	IO_RegisterReadHandler(0x100, &MCA_Card_Reg_Read, IO_MB);
	IO_RegisterWriteHandler(0x101, &MCA_Card_Reg_Write, IO_MB);
	IO_RegisterReadHandler(0x101, &MCA_Card_Reg_Read, IO_MB);
	IO_RegisterWriteHandler(0x102, &MCA_Card_Reg_Write, IO_MB);
	IO_RegisterReadHandler(0x102, &MCA_Card_Reg_Read, IO_MB);
	IO_RegisterWriteHandler(0x103, &MCA_Card_Reg_Write, IO_MB);
	IO_RegisterReadHandler(0x103, &MCA_Card_Reg_Read, IO_MB);
	IO_RegisterWriteHandler(0x104, &MCA_Card_Reg_Write, IO_MB);
	IO_RegisterReadHandler(0x104, &MCA_Card_Reg_Read, IO_MB);
	IO_RegisterWriteHandler(0x105, &MCA_Card_Reg_Write, IO_MB);
	IO_RegisterReadHandler(0x105, &MCA_Card_Reg_Read, IO_MB);

	svga.set_video_mode = &FinishSetMode_PS55;
	svga.determine_mode = &DetermineMode_PS55;
	//svga.set_clock = &SetClock_PS55;
	svga.get_clock = &GetClock_PS55;
	svga.accepts_mode = &AcceptsMode_PS55;

	vga.vmemsize = 512 * 1024; // Cannot figure how this was supposed to work for the real card
}
