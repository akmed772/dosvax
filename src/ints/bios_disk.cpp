/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *  Copyright (C) 2016 akm
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


#include "dosbox.h"
#include "callback.h"
#include "bios.h"
#include "bios_disk.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h" /* for Drives[] */
#include "../dos/drives.h"
#include "mapper.h"



diskGeo DiskGeometryList[] = {
	{ 160,  8, 1, 40, 0},	// SS/DD 5.25"
	{ 180,  9, 1, 40, 0},	// SS/DD 5.25"
	{ 200, 10, 1, 40, 0},	// SS/DD 5.25" (booters)
	{ 320,  8, 2, 40, 1},	// DS/DD 5.25"
	{ 360,  9, 2, 40, 1},	// DS/DD 5.25"
	{ 400, 10, 2, 40, 1},	// DS/DD 5.25" (booters)
	{ 640,  8, 2, 80, 2},	// DS/DD 5.25" (Only DOS K3.x can read) for DOSVAX
	{ 720,  9, 2, 80, 3},	// DS/DD 3.5"
	{1200, 15, 2, 80, 2},	// DS/HD 5.25"
	{1440, 18, 2, 80, 4},	// DS/HD 3.5"
	{1680, 21, 2, 80, 4},	// DS/HD 3.5"  (DMF)
	{2880, 36, 2, 80, 6},	// DS/ED 3.5"
	{0, 0, 0, 0, 0}
};

Bitu call_int13;
Bitu diskparm0, diskparm1;
static Bit8u last_status;
static Bit8u last_drive;
Bit16u imgDTASeg;
RealPt imgDTAPtr;
DOS_DTA *imgDTA;
bool killRead;
static bool swapping_requested;

void BIOS_SetEquipment(Bit16u equipment);

/* 2 floppys and 2 harddrives, max */
imageDisk *imageDiskList[MAX_DISK_IMAGES];
imageDisk *diskSwap[MAX_SWAPPABLE_DISKS];
Bit32s swapPosition;

void updateDPT(void) {
	Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
	if(imageDiskList[2] != NULL) {
		PhysPt dp0physaddr=CALLBACK_PhysPointer(diskparm0);
		imageDiskList[2]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		phys_writew(dp0physaddr,(Bit16u)tmpcyl);
		phys_writeb(dp0physaddr+0x2,(Bit8u)tmpheads);
		phys_writew(dp0physaddr+0x3,0);
		phys_writew(dp0physaddr+0x5,(Bit16u)-1);
		phys_writeb(dp0physaddr+0x7,0);
		phys_writeb(dp0physaddr+0x8,(0xc0 | (((imageDiskList[2]->heads) > 8) << 3)));
		phys_writeb(dp0physaddr+0x9,0);
		phys_writeb(dp0physaddr+0xa,0);
		phys_writeb(dp0physaddr+0xb,0);
		phys_writew(dp0physaddr+0xc,(Bit16u)tmpcyl);
		phys_writeb(dp0physaddr+0xe,(Bit8u)tmpsect);
	}
	if(imageDiskList[3] != NULL) {
		PhysPt dp1physaddr=CALLBACK_PhysPointer(diskparm1);
		imageDiskList[3]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		phys_writew(dp1physaddr,(Bit16u)tmpcyl);
		phys_writeb(dp1physaddr+0x2,(Bit8u)tmpheads);
		phys_writeb(dp1physaddr+0xe,(Bit8u)tmpsect);
	}
}

void incrementFDD(void) {
	Bit16u equipment=mem_readw(BIOS_CONFIGURATION);
	if(equipment&1) {
		Bitu numofdisks = (equipment>>6)&3;
		numofdisks++;
		if(numofdisks > 1) numofdisks=1;//max 2 floppies at the moment
		equipment&=~0x00C0;
		equipment|=(numofdisks<<6);
	} else equipment|=1;
	BIOS_SetEquipment(equipment);
}

