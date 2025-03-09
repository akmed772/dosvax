/*
Copyright (c) 2021-2024 akm
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
#include "paging.h"
#include "inout.h"

#include "ps55.h"//for PS/55
#include "jsupport.h"
//#include "regs.h"

PS55Registers ps55;
MCARegisters mca;
extern VGA_Type vga;//vga.cpp

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
void PS55_GC_Data_Write(Bitu port, Bitu val, Bitu len);

void PS55_GCR_Write(Bitu val);
Bitu PS55_GC_Read(Bitu port, Bitu len);

void EnableGaijiRAMHandler();
void DisableGaijiRAMHandler();

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
			// ----Å´for DA1----
			//addr |= ((Bitu)ps55.mem_bank << 16);
			// ----Å´for DA2----
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
			addr &= 0x3FFFF;//safety access
			//LOG_MSG("PS55_MemHnd_G: Read from mem %x, bank %x, chr %x (%x), val %x", ps55.mem_select, ps55.mem_bank, addr / 128, addr, ps55.gaiji_ram[addr]);
			return ps55.gaiji_ram[addr];
			break;
		case 0x10://Font ROM
			// ----Å´for DA1----
			//LOG_MSG("PS55_MemHnd_F: Read from mem %x, bank %x, chr %x (%x), val %x", ps55.mem_select, ps55.mem_bank, addr / 72, addr, ps55.font_da1[addr]);
			//return ps55.font_da1[addr];
			// ----Å´for DA2----
			//LOG_MSG("PS55_MemHnd: Read from mem %x, bank %x, chr %x (%x), val %x", ps55.mem_select, ps55.mem_bank, addr / 72, addr, ps55font_24[addr]);
			if (addr > DBCS24_LEN) return 0xff;
			return ps55font_24[addr];
			break;
		default:
			//LOG_MSG("PS55_MemHnd: Read from mem %x, bank %x, addr %x", ps55.mem_select, ps55.mem_bank, addr);
			return 0xff;//invalid memory access
			break;
		}
	}
	void writeHandler(PhysPt addr, Bit8u val) {
		//if (!(ps55.gaijiram_access & 0x10))
		//{
		//	LOG_MSG("PS55_MemHnd: Failure to write to mem %x, addr %x, val %x", ps55.mem_select, addr, val);
		//	return;//safety
		//}
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
		case 0xb0://Gaiji RAM 1011 0000
			//LOG_MSG("PS55_MemHnd_G: Write to mem %x, chr %x (%x), val %x", ps55.mem_select, addr / 128, addr, val);
			addr &= 0x3FFFF;//safety access
			ps55.gaiji_ram[addr] = val;
			break;
		case 0x10://Font ROM  0001 0000
			//LOG_MSG("PS55_MemHnd: Attempted to write Font ROM!!!");
			//LOG_MSG("PS55_MemHnd_F: Failure to write to mem %x, addr %x, val %x", ps55.mem_select, addr, val);
			//Read-Only
			break;
		case 0x00:
			//if (addr >= PS55_BITBLT_MEMSIZE)
			//{
			//	LOG_MSG("PS55_MemHnd: Failure to write to bitblt mem addr %x, val %x", ps55.mem_select, addr, val);
			//	return;
			//}
			//ps55.bitblt.payload[addr] = val;
			if (ps55.bitblt.payload_addr >= PS55_BITBLT_MEMSIZE)
				LOG_MSG("PS55_MemHnd: Failure to write to bitblt mem addr %x, val %x", ps55.mem_select, addr, val);
			else {
				ps55.bitblt.payload[ps55.bitblt.payload_addr] = val;
				ps55.bitblt.payload_addr++;
			}
			break;
		default:
			//LOG_MSG("PS55_MemHnd_F: Default passed!!!");
			LOG_MSG("PS55_MemHnd: Failure to write to mem %x, addr %x, val %x", ps55.mem_select, addr, val);
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

//MicroChannel NVRAM Index LSB (Hex 0074)
void write_p74(Bitu port, Bitu val, Bitu len) {
#ifdef _DEBUG
	LOG_MSG("MCA_p74: Write to port %x, val %x, len %x", port, val, len);
#endif
	mca.idx_nvram = val;
}

//MicroChannel NVRAM Data (Hex 0076)
Bitu read_p76(Bitu port, Bitu len) {
	Bitu ret = 0xff;
#ifdef _DEBUG
	LOG_MSG("MCA_p76: Read from port %x, len %x", port, len);
#endif
	switch (mca.idx_nvram) {
	case 0x0://
		ret = 0xfe;
		break;
	case 0x1://
		ret = 0xef;
		break;
	default:
		break;
	}
	return ret;
}
//I/O Card Selected Feedback Register (Hex 0091) : Unused
//I/O System Board Enable/Setup Regiseter (Hex 0094)
void write_p94(Bitu port, Bitu val, Bitu len) {
#ifdef _DEBUG
	LOG_MSG("MCA_p94: Write to port %x, val %x, len %x", port, val, len);
#endif
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
#ifdef _DEBUG
	LOG_MSG("MCA_p96: Write to port %x, val %x, len %x", port, val, len);
#endif
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
#ifdef _DEBUG
	LOG_MSG("MCA_p96: Read from port %x, len %x", port, len);
#endif
	return 0;
}

void EnableVGA(void) {
	vga.vmemsize = 256 * 1024;
	VGA_SetupMisc();
	VGA_SetupDAC();
	VGA_SetupGFX();
	VGA_SetupSEQ();
	VGA_SetupAttr();
	VGA_SetupOther();
	LOG_MSG("VGA I/O handlers enabled!!");
	VGA_DetermineMode();
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

	//MEM_ResetPageHandler(0xc0000 / 4096, 8);//setup ram at 0xc0000-0xc8000
	//PAGING_ClearTLB();
	//for (Bitu i = 0; i < 0x8000; i++) {
	//	mem_writeb(0xc0000 + i, (Bit8u)0);
	//}

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
#ifdef C_HEAVY_DEBUG
	LOG_MSG("MCA_p10x: Write to port %x, val %x, len %x", port, val, len);
#endif
	if (mca.vgasetupen) {//I/O 94h bit 5 is on
		switch (port) {
		case 0x102://POS Register 2 (Hex 102)
			if (!(val & 0x01))
				DisableVGA();//disable VGA original I/O port handler
			else
			{
				EnableVGA();
			}
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
			else
				PS55_Suspend();//disable PS/55 DA
			;
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
	 [Identification of Display Adapter]
	POS ID	SYS ID	
	EFFFh	*		Display Adapter (PS/55 Model 5571-S0A) [Toledo]
	EFFEh	*		Display Adapter II (I/O 3E0:0A = xx0x xxxx) [Atlas]
	|-		FFF2h	Display Adapter B2 (I/O 3E0:0A = xx1x xxxx) (PS/55 Model 5530Z-SX)
	|-		FDFEh	Display Adapter B2 (I/O 3E0:0A = xx1x xxxx) (PS/55 Model 5550-V2)
	|-		*		Display Adapter III,V (I/O 3E0:0A = xx1x xxxx)
	ECECh	FF4Fh	Display Adapter B1 (PS/55Z 5530Z-SX) [Atlas-KENT]
	|-		*		Display Adapter IV
	ECCEh	*		Display Adapter IV
	901Fh	*		Display Adapter A2
	901Dh	*		Display Adapter A1 [ATLAS II]
	901Eh	*		Plasma Display Adapter
	EFD8h	*		DBCS Display Adapter/J [Atlas-SP2]

	 [Japanese DOS and Display Adapter compatibility]
	| POS ID     | Name                        | K3.31 | J4.04 | J5.02 | OS/2 J1.3 |
	|------------|-----------------------------|:-----:|:-----:|:-----:|:---------:|
	| EFFFh      | Display Adapter [Toledo]    | X     |       |       |           |
	| FFEDh      | ? [Atlas EVT]               | X     |       |       |           |
	| FFFDh      | ? [LDT EVT]                 | X     |       |       |           |
	| EFFEh      | Display Adapter II,III,V,B2 | X     | X     | X     | X         |
	| E013h      | ? [LDT]                     | X     | X     | X     | X         |
	| ECCEh      | Display Adapter IV          |       | X     | X     | X         |
	| ECECh      | Display Adapter IV,B1       |       | X     | X     | X         |
	| 9000-901Fh | Display Adapter A1,A2       |       | X     | X     |           |
	| EFD8h      | Display Adapter /J          |       |       | X     | X         |
	
	[Others]
	8FDA - XGA-2 Video Adapter
	8FDB - XGA Video Adapter
	EFFD - IBM VGA Adapter
	90EF - Display Adapter C0
	90FD - Font / LCD Adapter
	EFDE - PS/55-Secondary Display Adapter
	EFEB - ? (appeared in OS/2 J1.3) [Atlas IDS]
*/

