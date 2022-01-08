#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

//jfontload.cpp
#define SBCS19_LEN 256 * 19
#define DBCS16_LEN 65536 * 32
#define SBCS24_CHARS 256
#define SBCS24_LEN SBCS24_CHARS * 48
#define DBCS24_CHARS 0x2D0F//=SJIS FCFCh
#define DBCS24_LEN DBCS24_CHARS * 72

Bit16u SJIStoIBMJ(Bit16u knj);
Bit16u IBMJtoSJIS(Bit16u knj);

//extern Bit16u jfont_sbcs_24[SBCS24_LEN];//256 * 24 * (16 bit)
extern Bit8u ps55font_24[DBCS24_LEN];//65536 * 24 * (24 bit)
extern Bit8u jfont_sbcs_19[SBCS19_LEN];//256 * 19( * 8 bit)
extern Bit8u jfont_dbcs_16[DBCS16_LEN];//65536 * 16 * (16 bit)

//inline functions
inline bool isKanji1(Bit8u chr) { return (chr >= 0x81 && chr <= 0x9f) || (chr >= 0xe0 && chr <= 0xfc); }
inline bool isKanji2(Bit8u chr) { return (chr >= 0x40 && chr <= 0x7e) || (chr >= 0x80 && chr <= 0xfc); }