void swapInDisks(void) {
	bool allNull = true;
	Bit32s diskcount = 0;
	Bit32s swapPos = swapPosition;
	Bit32s i;

	/* Check to make sure that  there is at least one setup image */
	for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
		if(diskSwap[i]!=NULL) {
			allNull = false;
			break;
		}
	}

	/* No disks setup... fail */
	if (allNull) return;

	/* If only one disk is loaded, this loop will load the same disk in dive A and drive B */
	while(diskcount<2) {
		if(diskSwap[swapPos] != NULL) {
			LOG_MSG("Loaded disk %d from swaplist position %d - \"%s\"", diskcount, swapPos, diskSwap[swapPos]->diskname);
			imageDiskList[diskcount] = diskSwap[swapPos];
			diskcount++;
		}
		swapPos++;
		if(swapPos>=MAX_SWAPPABLE_DISKS) swapPos=0;
	}
}

bool getSwapRequest(void) {
	bool sreq=swapping_requested;
	swapping_requested = false;
	return sreq;
}

void swapInNextDisk(bool pressed) {
	if (!pressed)
		return;
	DriveManager::CycleAllDisks();
	/* Hack/feature: rescan all disks as well */
	LOG_MSG("Diskcaching reset for normal mounted drives.");
	for(Bitu i=0;i<DOS_DRIVES;i++) {
		if (Drives[i]) Drives[i]->EmptyCache();
	}
	swapPosition++;
	if(diskSwap[swapPosition] == NULL) swapPosition = 0;
	swapInDisks();
	swapping_requested = true;
}

extern bool OpenFileDialog(char* szFile, int sizeSzFile);

void mountFloppyDiskDialog(bool pressed) {//for DOSVAX
	TCHAR filename[MAX_PATH] = { 0 };
	if (!pressed)
		return;

	if (imageDiskList[0] != NULL)
	{
		delete imageDiskList[0];
		imageDiskList[0] = NULL;
	}

	if (!OpenFileDialog(filename, sizeof(filename)))
	{
		LOG_MSG("MountFloppy: Dialog has been cancelled.");
		return;
	}

	FILE* newDisk = fopen_wrap(filename, "rb+");
	if (!newDisk) {
		LOG_MSG("MountFloppy: Cannot open the file.");
		return;
	}
	fseek(newDisk, 0L, SEEK_END);
	Bit32u imagesize = (ftell(newDisk) / 1024);
	if (imagesize > 2880) {
		fclose(newDisk);
		LOG_MSG("MountFloppy: The file may be not a floppy disk image.");
		return;
	}
	imageDisk* newImage = new imageDisk(newDisk, filename, imagesize, false);
	if (!newImage->active)
	{
		delete newImage;//destructor will call fclose
		LOG_MSG("MountFloppy: The disk geometry doesn't match for floppy drives.");
		return;
	}
	//set the new disk image to Drive A
	imageDiskList[0] = newImage;
	LOG_MSG("Drive number 0 mounted as %s", filename);
}

Bit8u imageDisk::Read_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Read_AbsoluteSector(sectnum, data);
}

Bit8u imageDisk::Read_AbsoluteSector(Bit32u sectnum, void * data) {
	Bit32u bytenum;

	bytenum = sectnum * sector_size;

	if (last_action==WRITE || bytenum!=current_fpos) fseek(diskimg,bytenum,SEEK_SET);
	size_t ret=fread(data, 1, sector_size, diskimg);
	current_fpos=bytenum+ret;
	last_action=READ;

	return 0x00;
}

Bit8u imageDisk::Write_Sector(Bit32u head,Bit32u cylinder,Bit32u sector,void * data) {
	Bit32u sectnum;

	sectnum = ( (cylinder * heads + head) * sectors ) + sector - 1L;

	return Write_AbsoluteSector(sectnum, data);
}


Bit8u imageDisk::Write_AbsoluteSector(Bit32u sectnum, void *data) {
	Bit32u bytenum;

	bytenum = sectnum * sector_size;

	//LOG_MSG("Writing sectors to %ld at bytenum %d", sectnum, bytenum);

	if (last_action==READ || bytenum!=current_fpos) fseek(diskimg,bytenum,SEEK_SET);
	size_t ret=fwrite(data, 1, sector_size, diskimg);
	current_fpos=bytenum+ret;
	last_action=WRITE;

	return ((ret>0)?0x00:0x05);

}