//I/O Read POS Regs to return card ID 0xEFFE (Display Adapter II)
Bitu MCA_Card_Reg_Read(Bitu port, Bitu len) {
	Bitu ret = 0xff;
	if ((mca.cardsetupen) && (mca.cardsel == 0)) {
		switch (port) {
		case 0x100://POS Register 0 - Adapter Identification Byte (Low byte)
			ret = 0xfe;
			//ret = 0xff;
			break;
		case 0x101://POS Register 1 - Adapter Identification Byte (High byte)
			ret = 0xef;
			break;
		case 0x102://POS Register 2 - Option Select Data Byte 1
			ret = (ps55.carden) ? 0x41 : 0x40;//Bit 6: Color Mon, Bit 0: Card Enable
			break;
		default:
			break;
		}
	}
	if ((mca.mothersetupen)) {//I/O 94h bit 7 is on
		switch (port) {
		case 0x102://System Board POS Register 2 - External I/O Configuration
			//Bit 7: Disable Parallel Port Extended Mode
			//Bit 6, 5: Parallel Port Select
			//Bit 4: Enable Parallel Port
			//Bit 3: Serial Port Select
			//Bit 2: Enable Serial Port
			//Bit 1: Enable Diskette Drive Interface
			//Bit 0: Enable System Board
			ret = 0xaf;//1010 1111
			break;
		default:
			break;
		}
	}
#ifdef C_HEAVY_DEBUG
	LOG_MSG("MCA_p10x: Read from port %x, len %x, ret %x", port, len, ret);
#endif
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
void EnableGaijiRAMHandler()
{
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

void PS55_UpdatePixel(const Bitu start, const VGA_Latch pixels) {
	((Bit32u*)vga.mem.linear)[start] = pixels.d;
	Bit8u* write_pixels = &vga.fastmem[start << 3];
	Bit32u colors0_3, colors4_7;
	VGA_Latch temp; temp.d = (pixels.d >> 4) & 0x0f0f0f0f;
	colors0_3 =
		Expand16Table[0][temp.b[0]] |
		Expand16Table[1][temp.b[1]] |
		Expand16Table[2][temp.b[2]] |
		Expand16Table[3][temp.b[3]];
	*(Bit32u*)write_pixels = colors0_3;
	temp.d = pixels.d & 0x0f0f0f0f;
	colors4_7 =
		Expand16Table[0][temp.b[0]] |
		Expand16Table[1][temp.b[1]] |
		Expand16Table[2][temp.b[2]] |
		Expand16Table[3][temp.b[3]];
	*(Bit32u*)(write_pixels + 4) = colors4_7;
}

typedef union {
	Bit32u d;
	Bit8u b[4];
} PS55_VidSeq32;

void PS55_WritePlaneDataWithBitmask(Bitu destvmemaddr, const Bit16u mask, VGA_Latch srclatch1, VGA_Latch srclatch2, VGA_Latch srclatch3, VGA_Latch srclatch4)
{
	VGA_Latch destlatch1, destlatch2, destlatch3, destlatch4;
	destvmemaddr &= 0xfffffffe; /* align to word address to work bit shift correctly */
	Bit32u maxvmemaddr = (vga.vmemsize - 1) >> 2;//divided by 4 because below code use dword memory access
	destlatch1.d = ((Bit32u*)vga.mem.linear)[(destvmemaddr) & maxvmemaddr];
	destlatch2.d = ((Bit32u*)vga.mem.linear)[(destvmemaddr + 1) & maxvmemaddr];
	//destlatch3.d = ((Bit32u*)vga.mem.linear)[destvmemaddr + 2];
	//destlatch4.d = ((Bit32u*)vga.mem.linear)[destvmemaddr + 3];
	// 
	PS55_VidSeq32 mask32in; mask32in.d = (Bit32u)mask;//[3]0x00 [2]0x00 [1]0xFF [0]0x3F (mask is 00111111 11111111, but byte order is opposite)
	PS55_VidSeq32 mask32; mask32.d = 0;
	PS55_VidSeq32 masksh32; masksh32.d = 0xffff0000;
	mask32.b[3] = mask32in.b[0];
	mask32.b[2] = mask32in.b[1];
	//mask32.b[1] = mask32in.b[2];
	//mask32.b[0] = mask32in.b[3];
	mask32.d &= 0xffff0000;//[3]0x3F [2]0xFF [1]0x00 [0]0x00
	//if (ps55.data3ea_0b < 0x08)
	//LOG_MSG("dest %05X x %4d y %3d | src          mask     shift=%x", destvmemaddr, (destvmemaddr % vga.config.scan_len) * 8 ,(destvmemaddr >> 1) / vga.config.scan_len, bitshiftin_destr);
	for (int i = 0; i < 4; i++)
	{
		// Clear vmem (vmem AND ~bit-mask), copy tile data, bit-masking it, and write it to vmem (vmem OR masked data)
		//srclatch1.b[i] &= masksrc1;
		//srclatch2.b[i] &= masksrc2;
		PS55_VidSeq32 srctemp32; srctemp32.d = 0;
		PS55_VidSeq32 destop32, desttemp32;
		srctemp32.b[3] = srclatch1.b[i];
		srctemp32.b[2] = srclatch2.b[i];
		srctemp32.b[1] = srclatch3.b[i];
		srctemp32.b[0] = srclatch4.b[i];
		destop32.b[3] = destlatch1.b[i];
		destop32.b[2] = destlatch2.b[i];
		//destop32.b[1] = destlatch3.b[i];
		//destop32.b[0] = destlatch4.b[i];
		//desttemp32.d = destop32.d;
		//LOG_MSG("                             %08x", srctemp32.d);
		if (ps55.bitblt.bitshift_destr) {
			srctemp32.d >>= ps55.bitblt.bitshift_destr;
			srctemp32.d <<= 16;
		}
		if (ps55.bitblt.raster_op & 0x2010) /* NOT Src or NOT Pattern */
			srctemp32.d = ~srctemp32.d & mask32.d;
		if (ps55.bitblt.raster_op & 0x20) /* Dest NOT */
			destop32.d = (~destop32.d & mask32.d) | (destop32.d & ~mask32.d);
		//if(ps55.data3ea_0b < 0x08)
		//	LOG_MSG("%02x %02x %02x %02x          | %02x %02x %02x %02x ,%02x %02x %02x %02x (Plane %d, rop %d, 3EA-0B %d)", destlatch1.b[i], destlatch2.b[i], destlatch3.b[i], destlatch4.b[i],
		//	srctemp32.b[3], srctemp32.b[2], srctemp32.b[1], srctemp32.b[0], mask32.b[3], mask32.b[2], mask32.b[1], mask32.b[0], i, vga.config.raster_op, ps55.data3ea_0b);
		switch (ps55.bitblt.raster_op & 0x03) {
		case 0x00:	/* None */
					//return (input & mask) | (vga.latch.d & ~mask);
			destop32.d &= ~mask32.d;
			destop32.d |= srctemp32.d & mask32.d;
			break;
		case 0x01:	/* AND */
					//return (input | ~mask) & vga.latch.d;
			destop32.d &= srctemp32.d | ~mask32.d;
			break;
		case 0x02:	/* OR */
					//return (input & mask) | vga.latch.d;
			destop32.d |= srctemp32.d & mask32.d;
			break;
		case 0x03:	/* XOR */
					//return (input & mask) ^ vga.latch.d;
			destop32.d ^= srctemp32.d & mask32.d;
			break;
		}
		destlatch1.b[i] = destop32.b[3];
		destlatch2.b[i] = destop32.b[2];
		//destlatch3.b[i] = destop32.b[1];
		//destlatch4.b[i] = destop32.b[0];
		//if (ps55.data3ea_0b < 0x08) {
		//	LOG_MSG("%02x %02x %02x %02x", destlatch1.b[i], destlatch2.b[i], destlatch3.b[i], destlatch4.b[i]);
		//}
	}
	PS55_UpdatePixel(destvmemaddr & maxvmemaddr, destlatch1);
	PS55_UpdatePixel((destvmemaddr + 1) & maxvmemaddr, destlatch2);
}
void PS55_DrawColorWithBitmask(Bitu destvmemaddr, const Bit8u color, Bit16u mask)
{
	VGA_Latch srclatch1, srclatch2, srclatch3, srclatch4;

	srclatch1.d = FillTable[color & 0x0f];
	srclatch2.d = FillTable[color & 0x0f];
	srclatch3.d = FillTable[color & 0x0f];
	srclatch4.d = FillTable[color & 0x0f];

	PS55_WritePlaneDataWithBitmask(destvmemaddr, mask, srclatch1, srclatch2, srclatch3, srclatch4);
}
void PS55_CopyPlaneDataWithBitmask(Bitu srcvmemaddr, Bitu destvmemaddr, Bit16u mask)
{
	VGA_Latch srclatch1, srclatch2, srclatch3, srclatch4;
	//wrap around for safe memory access
	Bit32u maxvmemaddr = (vga.vmemsize - 1) >> 2;//divided by 4 because below code use dword memory access
	srcvmemaddr &= 0xfffffffe;

	srclatch1.d = ((Bit32u*)vga.mem.linear)[(srcvmemaddr + 0) & maxvmemaddr];
	srclatch2.d = ((Bit32u*)vga.mem.linear)[(srcvmemaddr + 1) & maxvmemaddr];
	srclatch3.d = ((Bit32u*)vga.mem.linear)[(srcvmemaddr + 2) & maxvmemaddr];
	srclatch4.d = ((Bit32u*)vga.mem.linear)[(srcvmemaddr + 3) & maxvmemaddr];

	PS55_WritePlaneDataWithBitmask(destvmemaddr, mask, srclatch1, srclatch2, srclatch3, srclatch4);
}
void DisableGaijiRAMHandler()
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
			//LOG_MSG("PS55_MemHnd: Page handler not restored -");
		}
	}
	PAGING_ClearTLB();
	//for PS/55 DA BitBLT operations
	Bitu value32;
	Bit64u value64;
	if (ps55.mem_select == 0)
	{
		//LOG_MSG("Line Length: %d %d\n", vga.draw.address_add, vga.config.scan_len);
		//LOG_MSG("BitBlt memory:\n");
		//for (int j = 0; j < PS55_BITBLT_MEMSIZE / 8; j++)
		//{
		//	int i = j * 8;
		//	LOG_MSG("%02x %02x %02x %02x %02x %02x %02x %02x ", ps55.bitblt.payload[i], ps55.bitblt.payload[i+1], ps55.bitblt.payload[i+2], ps55.bitblt.payload[i+3], 
		//		ps55.bitblt.payload[i+4], ps55.bitblt.payload[i+5], ps55.bitblt.payload[i+6], ps55.bitblt.payload[i+7]);
		//}
		int i = 0;
		while (i < PS55_BITBLT_MEMSIZE)
		{
			switch (ps55.bitblt.payload[i]) {
			case 0x88:
			case 0x89:
			case 0x95:
				value32 = ps55.bitblt.payload[i + 3];
				value32 <<= 8;
				value32 |= ps55.bitblt.payload[i + 2];
				//LOG_MSG("[%02x] %02x: %04x (%d)", ps55.bitblt.payload[i], ps55.bitblt.payload[i + 1], value32, value32);
				ps55.bitblt.reg[ps55.bitblt.payload[i + 1]] = value32;
				i += 3;
				break;
			case 0x91:
				value32 = ps55.bitblt.payload[i + 5];
				value32 <<= 8;
				value32 |= ps55.bitblt.payload[i + 4];
				value32 <<= 8;
				value32 |= ps55.bitblt.payload[i + 3];
				value32 <<= 8;
				value32 |= ps55.bitblt.payload[i + 2];
				//LOG_MSG("[%02x] %02x: %08x (%d)", ps55.bitblt.payload[i], ps55.bitblt.payload[i + 1], value32, value32);
				ps55.bitblt.reg[ps55.bitblt.payload[i + 1]] = value32;
				i += 5;
				break;
			case 0x99:
				value64 = ps55.bitblt.payload[i + 7];
				value64 <<= 8;
				value64 = ps55.bitblt.payload[i + 6];
				value64 <<= 8;
				value64 = ps55.bitblt.payload[i + 5];
				value64 <<= 8;
				value64 |= ps55.bitblt.payload[i + 4];
				value64 <<= 8;
				value64 |= ps55.bitblt.payload[i + 3];
				value64 <<= 8;
				value64 |= ps55.bitblt.payload[i + 2];
				ps55.bitblt.reg[ps55.bitblt.payload[i + 1]] = value64;
				//LOG_MSG("[%02x] %02x: %02x %02x %02x %02x %02x %02x", ps55.bitblt.payload[i], ps55.bitblt.payload[i + 1], ps55.bitblt.payload[i + 2], ps55.bitblt.payload[i + 3],
				//	ps55.bitblt.payload[i + 4], ps55.bitblt.payload[i + 5], ps55.bitblt.payload[i + 6], ps55.bitblt.payload[i + 7]);
				i += 7;
				break;
			case 0x00:
				break;
			default:
				LOG_MSG("PS55_ParseBLT: Unknown PreOP!");
				break;
			}
			i++;
		}
		//[89] 20: 0001 (1) then execute payload and clear it
		if (ps55.bitblt.reg[0x20] & 0x1)
		{
#if C_DEBUG
			//begin for debug
			for (i = 0; i < PS55_DEBUG_BITBLT_SIZE; i++) {
				//if(ps55.bitblt.reg[i] != 0xfefe && ps55.bitblt.reg[i] != 0xfefefefe) LOG_MSG("%02x: %04x (%d)",i, ps55.bitblt.reg[i], ps55.bitblt.reg[i]);
			}
			for (i = 0; i < PS55_DEBUG_BITBLT_SIZE; i++) {
				ps55.bitblt.debug_reg[PS55_DEBUG_BITBLT_SIZE * ps55.bitblt.debug_reg_ip + i] = ps55.bitblt.reg[i];
			}
			ps55.bitblt.debug_reg[PS55_DEBUG_BITBLT_SIZE * (ps55.bitblt.debug_reg_ip + 1) - 1] = ps55.data3ea_0b;
			ps55.bitblt.debug_reg_ip++;
			//end for debug
#endif

			ps55.bitblt.bitshift_destr = ((ps55.bitblt.reg[0x3] >> 4) & 0x0f);//set bit shift
			ps55.bitblt.raster_op = ps55.bitblt.reg[0x0b];
			//if (~ps55.data3ea_0b & 0x08) {//for debugging 3EA 0b = 00h
			//	LOG_MSG("---------------------------------------------");
			//}
			//if (ps55.data3ea_0b >= 0x08)
			//	;
			//else
			if (ps55.bitblt.reg[0x5] == 0x43) {//Draw a line
				Bit16s dest_x = (ps55.bitblt.reg[0x32] & 0xffff);
				Bit16s dest_y = (ps55.bitblt.reg[0x34] & 0xffff);
				Bit16s wx1 = (ps55.bitblt.reg[0x32]) >> 16;
				Bit16s wx2 = (ps55.bitblt.reg[0x33]) >> 16;
				Bit16s wy1 = (ps55.bitblt.reg[0x34]) >> 16;
				Bit16s wy2 = (ps55.bitblt.reg[0x35]) >> 16;
				Bit16u size_x = abs((Bit16s)(ps55.bitblt.reg[0x33] & 0xffff) - dest_x);
				Bit16u size_y = abs((Bit16s)(ps55.bitblt.reg[0x35] & 0xffff) - dest_y);
				Bits countend, d;
				ps55.bitblt.bitshift_destr = 0;
				if (ps55.bitblt.reg[0x2D] & 0x04) /* dX > dY */
				{
					d = 2 * size_y - size_x;
					countend = size_x;
				}
				else
				{
					d = 2 * size_x - size_y;
					countend = size_y;
				}
				Bits x = dest_x;
				Bits y = dest_y;
				Bits destaddr, pixelmask;

				for (Bits count = 0; count <= countend; count++) {
					destaddr = y * (ps55.crtc_reg[0x13] * 2) + x / 8;
					pixelmask = x % 16;
					if (pixelmask >= 8)
						pixelmask = (0x8000 >> (pixelmask - 8));
					else
						pixelmask = (0x80 >> pixelmask);

					/* check the current position is inside the window */
					if (x < wx1 || x > wx2
						|| y < wy1 || y > wy2)
						;
					else
						PS55_DrawColorWithBitmask(destaddr, ps55.bitblt.reg[0x0], pixelmask);

					/* calculate the next position with Bresenham's line algorithm */
					if (ps55.bitblt.reg[0x2D] & 0x04) { /* dX > dY */
						if (ps55.bitblt.reg[0x2D] & 0x02) {
							x++;
						}
						else {
							x--;
						}
						if (d >= 0) {
							d -= (2 * size_x);
							if (ps55.bitblt.reg[0x2D] & 0x01) {
								y++;
							}
							else {
								y--;
							}
						}
						d += (2 * size_y);
					}
					else {
						if (ps55.bitblt.reg[0x2D] & 0x01) {
							y++;
						}
						else {
							y--;
						}
						if (d >= 0) {
							d -= (2 * size_y);
							if (ps55.bitblt.reg[0x2D] & 0x02) {
								x++;
							}
							else {
								x--;
							}
						}
						d += (2 * size_x);
					}
				}
			}
			else if ((ps55.bitblt.reg[0x5] & 0xfff0) == 0x40 && ps55.bitblt.reg[0x3D] == 0) {//Fill a rectangle (or draw a line)
				Bitu destaddr_begin = ps55.bitblt.reg[0x29];
				Bit8u frontcolor = ps55.bitblt.reg[0x0] & 0x0f;
				for (Bitu y = 0; y < ps55.bitblt.reg[0x35]; y++)
				{
					for (Bitu x = 0; x < ps55.bitblt.reg[0x33]; x++)
					{
						Bitu destaddr_now = destaddr_begin + x * 2 + y * vga.config.scan_len * 2 + 2;
						////Set color
						//PS55_GC_Index_Write(0x3ea, 0x0a, 1);
						//PS55_GCR_Write(ps55.bitblt.reg[0x0] & 0x0f);//0a Map Mask (set color)
						//Transfer masked bitmap data to vmem
						//vga.config.raster_op = 2;//05 Graphics mode (000x x000  00 = unmodified, 01 = AND, 10 = OR, 11 = XOR)
						if (x == 0) {
							PS55_DrawColorWithBitmask(destaddr_now, frontcolor, ps55.bitblt.reg[0x8]);
						}
						else if (x == ps55.bitblt.reg[0x33] - 1) {
							PS55_DrawColorWithBitmask(destaddr_now, frontcolor, ps55.bitblt.reg[0x9]);
						}
						else {
							PS55_DrawColorWithBitmask(destaddr_now, frontcolor, 0xffff);
						}
					}
				}
			}
			else if ((ps55.bitblt.reg[0x5] & 0xfff0) == 0x1040 && ps55.bitblt.reg[0x3D] == 0x40) {//Tiling a rectangle (transfer tile data multiple times)
				Bitu destaddr_begin = ps55.bitblt.reg[0x29];
				Bitu tileaddr_begin = ps55.bitblt.reg[0x2B];
				//LOG_MSG("bitblt tiling: wt %d, ht %d, wd %d, wh %d, 3d %d", ps55.bitblt.reg[0x23], ps55.bitblt.reg[0x28], ps55.bitblt.reg[0x33], ps55.bitblt.reg[0x35], ps55.bitblt.reg[0x3d]);
				for (Bitu y = 0; y < ps55.bitblt.reg[0x35]; y++)
				{
					for (Bitu x = 0; x < ps55.bitblt.reg[0x33]; x++)
					{
						Bitu destaddr_now = destaddr_begin + x * 2 + y * vga.config.scan_len * 2 + 2;
						Bitu tileaddr_now = tileaddr_begin + (y % 8) * 2;
						if (x == 0) {
							PS55_CopyPlaneDataWithBitmask(tileaddr_now, destaddr_now, ps55.bitblt.reg[0x8]);
						}
						else if (x == ps55.bitblt.reg[0x33] - 1) {
							PS55_CopyPlaneDataWithBitmask(tileaddr_now, destaddr_now, ps55.bitblt.reg[0x9]);
						}
						else {
							PS55_CopyPlaneDataWithBitmask(tileaddr_now, destaddr_now, 0xffff);
						}
					}
				}
			}
			else if ((ps55.bitblt.reg[0x5] & 0xfff0) == 0x1040 && ps55.bitblt.reg[0x3D] == 0x00) {//Block transfer (range copy)
				Bitu destaddr_begin = ps55.bitblt.reg[0x29];
				Bitu srcaddr_begin = ps55.bitblt.reg[0x2A];
				//LOG_MSG("bitblt bitshift: %02x, dest: %08x src: %08x, mask: %04x", ps55.bitblt.bitshift_destr, destaddr_begin, srcaddr_begin, ps55.bitblt.reg[0x8]);
				for (Bitu y = 0; y < ps55.bitblt.reg[0x35]; y++)
				{
					for (Bitu x = 0; x < ps55.bitblt.reg[0x33]; x++)
					{
						Bitu destaddr_now = destaddr_begin + x * 2 + y * vga.config.scan_len * 2 + 2;
						Bitu srcaddr_now = srcaddr_begin + x * 2 + y * vga.config.scan_len * 2;
						if (x == 0) {
							PS55_CopyPlaneDataWithBitmask(srcaddr_now, destaddr_now, ps55.bitblt.reg[0x8]);
						}
						else if (x == ps55.bitblt.reg[0x33] - 1) {
							PS55_CopyPlaneDataWithBitmask(srcaddr_now, destaddr_now, ps55.bitblt.reg[0x9]);
						}
						else {
							PS55_CopyPlaneDataWithBitmask(srcaddr_now, destaddr_now, 0xffff);
						}
					}
				}
			}
			else if ((ps55.bitblt.reg[0x5] & 0xfff0) == 0x1140 && ps55.bitblt.reg[0x3D] == 0x00) {//Block copy but reverse direction
				Bitu destaddr_begin = ps55.bitblt.reg[0x29];
				Bitu srcaddr_begin = ps55.bitblt.reg[0x2A];
				for (Bitu y = 0; y < ps55.bitblt.reg[0x35]; y++)
				{
					for (Bitu x = 0; x < ps55.bitblt.reg[0x33]; x++)
					{
						Bitu destaddr_now = destaddr_begin - x * 2 - y * vga.config.scan_len * 2 - 2;
						Bitu srcaddr_now = srcaddr_begin - x * 2 - y * vga.config.scan_len * 2 - 2;
						if (x == 0) {
							PS55_CopyPlaneDataWithBitmask(srcaddr_now, destaddr_now, ps55.bitblt.reg[0x8]);
						}
						else if (x == ps55.bitblt.reg[0x33] - 1) {
							PS55_CopyPlaneDataWithBitmask(srcaddr_now, destaddr_now, ps55.bitblt.reg[0x9]);
						}
						else {
							PS55_CopyPlaneDataWithBitmask(srcaddr_now, destaddr_now, 0xffff);
						}
					}
				}
			}
			//clear payload
			memset(ps55.bitblt.payload, 0x00, PS55_BITBLT_MEMSIZE);
			ps55.bitblt.payload_addr = 0;
			//initialize regs with fefefefeh, clear regs with fefeh
			if (!(ps55.bitblt.reg[0x20] & 0x20)) {
				for (i = 0; i < PS55_BITBLT_REGSIZE; i++) {
					if (ps55.bitblt.reg[i] != 0xfefefefe) ps55.bitblt.reg[i] = 0xfefe;
				}
			}
			else//exec without init regs
			{
				ps55.bitblt.reg[0x20] = 0; /* need to stop execution */
			}
		}
	}
}
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
		//LOG_MSG("PS55_IOCTL: Write to port %x, idx %x, val %02xh (%d) -> %02xh (%d)", port, ps55.idx_3e1, ps55.p3e1_reg[ps55.idx_3e1], ps55.p3e1_reg[ps55.idx_3e1], val, val);
		if (ps55.idx_3e1 < 0x20) ps55.p3e1_reg[ps55.idx_3e1] = val;//save value for 0x1f function
		switch (ps55.idx_3e1) {
		case 0x00:
			//LOG_MSG("PS55_IOCTL: 3E1:%X Accessed !!!!!!!!!!!!! (val: %X, %dd)", ps55.idx_3e1, val, val);
			break;
		case 0x02:
			//Bit 8: ?(this bit is changed many times when filling a shape in BASIC)
			//Bit 4: Graphics / Text mode ???
			//Bit 2: Graphics Color / Mono ?
			//Bit 0: Text / Graphics (Text buffer / APA buffer)
			//LOG_MSG("PS55_IOCTL: 3E1:%X Accessed !!!!!!!!!!!!! (val: %X, %dd)", ps55.idx_3e1, val, val);
			//Filter 0000 0X0Xh (05h)
			if ((ps55.data3e1_02 & 0x05) ^ (val & 0x05)) {
			//if (ps55.data3e1_02 ^ val) {
				ps55.data3e1_02 = val;
				VGA_DetermineMode();
			}
			break;
		case 0x08://bit 4: Gaiji RAM Access Enabled
			if((ps55.gaijiram_access & 0x10) ^ (val & 0x10)){
				if (val & 0x10) {//remove these lines for DA1
					EnableGaijiRAMHandler();
				}
				else
				{
					DisableGaijiRAMHandler();
				}
			}
			ps55.gaijiram_access = val;
			break;
		default:
			//LOG_MSG("PS55_IOCTL: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e1, val, val);
			break;
		}
		//LOG_MSG("PS55_IOCTL: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e1, val, val);
		break;
	case 0x3e3://Font Buffer Registers (undoc) : TODO IBM 5550 Graphics Support Option uses many unknown regs (Index 20-2Fh)
		switch (ps55.idx_3e3) {
		case 0x00:
			//LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %02xh (%d) -> val %02xh (%d)", port, ps55.idx_3e3, ps55.mem_conf, ps55.mem_conf, val, val);
			if ((ps55.mem_conf & 0x40) ^ (val & 0x40)) {
				ps55.mem_conf = val;
				VGA_DetermineMode();//setup vga drawing if the value is changed
			}
			ps55.mem_conf = val;//for DEBUG
			break;
		case 0x08://??? length of font data? 80h latching? 10h
			ps55.data3e3_08 = val;
			//if(val != 0x80) LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			break;
		case 0x0a://Number of the memory page to access
			//if (val > 0x04) LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			ps55.mem_bank = val;
			break;
		case 0x0b://??? Select memory to be located on the system bus (b0, 10, 00)
			ps55.mem_select = val;
			//if (!(val == 0xb0 || val == 0x10)) LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			break;
		case 0x18://??? Memory refresh? 4->0
			break;
		default:
			//LOG_MSG("PS55_FONT: Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
			break;
		}
		//LOG_MSG("PS55_3E3(FONT): Write to port %x, idx %x, val %04xh (%d)", port, ps55.idx_3e3, val, val);
		break;
	case 0x3e5://CRT Controller Registers (undocumented)
