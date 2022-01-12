 /*
 Copyright (c) 2016-2022 akm
 All rights reserved.
 This content is under the MIT License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "control.h"
#include "support.h"
#include "jsupport.h"
//#include "jega.h"//for AX
//#include "ps55.h"//for PS/55
using namespace std;

#define SBCS 0
#define DBCS 1
#define ID_LEN 6
#define NAME_LEN 8
Bit8u jfont_sbcs_19[SBCS19_LEN];//256 * 19( * 8 bit)
Bit8u jfont_dbcs_16[DBCS16_LEN];//65536 * 16 * (16 bit)
//Bit16u jfont_sbcs_24[SBCS24_LEN];//256 * 24 * (16 bit)
Bit8u ps55font_24[DBCS24_LEN];//65536 * 24 * (24 bit)

typedef struct {
    char id[ID_LEN];
    char name[NAME_LEN];
    unsigned char width;
    unsigned char height;
    unsigned char type;
} fontx_h;

typedef struct {
    Bit16u start;
	Bit16u end;
} fontxTbl;

Bit16u chrtosht(FILE *fp)
{
	Bit16u i, j;
	i = (Bit8u)getc(fp);
	j = (Bit8u)getc(fp) << 8;
	return(i | j);
}

/*
--------------------------------81=====9F---------------E0=====FC-: High byte of SJIS
----------------40=========7E-80===============================FC-:  Low byte of SJIS
00=====1E1F=====3B------------------------------------------------: High byte of IBMJ
00=========3839===============================FB------------------:  Low byte of IBMJ

 A     B     C
 2ADC, 2C5F, +5524  --> 8000
 2614, 2673, +90EC  --> B700
 2384, 2613, +906C  --> B3F0
 0682, 1F7D, +0000
*/
//convert Shift JIS code to PS/55 internal char code
Bit16u SJIStoIBMJ(Bit16u knj)
{
	//verify code
	if (knj < 0x8140 || knj > 0xfc4b) return 0xffff;
	Bitu knj1 = (knj >> 8) & 0xff;// = FCh - 40h //higher code (ah)
	Bitu knj2 = knj & 0xff;//lower code (al)
	knj1 -= 0x81;
	if (knj1 > 0x5E) knj1 -= 0x40;
	knj2 -= 0x40;
	if (knj2 > 0x3F) knj2--;
	knj1 *= 0xBC;
	knj = knj1 + knj2;
	knj += 0x100;
	//shift code out of JIS standard
	if (knj >= 0x2ADC && knj <= 0x2C5F)
		knj += 0x5524;
	else if (knj >= 0x2384 && knj <= 0x2613)
		knj += 0x906c;
	else if (knj >= 0x2614 && knj <= 0x2673)
		knj += 0x90ec;
	return knj;
}

//Convert internal char code to Shift JIS code (obsolete)
Bit16u IBMJtoSJIS(Bit16u knj)
{
	//verify code
	if (knj < 0x100) return 0xffff;
	knj -= 0x100;
	if (knj <= 0x1f7d)
		;//do nothing
	else if(knj >= 0xb700 && knj <= 0xb75f) {
		knj -= 0x90ec;
	}
	else if (knj >= 0xb3f0 && knj <= 0xb67f) {
		knj -= 0x906c;
	}
	else if (knj >= 0x8000 && knj <= 0x8183)
	{
		knj -= 0x5524;
	}
	else
		return 0xffff;
	Bitu knj1 =  knj / 0xBC;// = FCh - 40h //higher code (ah)
	Bitu knj2 = knj - (knj1 * 0xBC);//lower code (al)
	//knj1 = knj >> 8;//higher code (ah)
	knj1 += 0x81;
	if (knj1 > 0x9F) knj1 += 0x40;
	knj2 += 0x40;
	if (knj2 > 0x7E) knj2++;
	//verify code
	if (!isKanji1(knj1)) return 0xffff;
	if (!isKanji2(knj2)) return 0xffff;
	knj = knj1 << 8;
	knj |= knj2;
	return knj;
}