imageDisk::imageDisk(FILE *imgFile, const char *imgName, Bit32u imgSizeK, bool isHardDisk) {
	heads = 0;
	cylinders = 0;
	sectors = 0;
	sector_size = 512;
	current_fpos = 0;
	last_action = NONE;
	diskimg = imgFile;
	fseek(diskimg,0,SEEK_SET);
	memset(diskname,0,512);
	safe_strncpy(diskname, imgName, sizeof(diskname));
	active = false;
	hardDrive = isHardDisk;
	if(!isHardDisk) {
		Bit8u i=0;
		bool founddisk = false;
		while (DiskGeometryList[i].ksize!=0x0) {
			if ((DiskGeometryList[i].ksize==imgSizeK) ||
				(DiskGeometryList[i].ksize+1==imgSizeK)) {
				if (DiskGeometryList[i].ksize!=imgSizeK)
					LOG_MSG("ImageLoader: image file with additional data, might not load!");
				founddisk = true;
				active = true;
				floppytype = i;
				heads = DiskGeometryList[i].headscyl;
				cylinders = DiskGeometryList[i].cylcount;
				sectors = DiskGeometryList[i].secttrack;
				break;
			}
			i++;
		}
		if(!founddisk) {
			active = false;
		} else {
			incrementFDD();
		}
	}
}

void imageDisk::Set_Geometry(Bit32u setHeads, Bit32u setCyl, Bit32u setSect, Bit32u setSectSize) {
	heads = setHeads;
	cylinders = setCyl;
	sectors = setSect;
	sector_size = setSectSize;
	active = true;
}

void imageDisk::Get_Geometry(Bit32u * getHeads, Bit32u *getCyl, Bit32u *getSect, Bit32u *getSectSize) {
	*getHeads = heads;
	*getCyl = cylinders;
	*getSect = sectors;
	*getSectSize = sector_size;
}

Bit8u imageDisk::GetBiosType(void) {
	if(!hardDrive) {
		return (Bit8u)DiskGeometryList[floppytype].biosval;
	} else return 0;
}

Bit32u imageDisk::getSectSize(void) {
	return sector_size;
}

static Bit8u GetDosDriveNumber(Bit8u biosNum) {
	switch(biosNum) {
		case 0x0:
			return 0x0;
		case 0x1:
			return 0x1;
		case 0x80:
			return 0x2;
		case 0x81:
			return 0x3;
		case 0x82:
			return 0x4;
		case 0x83:
			return 0x5;
		default:
			return 0x7f;
	}
}

static bool driveInactive(Bit8u driveNum) {
	if(driveNum>=MAX_DISK_IMAGES) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d non-existant", driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	if(imageDiskList[driveNum] == NULL) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	if(!imageDiskList[driveNum]->active) {
		LOG(LOG_BIOS,LOG_ERROR)("Disk %d not active", driveNum);
		last_status = 0x01;
		CALLBACK_SCF(true);
		return true;
	}
	return false;
}