#ifdef C_HEAVY_DEBUG
		if (!(ps55.idx_3e5 == 0xe || ps55.idx_3e5 == 0xf))
			LOG_MSG("PS55_CRTC: Write to port %x, idx %x, val %02xh (%d) -> %02xh (%d), len %d", port, ps55.idx_3e5, ps55.crtc_reg[ps55.idx_3e5], ps55.crtc_reg[ps55.idx_3e5], val, val, len);
#endif
		if (ps55.idx_3e5 < 0x20) ps55.crtc_reg[ps55.idx_3e5] = val;//save value for 0x1f function
		switch (ps55.idx_3e5) {
		//PS/55 actually has 16-bit registers, but in DOSBox, this uses the overflow register to overlap the VGA. Arkward:-<
		//6, 0c, 0e, 10, 12, 15, 16, 18
		case 0x00://for VGA: Horizontal Total Character Clocks-5
			//val -= 5;
			break;
		case 0x01://Horizontal Display End Register
			if (ps55.crtc_cmode) 
				val-=2;
			else val--;
			break;
		case 0x02://Start Horizontal Blanking
			val--;
			break;
		case 0x04://Start Horizontal Retrace Pulse
			val--;
			break;
		case 0x05://End Horizontal Retrace
			val--;
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
		case 0x0c://Start Address
			if (ps55.crtc_reg[0x1c] & 0x40)
				return;//ignore write (0xff) if reg 1C & 0x04
			//val &= 0xff;
			break;
		case 0x10://Vertical Retrace Start Register
			//val = 0x400; //for debugging bitblt
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
			//val = 0x400; //for debugging bitblt
			if (ps55.crtc_cmode) val -= 31;
			else val-=2;//adjust for VGA
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
		case 0x13://Offset Register (Horizontal line length in memory)
			//val=64;
			//through to VGA CRTC
			break;
		case 0x15://Start Vertical Blank Register
			//val = 0x400; //for debugging bitblt
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
			//new_crtcmode = true;
			if (ps55.crtc_cmode != new_crtcmode) {//if changed
				ps55.crtc_cmode = new_crtcmode;
				LOG_MSG("Reprogramming CRTC timing table.");
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
		PS55_GCR_Write(val);
		break;
	case 0x3ec://used by Windows 3.1 display driver
		//if (len == 2) LOG_MSG("PS55_??: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
		//else LOG_MSG("PS55_??: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
		val >>= 8;
		PS55_GCR_Write(val);
		/* reset masks for compatibility with Win 3.1 solitaire */
		if (ps55.idx_3eb == 05) {
			ps55.idx_3eb = 0x08;
			PS55_GCR_Write(0xff);
			ps55.idx_3eb = 0x09;
			PS55_GCR_Write(0xff);
			ps55.idx_3eb = 0x0a;
			PS55_GCR_Write(0xff);
		}
		break;
	case 0x3ed://used by Windows 3.1 display driver
		//if (len == 2) LOG_MSG("PS55_??: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
		//else LOG_MSG("PS55_??: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
		if (len == 1) return;
		ps55.idx_3eb = 05;
		PS55_GCR_Write(val);
		break;
	//case 0x3ee:
	//	//LOG_MSG("PS55_GC: Write to port %x, val %04xh (%d)", port, val, val);
	//	LOG_MSG("PS55_GC: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
	//	break;
	case 0x3e9:
		/* VZ Editor's CURSOR.COM writes data via this port */
		ps55.attr_reg[ps55.idx_3e8 & 0x1f] = val;
		break;
	default:
		if (len == 2) LOG_MSG("PS55_??: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
		else LOG_MSG("PS55_??: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
		break;
	}
}
void PS55_SetupSetResetRegisters()
{
	ps55.full_not_enable_set_reset_low = ~ps55.full_enable_set_reset_low;
	ps55.full_enable_and_set_reset_low = ps55.full_set_reset_low & ps55.full_enable_set_reset_low;
	ps55.full_not_enable_set_reset_high = ~ps55.full_enable_set_reset_high;
	ps55.full_enable_and_set_reset_high = ps55.full_set_reset_high & ps55.full_enable_set_reset_high;
}
void PS55_GCR_Write(Bitu val)
{
	ps55.gc_reg[ps55.idx_3eb & 0x1f] = (Bit8u)val;
	switch (ps55.idx_3eb) {
	case 0x00://Set/Reset
		//if(ps55.set_reset != val) LOG_MSG("PS55_GC: Set/Reset (00h) val %02xh (%d) -> %02xh (%d)", ps55.set_reset, ps55.set_reset, val, val);
		ps55.set_reset = val;
		ps55.full_set_reset_low = FillTable[val & 0x0f];
		ps55.full_set_reset_high = FillTable[(val >> 4) & 0x0f];
		PS55_SetupSetResetRegisters();
		break;
	case 0x01://Enable Set/Reset
		//if(ps55.enable_set_reset != val) LOG_MSG("PS55_GC: Enable Set/Reset (01h) val %02xh (%d) -> %02xh (%d)", ps55.enable_set_reset, ps55.enable_set_reset, val, val);
		ps55.enable_set_reset = val;
		ps55.full_enable_set_reset_low = FillTable[val & 0x0f];
		ps55.full_enable_set_reset_high = FillTable[(val >> 4) & 0x0f];
		PS55_SetupSetResetRegisters();
		break;
	//case 0x02://(VGA: Color Compare)
	case 0x03://Data Rotate
		//if(ps55.data_rotate != val) LOG_MSG("PS55_GC: Data Rotate (03h) val %02xh (%d) -> %02xh (%d)", ps55.data_rotate, ps55.data_rotate, val, val);
		ps55.data_rotate = val & 0x0f;
		break;
	case 0x04://Read Map Select
		//LOG_MSG("PS55_GC: Read Map Select (04h) val %02xh (%d) -> %02xh (%d)", vga.gfx.read_map_select, vga.gfx.read_map_select, val, val);
		vga.gfx.read_map_select = val;
		vga.config.read_map_select = val;
		break;
	case 0x06://Reserved (VGA: Miscellaneous)
			  //do nothing
		break;
	case 0x07://(VGA: Color Don't Care)
		val = ~val;//invert the input value for VGA register
		write_p3cf(0x3CF, val, 1);
		break;
	case 0x08://Bit Mask (Low)
		//if (ps55.bit_mask_low != val) LOG_MSG("PS55_GC: Bit Mask Low (08h) val %02xh (%d) -> %02xh (%d)", ps55.bit_mask_low, ps55.bit_mask_low, val, val);
		ps55.bit_mask_low = val & 0xff;
		ps55.full_bit_mask_low = ExpandTable[val & 0xff];
		break;
	case 0x09://Bit Mask (High)
		//if (ps55.bit_mask_high != val) LOG_MSG("PS55_GC: Bit Mask High (09h) val %02xh (%d) -> %02xh (%d)", ps55.bit_mask_high, ps55.bit_mask_high, val, val);
		ps55.bit_mask_high = val & 0xff;
		ps55.full_bit_mask_high = ExpandTable[val & 0xff];
		break;
	case 0x0a://Map Mask
		//if(ps55.map_mask != val) LOG_MSG("PS55_GC: Map Mask (0Ah) val %02xh (%d) -> %02xh (%d)", ps55.map_mask, ps55.map_mask, val, val);
		ps55.map_mask = val;
		ps55.full_map_mask_low = FillTable[val & 0x0f];
		ps55.full_not_map_mask_low = ~ps55.full_map_mask_low;
		ps55.full_map_mask_high = FillTable[(val >> 4) & 0x0f];//256 color does not support
		ps55.full_not_map_mask_high = ~ps55.full_map_mask_high;
		break;
	case 0x0b://Command (must be 08h according to the IBM reference) but J-DOS uses 0Bh, Windows 3.1 uses 00h
			  // 08 0000 1000
			  // 0B 0000 1011
		//if (ps55.data3ea_0b != val) LOG_MSG("PS55_GC: Command (0Bh) val %02xh (%d) -> %02xh (%d)", ps55.data3ea_0b, ps55.data3ea_0b, val, val);
		vga.config.raster_op = val;
		ps55.data3ea_0b = val;//for debug
		break;
	case 0x05://Mode (readmode and writemode)
		// 00 08 40 41 42 43
		//LOG_MSG("PS55_GC: Graphics Mode (05h) val %02xh (%d) CS %x IP %x", val, val, SegValue(cs), reg_ip);
		write_p3cf(0x3CF, val, 1);
		break;
	default:
		//LOG_MSG("PS55_3CF(VGA_GC): Write to idx %x, val %04xh (%d)", ps55.idx_3eb, val, val);
		write_p3cf(0x3CF, val, 1);
		break;
	}
}

void PS55_ATTR_Write(Bitu port, Bitu val, Bitu len) {
	Bitu data = 0;
	Bit8u chgbits;
	if (len == 1 && ps55.latch_3e8 == false && port == 0x3e8)
	{
#ifdef C_HEAVY_DEBUG
		//LOG_MSG("PS55_ATTR: Set index %02xh (%d), not latched", val, val);
#endif
		ps55.idx_3e8 = val;
		ps55.latch_3e8 = true;
		if (val & 0x20) vga.attr.disabled &= ~1;
		else vga.attr.disabled |= 1;
	}
	else {
		if (len == 2) {
#ifdef C_HEAVY_DEBUG
			//LOG_MSG("PS55_ATTR: Write val %04xh (%d), previdx %x", val, val, ps55.idx_3e8);
#endif
			ps55.idx_3e8 = val & 0xff;
			data = val >> 8;
		}
		else
		{
			data = val;
#ifdef C_HEAVY_DEBUG
			//LOG_MSG("PS55_ATTR: Write val %02xh (%d), idx %x", val, val, ps55.idx_3e8);
#endif
		}
		ps55.latch_3e8 = false;
#ifdef C_HEAVY_DEBUG
		//LOG_MSG("PS55_ATTR: Write to port %x, idx %x, val %02xh (%d) -> %02xh (%d), len %d", port, ps55.idx_3e8, ps55.attr_reg[ps55.idx_3e8 & 0x1f], ps55.attr_reg[ps55.idx_3e8 & 0x1f], data, data, len);
#endif
		ps55.attr_reg[ps55.idx_3e8 & 0x1f] = data;
		//!!!!!  ps55.idx_3e8=Index, data=Data   !!!!!
		switch (ps55.idx_3e8 & 0x1f) { 
		case 0x14://Set line color for character mode
			ps55.palette_line = data & 0x0f;
			//ps55.palette_line = data;
			return;
		case 0x18://attribute mode 03 compatible?
			if ((ps55.text_mode03 & 0x01) ^ (data & 0x01)) {
				ps55.text_mode03 = data;
				VGA_DetermineMode();//setup vga drawing if the value is changed
			}
			return;
		case 0x1a://Cursor color
#ifdef C_HEAVY_DEBUG
			if ((ps55.cursor_color & 0x0f) ^ (data & 0x0f)) {
				//LOG_MSG("PS55_ATTR: The cursor color has been changed to %02xh (%d)", data & 0x0f, data & 0x0f);
			}
#endif
			ps55.cursor_color = data & 0x0f;
			return;
		case 0x1b://Cursor blinking speed
#ifdef C_HEAVY_DEBUG
			if (ps55.cursor_options ^ data) {
				//LOG_MSG("PS55_ATTR: The cursor option has been changed to %02xh (%d)", data, data);
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
	//if (len == 2) LOG_MSG("PS55_IDX: Write to port %x, val %04xh (%d), len %x %x:%x", port, val, val, len, SegValue(cs), reg_ip);
	//else LOG_MSG("PS55_IDX: Write to port %x, val %02xh (%d), len %x %x:%x", port, val, val, len, SegValue(cs), reg_ip);
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
		ps55.idx_3eb = val;
		//LOG_MSG("PS55_GC: Write to port %x, idx %x, val %02xh (%d)", port, ps55.idx_3eb, data, data);
		write_p3ce(port, val, 1);
		break;
	}
	ps55.prevdata = NOAHDATA;
	ps55.nextret = NOAHDATA;
	if (len == 2) PS55_GC_Data_Write(port + 1, data, 1);
}

Bitu PS55_GC_Read(Bitu port, Bitu len) {
	Bitu ret = 0xFF;//J-DOS BASIC interpreter tests 3E3/Index 36h is writable
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
	case 0x3e1://??? (undocumented)
		switch (ps55.idx_3e1) {
		case 0x02://?
			ret = ps55.data3e1_02;
			break;
		case 0x03://bit 7: Display supports color/mono, bit 0: In Operation (wait) / -Access
			ret = ps55.data3e1_03;//default 0x80
			//ret = 0x80;//1000 0000
			//ret = 0x00;//0000 0000 for TEST
			break;
		case 0x08://bit 4: Gaiji RAM Access Enabled
			ret = ps55.gaijiram_access;
			break;
		case 0x0a:  //Config1 Bit 5 = 0: DA-2, 1: DA-3 or DA-5 
					//Bit2-0 = (110 or 111): Display Adapter Memory Expansion Kit is not installed to disable 256 color mode that is still not supported.
			ret = 0x16;//MonID3 + 512KB
			break;
		case 0x0b://Config2
			ret = 0x02;//MonID1
			break;
		default:
			//LOG_MSG("PS55_IOCTL: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3e1, len, ret);
			break;
		}
		//LOG_MSG("PS55_IOCTL: Read from port %x, idx %x, len %x, ret %x", port, ps55.idx_3e1, len, ret);
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
		//LOG_MSG("PS55_ATTR: Read from port %x, idx %2x, len %x, ret %2x", port, ps55.idx_3e8, len, ret);
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

void PS55_DA1_Write(Bitu port, Bitu val, Bitu len) {
	//EnableGaijiRAMHandler();//for DA1
	//if (len == 2) LOG_MSG("PS55_DA1: Write to port %x, val %04xh (%d), len %x", port, val, val, len);
	//else LOG_MSG("!!PS55_DA1: Write to port %x, val %02xh (%d), len %x", port, val, val, len);
	//switch (port) {
	//case 0x3ee://?
	//	//Switch Font ROM/RAM window addressed at A0000-BFFFFh
	//	if ((ps55.data3ee & 0x40) ^ (val & 0x40)) {
	//		//ps55.gaijiram_access = true;
	//		if (val & 0x40) {//Font ROM
	//			ps55.mem_select = 0x10;
	//			LOG_MSG("Font ROM selected (DA1)");
	//		}
	//		else//Font display buffer
	//		{
	//			ps55.mem_select = 0xb0;
	//			DisableGaijiRAMHandler();
	//			LOG_MSG("Font buffer selected (DA1)");
	//		}
	//	}
	//	if (ps55.mem_select == 0xb0) {
	//		if (val & 0x80) {
	//			ps55.data3e3_08 = 0x80;
	//			ps55.mem_bank = 1;
	//		}
	//		else {
	//			ps55.mem_bank = 0;
	//		}
	//	}
	//	ps55.data3ee = val;
	//	break;
	//case 0x3ef:
	//	ps55.mem_bank = val & 0x0f;
	//	break;
	//}
}
Bitu PS55_DA1_Read(Bitu port, Bitu len) {
	LOG_MSG("!!PS55_DA1: Read from port %x, len %x", port, len);
	return ps55.data3ee;
}
void FinishSetMode_PS55(Bitu /*crtc_base*/, VGA_ModeExtraData* modeData) {
	VGA_SetupHandlers();
}

void DetermineMode_PS55() {
	if (ps55.carden)
	{
		vga.vmemwrap = 512 * 1024;
		if (!(ps55.data3e1_02 & 0x01)) {
#ifdef C_HEAVY_DEBUG
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 8-color graphics.");
#endif
					VGA_SetMode(M_PS55_GFX);
			//}
		}
		else {
			if (ps55.text_mode03) {
#ifdef C_HEAVY_DEBUG
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 Mode 03 text.");
#endif
				VGA_SetMode(M_PS55_M3TEXT);
			}
			else {
#ifdef C_HEAVY_DEBUG
				LOG_MSG("PS55_DetermineMode: Set videomode to PS/55 text.");
#endif
				VGA_SetMode(M_PS55_TEXT);
			}
		}
	}
	else
	{//replica of VGA_DetermineMode() without S3 dependencies
#ifdef C_HEAVY_DEBUG
		LOG_MSG("PS55_DetermineMode: Set videomode to VGA.");
#endif
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
/*
DOS K3.3
Mode 0 C8 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C CRTC 0x0c-1f: FF 00 01 00 80 00 04 02
Mode 1 GA 3e1_2 10, 3e3_0 00, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E CRTC 0x0c-1f: 00 00 01 00 80 00 04 02
Mode 3 CE 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C CRTC 0x0c-1f: FF 00 01 00 80 00 04 02
Mode 4 GD 3e1_2 10, 3e3_0 2B, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E CRTC 0x0c-1f: FF 00 01 00 80 00 04 02

DOS J4.0
Mode 0 C8 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C
Mode 1 GA 3e1_2 10, 3e3_0 2B, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E CRTC 0x0c-1f: FF 00 01 00 80 00 04 02
Mode 3 CE 3e1_2 11, 3e3_0 2B, 3e5_1c 00, 3e5_1f 00, 3e8_38 00, 3e8_3f 0C
Mode 4 GD 3e1_2 14, 3e3_0 2B, 3e5_1c 80, 3e5_1f 02, 3e8_38 00, 3e8_3f 0E CRTC 0x0c-1f: FF 00 01 00 80 00 04 02
VMX  3 C3 3e1_2 15, 3e3_0 6B, 3e5_1c 00, 3e5_1f 02, 3e8_38 01, 3e8_3f 0C
*/

//[Font ROM Map (DA1)]
//Bank 0
// 0000-581Fh Pointers (Low) for each character font?
// 5820-7FFFh Pointers (High) for each character font?
// 8000- *  h Font
//
//void generate_DA1Font()
//{
//	//int membank = 1;
//	int baseaddr = 0x8000;
//	for (int chr = 0x100; chr < DBCS24_CHARS; chr++) {
//		for (int line = 0; line < 72; line++)
//		{
//			ps55.font_da1[baseaddr + line] = ps55font_24[chr * 72 + line];
//		}
//		//Store memory pointers for each characters
//		ps55.font_da1[(chr - 0x100) * 2 + 1] = baseaddr & 0xff;
//		ps55.font_da1[(chr - 0x100) * 2] = (baseaddr >> 8) & 0xff;
//		ps55.font_da1[0x5820 + (chr - 0x100) / 2] = (baseaddr >> 16) & 0xff;
//		//if above 0xffff, switch the bank (910 characters per one bank)
//		baseaddr += 72;
//		//if (baseaddr > 0xffff) {
//		//	baseaddr &= 0xf0000;
//		//	baseaddr += 0x10000;
//		//	//membank++;
//		//}
//	}
//}
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
	if (mode == 0x08 || mode == 0x0e || mode == 0x0d)
		return VideoModeMemSize(mode) < vga.vmemsize;
	else
		return false;
}
void PS55_Suspend(void) {
	ps55.carden = false;
	for (int port = 0x3e0; port < 0x3ef; port++)
	{
		IO_FreeWriteHandler(port, IO_MB | IO_MW);
		IO_FreeReadHandler(port, IO_MB | IO_MW);
	}
	vga.crtc.overflow = ps55.vga_crtc_overflow;
	vga.config.line_compare = ps55.vga_config_line_compare;
	LOG_MSG("PS/55 suspended");
}
void PS55_WakeUp(void) {
	ps55.carden = true;
	//Clear CRTC registers that are ignored(CRTC index 7, 8?, 18)
	ps55.vga_crtc_overflow = vga.crtc.overflow;
	ps55.vga_config_line_compare = vga.config.line_compare;
	vga.crtc.overflow = 0;
	vga.config.line_compare = 0xFFF;
	vga.crtc.read_only = false;
	MEM_ResetPageHandler(VGA_PAGE_A0, 32);//release VGA handlers

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

	IO_RegisterWriteHandler(0x3ec, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ec, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3ed, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ed, &PS55_GC_Read, IO_MB | IO_MW);
#ifdef C_HEAVY_DEBUG
	//IO_RegisterWriteHandler(0x3ee, &PS55_GC_Data_Write, IO_MB | IO_MW);//Attribute?
	IO_RegisterWriteHandler(0x3ee, &PS55_DA1_Write, IO_MB | IO_MW);//Attribute?
	//IO_RegisterReadHandler(0x3ee, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ee, &PS55_DA1_Read, IO_MB | IO_MW);
	//IO_RegisterWriteHandler(0x3ef, &PS55_GC_Data_Write, IO_MB | IO_MW);
	IO_RegisterWriteHandler(0x3ef, &PS55_DA1_Write, IO_MB | IO_MW);
	//IO_RegisterReadHandler(0x3ef, &PS55_GC_Read, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x3ef, &PS55_CRTC_Read, IO_MB | IO_MW);//FOR DEBUG
#endif
	//IO_RegisterWriteHandler(0x3ef, write_p3c0, IO_MB); // for PS/55
	//IO_RegisterReadHandler(0x3ef, read_p3c0, IO_MB); // for PS/55
	//IO_RegisterReadHandler(0x3ee, read_p3c1, IO_MB); // for PS/55

	vga.vmemsize = 512 * 1024;
	DetermineMode_PS55();
	LOG_MSG("PS/55 wakes up!!");
}

//Setup MicroChannel(tm) bus and adapters
void SVGA_Setup_PS55(void) {
	ps55.prevdata = NOAHDATA;
	ps55.nextret = NOAHDATA;
	ps55.gaiji_ram = new Bit8u[256 * 1024];//256 KB for Gaiji RAM
#if C_DEBUG
	ps55.bitblt.debug_reg = new Bitu[65536 * PS55_DEBUG_BITBLT_SIZE];
#endif
#ifdef C_HEAVY_DEBUG
	ps55.mmwdbg_fp = fopen("da2_mmiowdat.txt", "w");
	ps55.mmrdbg_fp = fopen("da2_mmiordat.txt", "w");
#endif
	//ps55.font_da1 = new Bit8u[1536 * 1024];//1024 KB for Display Adapter (I) Font ROM
	//memset(ps55.font_da1, 0x00, 1536 * 1024);
	//generate_DA1Font();
	ps55.bitblt.payload_addr = 0;
	memset(ps55.bitblt.payload, 0x00, PS55_BITBLT_MEMSIZE);
	memset(ps55.bitblt.reg, 0xfe, PS55_BITBLT_REGSIZE * sizeof(Bitu));//clear memory
	ps55.mem_select = 0xb0;
	if (ps55.palette_mono) ps55.data3e1_03 &= 0x7f;

	IO_RegisterWriteHandler(0x74, &write_p74, IO_MB | IO_MW);
	IO_RegisterReadHandler(0x76, &read_p76, IO_MB);
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
	svga.set_clock = &SetClock_PS55;
	svga.get_clock = &GetClock_PS55;
	svga.accepts_mode = &AcceptsMode_PS55;

	vga.vmemsize = 512 * 1024; // equivalent to DAIV without 256 color support (Memory Expansion Kit)
}