Bitu getfontx2header(FILE *fp, fontx_h *header)
{
    fread(header->id, ID_LEN, 1, fp);
    if (strncmp(header->id, "FONTX2", ID_LEN) != 0) {
	return 1;
    }
    fread(header->name, NAME_LEN, 1, fp);
    header->width = (Bit8u)getc(fp);
    header->height = (Bit8u)getc(fp);
    header->type = (Bit8u)getc(fp);
    return 0;
}

void readfontxtbl(fontxTbl *table, Bitu size, FILE *fp)
{
    while (size > 0) {
	table->start = chrtosht(fp);
	table->end = chrtosht(fp);
	++table;
	--size;
    }
}


static Bitu LoadFontxFile(const char * fname) {
    fontx_h head;
    fontxTbl *table;
	Bit8u buf;
    Bitu code;
    Bit8u size;
	Bitu font;
	Bitu ibmcode;
	if (!fname) return 127;
	if(*fname=='\0') return 127;
	FILE * mfile=fopen(fname,"rb");
	if (!mfile) {
		LOG_MSG("MSG: Can't open FONTX2 file: %s",fname);
		return 127;
	}
	if (getfontx2header(mfile, &head) != 0) {
		fclose(mfile);
		LOG_MSG("MSG: FONTX2 header is incorrect\n");
		return 1;
    }
	// switch whether the font is DBCS or not
	if (head.type == DBCS) {
		if (head.width == 16 && head.height == 16) {//for JEGA (AX)
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++)
				for (code = table[i].start; code <= table[i].end; code++)
					fread(&jfont_dbcs_16[(code * 32)], sizeof(Bit8u), 32, mfile);
		} else if (head.width == 24 && head.height == 24) {//for PS/55
			size = getc(mfile);
			table = (fontxTbl *)calloc(size, sizeof(fontxTbl));
			readfontxtbl(table, size, mfile);
			for (Bitu i = 0; i < size; i++)
				for (code = table[i].start; code <= table[i].end; code++) {
					ibmcode = SJIStoIBMJ(code);
					if (ibmcode >= 0x8000 && ibmcode <= 0x8183)//IBM extensions
						ibmcode -= 0x6000;
					if (ibmcode > DBCS24_CHARS) {//address is out of memory
						LOG_MSG("MSG: FONTX2 DBCS code point %X may be wrong\n", code);
						continue;
					}
					fread(&ps55font_24[(ibmcode * 72)], sizeof(Bit8u), 72, mfile);
				}
		}
		else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 DBCS font size is not correct\n");
			return 4;
		}
	}
    else {
		if (head.width == 8 && head.height == 19) {
			fread(jfont_sbcs_19, sizeof(Bit8u), SBCS19_LEN, mfile);
		}
		else if (head.width == 12 && head.height == 24) {
			//fread(jfont_sbcs_24, sizeof(Bit16u), SBCS24_LEN, mfile);
			//fread(&jfont_dbcs_24[72 * 0x2780], sizeof(Bit8u), SBCS24_LEN, mfile);
			for (Bitu i = 0; i < SBCS24_CHARS; i++) {
				ps55font_24[0x98000 + i * 64] = 0;
				ps55font_24[0x98000 + i * 64 + 1] = 0;
				ps55font_24[0x98000 + i * 64 + 2] = 0;
				ps55font_24[0x98000 + i * 64 + 3] = 0;
				ps55font_24[0x98000 + i * 64 + 52] = 0;
				ps55font_24[0x98000 + i * 64 + 53] = 0;
				ps55font_24[0x98000 + i * 64 + 54] = 0;
				ps55font_24[0x98000 + i * 64 + 55] = 0;
				ps55font_24[0x98000 + i * 64 + 56] = 0;
				ps55font_24[0x98000 + i * 64 + 57] = 0;
				for (Bitu line = 2 + 0; line < 2 + 24; line++)
				{
					fread(&buf, sizeof(Bit8u), 1, mfile);
					font = buf << 8;
					fread(&buf, sizeof(Bit8u), 1, mfile);
					font |= buf;
					font >> 1;
					ps55font_24[0x98000 + i * 64 + line * 2 + 1] = font & 0xff;
					ps55font_24[0x98000 + i * 64 + line * 2] = (font >> 8) & 0xff;
				}
			}

		}
		else {
			fclose(mfile);
			LOG_MSG("MSG: FONTX2 SBCS font size is not correct\n");
			return 4;
		}
    }
	fclose(mfile);
	return 0;
}
/*
Font specification
BF800h
24 x 24
Font data 120 (78h) bytes
FF FF FF F8
xxxx xxxx, xxxx xxxx, xxxx xxxx, xxxx x000
width 29 pixels ?
height 30 pixels ?
footer? 8 (8h) bytes
00 00 00 00 20 07 20 00

$JPNHZ24.FNT
12 x 24
Font data 48 bytes
FF F, F FF

$SYSHN24.FNT
13 x 30
Font data 60 (3Ch) bytes
FF F8
xxxx xxxx, xxxx x000
width 13 pixels
height 30 pixels ?
need to exclude 0x81-90, 0xE0-FC
*/
//Font Loader for $JPNHN24.FNT included in DOS/V
static Bitu LoadDOSVHNFontFile(const char* fname) {
	Bit8u buf;
	Bitu code = 0;
	Bit8u size;
	Bitu font;
	Bitu fsize;
	if (!fname) return 127;
	if (*fname == '\0') return 127;
	FILE* mfile = fopen(fname, "rb");
	if (!mfile) {
		LOG_MSG("MSG: Can't open IBM font file: %s", fname);
		return 127;
	}
	fseek(mfile, 0, SEEK_END);
	fsize = ftell(mfile);//get filesize
	fseek(mfile, 0, SEEK_SET);
	if (fsize != 12288) {
		//LOG_MSG("The size of font file is incorrect \n");
		fclose(mfile);
		return 1;
	}
	Bitu j = 0;
	while (ftell(mfile) < fsize) {
		fread(&buf, sizeof(Bit8u), 1, mfile);
		font = buf << 7;
		fread(&buf, sizeof(Bit8u), 1, mfile);
		font |= (buf & 0xf0) >> 1;
		ps55font_24[0x98000 + code * 64] = 0;
		ps55font_24[0x98000 + code * 64 + 1] = 0;
		ps55font_24[0x98000 + code * 64 + 2] = 0;
		ps55font_24[0x98000 + code * 64 + 3] = 0;
		ps55font_24[0x98000 + code * 64 + 52] = 0;
		ps55font_24[0x98000 + code * 64 + 53] = 0;
		ps55font_24[0x98000 + code * 64 + 54] = 0;
		ps55font_24[0x98000 + code * 64 + 55] = 0;
		ps55font_24[0x98000 + code * 64 + 56] = 0;
		ps55font_24[0x98000 + code * 64 + 57] = 0;
		ps55font_24[0x98000 + code * 64 + j + 4] = font >> 8;//a b
		j++;
		ps55font_24[0x98000 + code * 64 + j + 4] = font & 0xff;//a b
		j++;
		font = (buf & 0x0f) << 11;
		fread(&buf, sizeof(Bit8u), 1, mfile);
		font |= buf << 3;
		ps55font_24[0x98000 + code * 64 + j + 4] = font >> 8;//a b
		j++;
		ps55font_24[0x98000 + code * 64 + j + 4] = font & 0xff;//a b
		j++;
		if (j >= 48) {
			code++;
			j = 0;
		}
	}
	return 0;
}
//Font Loader for $SYSHN24.FNT included in DOS K3.3
static Bitu LoadIBMHN24Font(const char* fname) {
	Bit8u buf;
	Bitu code = 0;
	Bitu fsize;
	if (!fname) return 127;
	if (*fname == '\0') return 127;
	FILE* mfile = fopen(fname, "rb");
	if (!mfile) {
		LOG_MSG("MSG: Can't open IBM font file: %s", fname);
		return 127;
	}
	fseek(mfile, 0, SEEK_END);
	fsize = ftell(mfile);//get filesize
	fseek(mfile, 0, SEEK_SET);
	if (fsize != 11760) {
		//LOG_MSG("The size of font file is incorrect \n");
		fclose(mfile);
		return 1;
	}
	Bitu j = 0;
	while (ftell(mfile) < fsize) {
		if (isKanji1(code)) {
			code++;
			continue;
		}
		fread(&buf, sizeof(Bit8u), 1, mfile);
		ps55font_24[0x98000 + code * 64 + j] = buf;//a b
		j++;
		if (j >= 60) {
			code++;
			j = 0;
		}
		 
	}
	return 0;
}