static Bitu INT13_DiskHandler(void) {
	Bit16u segat, bufptr;
	Bit8u sectbuf[512];
	Bit8u  drivenum;
	Bitu  i,t;
	last_drive = reg_dl;
	drivenum = GetDosDriveNumber(reg_dl);
	bool any_images = false;
	for(i = 0;i < MAX_DISK_IMAGES;i++) {
		if(imageDiskList[i]) any_images=true;
	}
	//LOG(LOG_BIOS, LOG_ERROR)("INT13: Function %x called on drive %x (dos drive %d)", reg_ah, reg_dl, drivenum);

	// unconditionally enable the interrupt flag
	CALLBACK_SIF(true);

	//drivenum = 0;
	//LOG_MSG("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);

	// NOTE: the 0xff error code returned in some cases is questionable; 0x01 seems more correct
	switch(reg_ah) {
	case 0x0: /* Reset disk */
		{
			/* if there aren't any diskimages (so only localdrives and virtual drives)
			 * always succeed on reset disk. If there are diskimages then and only then
			 * do real checks
			 */
			if (any_images && driveInactive(drivenum)) {
				/* driveInactive sets carry flag if the specified drive is not available */
				if ((machine==MCH_CGA) || (machine==MCH_PCJR)) {
					/* those bioses call floppy drive reset for invalid drive values */
					if (((imageDiskList[0]) && (imageDiskList[0]->active)) || ((imageDiskList[1]) && (imageDiskList[1]->active))) {
						if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
						last_status = 0x00;
						CALLBACK_SCF(false);
					}
				}
				return CBRET_NONE;
			}
			if (machine!=MCH_PCJR && reg_dl<0x80) reg_ip++;
			last_status = 0x00;
			CALLBACK_SCF(false);
		}
        break;
	case 0x1: /* Get status of last operation */

		if(last_status != 0x00) {
			reg_ah = last_status;
			CALLBACK_SCF(true);
		} else {
			reg_ah = 0x00;
			CALLBACK_SCF(false);
		}
		break;
	case 0x2: /* Read sectors */
		if (reg_al==0) {
			reg_ah = 0x01;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		if (drivenum >= MAX_DISK_IMAGES || imageDiskList[drivenum] == NULL) {
			if (drivenum >= DOS_DRIVES || !Drives[drivenum] || Drives[drivenum]->isRemovable()) {
				//for DOSVAX : No disk
				if (drivenum < 2)//drive not ready
					reg_ah = 0x80;
				else
					reg_ah = 0x01;//invalid param
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			// Inherit the Earth cdrom and Amberstar use it as a disk test
			if (((reg_dl&0x80)==0x80) && (reg_dh==0) && ((reg_cl&0x3f)==1)) {
				if (reg_ch==0) {
					// write some MBR data into buffer for Amberstar installer
					real_writeb(SegValue(es),reg_bx+0x1be,0x80); // first partition is active
					real_writeb(SegValue(es),reg_bx+0x1c2,0x06); // first partition is FAT16B
				}
				reg_ah = 0;
				CALLBACK_SCF(false);
				return CBRET_NONE;
			}
		}
		if (driveInactive(drivenum)) {
			reg_ah = 0x80;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}

		segat = SegValue(es);
		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			last_status = imageDiskList[drivenum]->Read_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)+i), sectbuf);
			if((last_status != 0x00) || (killRead)) {
				LOG_MSG("Error in disk read");
				killRead = false;
				reg_ah = 0x04;
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			for(t=0;t<512;t++) {
				real_writeb(segat,bufptr,sectbuf[t]);
				bufptr++;
			}
		}
		//LOG(LOG_BIOS, LOG_ERROR)("INT13: Read data, Drive:%d, DH:%x, CX:%x", drivenum, reg_dh, reg_cx);
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x3: /* Write sectors */
		
		if(driveInactive(drivenum)) {
			reg_ah = 0x80;
			CALLBACK_SCF(true);
			return CBRET_NONE;
        }                     

		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			for(t=0;t<imageDiskList[drivenum]->getSectSize();t++) {
				sectbuf[t] = real_readb(SegValue(es),bufptr);
				bufptr++;
			}

			last_status = imageDiskList[drivenum]->Write_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0) << 2)), (Bit32u)((reg_cl & 63) + i), &sectbuf[0]);
			if(last_status != 0x00) {
            CALLBACK_SCF(true);
				return CBRET_NONE;
			}
        }