//Font Loader for $SYSEX24.FNT included in DOS K3.3
static Bitu LoadIBMEX24Font(const char* fname) {
	Bit8u buf;
	Bitu code = 0;
	Bitu fsize;
	if (!fname) return 127;
	if (*fname == '\0') return 127;
	FILE* mfile = fopen(fname, "rb");
	if (!mfile) {
		LOG_MSG("MSG: Can't open IBM font file: %s", fname);
		return 127;
	}
	fseek(mfile, 0, SEEK_END);
	fsize = ftell(mfile);//get filesize
	fseek(mfile, 0, SEEK_SET);
	if (fsize != 15360) {
		//LOG_MSG("The size of font file is incorrect \n");
		fclose(mfile);
		return 1;
	}
	Bitu j = 0;
	while (ftell(mfile) < fsize) {
		fread(&buf, sizeof(Bit8u), 1, mfile);
		ps55font_24[0xb0000 + code * 64 + j] = buf;//a b
		j++;
		if (j >= 60) {
			code++;
			j = 0;
		}
	}
	return 0;
}

static void LoadAnyFontFile(const char* fname)
{
	if (LoadFontxFile(fname) != 1) return;
	if (LoadDOSVHNFontFile(fname) != 1) return;
	if (LoadIBMHN24Font(fname) != 1) return;
	if (LoadIBMEX24Font(fname) != 1) return;
	LOG_MSG("MSG: The font file is incorrect or broken.\n");
}

//#define TESLENG 64
void JFONT_Init(Section_prop * section) {
	std::string file_name;
	Prop_path* pathprop = section->Get_path("jfontsbcs");
	//initialize memory array
	memset(jfont_sbcs_19, 0xff, SBCS19_LEN);
	memset(jfont_dbcs_16, 0xff, DBCS16_LEN);
	memset(ps55font_24, 0xff, DBCS24_LEN);
	//for (int i = 0; i < DBCS24_LEN / TESLENG; i++) {//for test
	//	jfont_dbcs_24[i * TESLENG + 2] = i & 0xff;
	//	jfont_dbcs_24[i * TESLENG + 4] = (i >> 8) & 0xff;
	//	jfont_dbcs_24[i * TESLENG + 6] = (i >> 16) & 0xff;
	//	jfont_dbcs_24[i * TESLENG + 8] = (i >> 24) & 0xff;
	//	jfont_dbcs_24[i * TESLENG + 10] = 0xaa;//1010 1010
	//	jfont_dbcs_24[i * TESLENG + 12] = 0xcc;//1100 1100 
	//	jfont_dbcs_24[i * TESLENG + 14] = 0xf0;//1111 0000
	//}
	if (pathprop) LoadFontxFile(pathprop->realpath.c_str());
	else LOG_MSG("MSG: SBCS font file path is not specified.\n");
	pathprop = section->Get_path("jfontdbcs");
	if(pathprop) LoadFontxFile(pathprop->realpath.c_str());
	else LOG_MSG("MSG: DBCS font file path is not specified.\n");
	pathprop = section->Get_path("jfont24sbcs");
	if (pathprop) LoadAnyFontFile(pathprop->realpath.c_str());
	else LOG_MSG("MSG: SBCS24 font file path is not specified.\n");
	pathprop = section->Get_path("jfont24dbcs");
	if (pathprop) LoadFontxFile(pathprop->realpath.c_str());
	else LOG_MSG("MSG: DBCS24 font file path is not specified.\n");
	pathprop = section->Get_path("jfont24sbex");
	if (pathprop) LoadAnyFontFile(pathprop->realpath.c_str());
	//else LOG_MSG("MSG: SBEX24 font file path is not specified.\n");
}