//		LOG(LOG_BIOS, LOG_ERROR)("INT13: Write data, Drive:%d, DH:%x, CX:%x", drivenum, reg_dh, reg_cx);
		reg_ah = 0x00;
		CALLBACK_SCF(false);
        break;
	case 0x04: /* Verify sectors */
		if (reg_al==0) {
			reg_ah = 0x01;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		if(driveInactive(drivenum)) {
			reg_ah = last_status;
			return CBRET_NONE;
		}

		/* TODO: Finish coding this section */
		/*
		segat = SegValue(es);
		bufptr = reg_bx;
		for(i=0;i<reg_al;i++) {
			last_status = imageDiskList[drivenum]->Read_Sector((Bit32u)reg_dh, (Bit32u)(reg_ch | ((reg_cl & 0xc0)<< 2)), (Bit32u)((reg_cl & 63)+i), sectbuf);
			if(last_status != 0x00) {
				LOG_MSG("Error in disk read");
				CALLBACK_SCF(true);
				return CBRET_NONE;
			}
			for(t=0;t<512;t++) {
				real_writeb(segat,bufptr,sectbuf[t]);
				bufptr++;
			}
		}*/
		reg_ah = 0x00;
		//Qbix: The following codes don't match my specs. al should be number of sector verified
		//reg_al = 0x10; /* CRC verify failed */
		//reg_al = 0x00; /* CRC verify succeeded */
		CALLBACK_SCF(false);
          
		break;
	case 0x05: /* Format track */
		if (driveInactive(drivenum)) {
			//for DOSVAX
			reg_ah = 0x80;//Time out
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x08: /* Get drive parameters */
		if(driveInactive(drivenum)) {
			//for DOSVAX
			//The default disk geometry for the FORMAT command is determined by the drive type,
			// and the drive type is determined by a mounted disk when the BOOT command is executed.
			//If there is no disk mounted for floppy drives when BOOT, DOSBox returns below parameters.
			if (IS_PS55_ARCH && drivenum < 2)
			{
				//1 x DSHD 3.5" drive attached
				reg_ax = 0;
				reg_bx = 0x0004;//DSHD 3.5"
				reg_dl = 0x02;//attached drives
				reg_dh = 0x01;//max head
				reg_cl = 18;//bit 7-6: cyl (Bit 9,8), bit 5-0: max sector
				reg_ch = 79;//cyl (Bit 7-0)
				////1 x DSHD 5.25" drive attached
				//reg_ax = 0;
				//reg_bx = 0x0002;//DSHD 5.25"
				//reg_dl = 0x02;//attached drives
				//reg_dh = 0x01;//max head
				//reg_cl = 15;//bit 7-6: cyl (Bit 9,8), bit 5-0: max sector
				//reg_ch = 79;//cyl (Bit 7-0)
				SegSet16(es, RealSeg(BIOS_INT1E_DISKETTE_PARAMETERS));
				reg_di = RealOff(BIOS_INT1E_DISKETTE_PARAMETERS);
				CALLBACK_SCF(false);
				return CBRET_NONE;
			}
			last_status = 0x07;
			reg_ah = last_status;
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		reg_ax = 0x00;
		reg_bh = 0x00;//for DOSVAX 2023/12/20; for DOS K3.3 to set BDS correctly
		reg_bl = imageDiskList[drivenum]->GetBiosType();
		Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
		imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		if (tmpcyl==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: cylinder count zero!");
		else tmpcyl--;		// cylinder count -> max cylinder
		if (tmpheads==0) LOG(LOG_BIOS,LOG_ERROR)("INT13 DrivParm: head count zero!");
		else tmpheads--;	// head count -> max head
		reg_ch = (Bit8u)(tmpcyl & 0xff);
		reg_cl = (Bit8u)(((tmpcyl >> 2) & 0xc0) | (tmpsect & 0x3f)); 
		reg_dh = (Bit8u)tmpheads;
		last_status = 0x00;
		if (reg_dl&0x80) {	// harddisks
			reg_dl = 0;
			if(imageDiskList[2] != NULL) reg_dl++;
			if(imageDiskList[3] != NULL) reg_dl++;
		} else {		// floppy disks
			reg_dl = 0;
			if(imageDiskList[0] != NULL) reg_dl++;
			if(imageDiskList[1] != NULL) reg_dl++;
		}
		CALLBACK_SCF(false);
		break;
	case 0x11: /* Recalibrate drive */
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x15: /* Read Drive Type : DOSVAX */
		if (driveInactive(drivenum)) {
			reg_ah = 0x00;//drive not present
			CALLBACK_SCF(true);//CF=1
			return CBRET_NONE;
		}
		if (imageDiskList[drivenum]->hardDrive) {
			//hard drive
			Bit32u sectnum = imageDiskList[drivenum]->sectors;
			reg_ah = 0x03;
			reg_cx = (Bit16u)(sectnum >> 16);
			reg_dx = (Bit16u)sectnum;
		}
		else {
			//floppy drive
			if (imageDiskList[drivenum]->floppytype < 6)
				reg_ah = 0x01;//2D (no change line available)
			else
				reg_ah = 0x02;//2DD/2HD (change line available)
		}
		CALLBACK_SCF(false);//CF=0
		break;
	case 0x16: /* Detect media changed : DOSVAX */
		if (driveInactive(drivenum)) {
			reg_ah = 0x80;//drive not ready
			CALLBACK_SCF(true);
		}
		else if (getSwapRequest()) {
			reg_ah = 0x06;//change detection
			CALLBACK_SCF(true);
		}
		else {
			reg_ah = 0x00;
			CALLBACK_SCF(false);
		}
		break;
	case 0x17: /* Set disk type for format */
		/* Pirates! needs this to load */
		LOG(LOG_BIOS, LOG_NORMAL)("INT13: Function %x called on drive %x (dos drive %d) reg_al %x", reg_ah, reg_dl, drivenum, reg_al);
		killRead = true;
		reg_ah = 0x00;
		CALLBACK_SCF(false);
		break;
	case 0x18: /* Set media type for format : DOSVAX */
		LOG(LOG_BIOS, LOG_NORMAL)("INT13: Function %x called on drive %x (dos drive %d) reg_cx %x", reg_ah, reg_dl, drivenum, reg_cx);
		if (driveInactive(drivenum)) {
			reg_ah = 0x80;//drive not ready
			CALLBACK_SCF(true);
			return CBRET_NONE;
		}
		//Bit32u tmpheads, tmpcyl, tmpsect, tmpsize;
		Bit16u reqcyl, reqsec;
		reqcyl = reg_cl & 0xC0;
		reqcyl <<= 2;
		reqcyl |= reg_ch;
		reqcyl++; //CH = max_cyl - 1
		reqsec = reg_cl & 0x3F;
		imageDiskList[drivenum]->Get_Geometry(&tmpheads, &tmpcyl, &tmpsect, &tmpsize);
		if (reqcyl == tmpcyl && reqsec == tmpsect) {
			LOG(LOG_BIOS, LOG_NORMAL)("INT13: Function 18 returned 0x00 (accepted)");
			reg_ah = 0x00;
			CALLBACK_SCF(false);
		} else {
			LOG(LOG_BIOS, LOG_ERROR)("INT13: Function 18 returned 0x0C (denied)");
#if C_HEAVY_DEBUG
			LOG(LOG_BIOS, LOG_NORMAL)("CHS Req: %d-%d-%d, Cur: %d-%d-%d", reqcyl, 2, reqsec, tmpcyl, tmpheads, tmpsect);
#endif
			reg_ah = 0x0C;
			CALLBACK_SCF(true);
		}
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT13: Function %x called on drive %x (dos drive %d)", reg_ah,  reg_dl, drivenum);
		reg_ah=0x01;
		CALLBACK_SCF(true);
	}
	return CBRET_NONE;
}


void BIOS_SetupDisks(void) {
/* TODO Start the time correctly */
	call_int13=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13,&INT13_DiskHandler,CB_INT13,"Int 13 Bios disk");
	RealSetVec(0x13,CALLBACK_RealPointer(call_int13));
	int i;
	for(i=0;i<4;i++) {
		imageDiskList[i] = NULL;
	}

	for(i=0;i<MAX_SWAPPABLE_DISKS;i++) {
		diskSwap[i] = NULL;
	}

	diskparm0 = CALLBACK_Allocate();
	diskparm1 = CALLBACK_Allocate();
	swapPosition = 0;

	RealSetVec(0x41,CALLBACK_RealPointer(diskparm0));
	RealSetVec(0x46,CALLBACK_RealPointer(diskparm1));

	PhysPt dp0physaddr=CALLBACK_PhysPointer(diskparm0);
	PhysPt dp1physaddr=CALLBACK_PhysPointer(diskparm1);
	for(i=0;i<16;i++) {
		phys_writeb(dp0physaddr+i,0);
		phys_writeb(dp1physaddr+i,0);
	}

	imgDTASeg = 0;

/* Setup the Bios Area */
	mem_writeb(BIOS_HARDDISK_COUNT,2);

	MAPPER_AddHandler(swapInNextDisk,MK_f4,MMOD1,"swapimg","Swap Image");
	MAPPER_AddHandler(mountFloppyDiskDialog, MK_f3, MMOD1, "mountflp", "Mount Flp");
	killRead = false;
	swapping_requested = false;
}
