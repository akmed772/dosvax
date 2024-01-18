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
extern Bit8u* ps55font_24;//65536 * 24 * (24 bit)
extern Bit8u jfont_sbcs_19[SBCS19_LEN];//256 * 19( * 8 bit)
extern Bit8u jfont_dbcs_16[DBCS16_LEN];//65536 * 16 * (16 bit)

//inline functions
inline bool isKanji1(Bit8u chr) { return (chr >= 0x81 && chr <= 0x9f) || (chr >= 0xe0 && chr <= 0xfc); }
inline bool isKanji2(Bit8u chr) { return (chr >= 0x40 && chr <= 0x7e) || (chr >= 0x80 && chr <= 0xfc); }

typedef union {
	Bit32u d = 0;
	Bit8u b[4];
} FontPattern;

//Conversion table to generate 1/2 and 1/4 DBCS fonts from DBCS fonts. 
const Bit16u tbl_halfdbcs[][3] =
{
	//half width symbols (exclude JIS X 0201 katakanas)
	{0x400,0x2015,0},//ϊUin IBM Ext Kanji
	{0x401,0x118,0},//X
	{0x402,0x151,0},//
	{0x403,0x189,0},
	{0x404,0x14c,0x10},//
	{0x405,0x150,0},
	{0x406,0x14a,0x10},//
	{0x407,0x14b,0x10},//
	{0x408,0x144,0},
	{0x409,0x145,0},
	{0x40a,0x13f,0},
	{0x40b,0x141,0},
	{0x40c,0x13e,0},
	{0x40d,0x13d,0},
	{0x40e,0x139,0x1a},//y
	{0x40f,0x13a,0x11},//z
	{0x410,0x137,0x1a},//w
	{0x411,0x138,0x11},//x
	{0x412,0x133,0x1a},//s
	{0x413,0x134,0x11},//t
	{0x414,0x131,0x1a},//q
	{0x415,0x132,0x11},//r
	{0x416,0x12b,0x1a},//k
	{0x417,0x12c,0x11},//l
	{0x418,0x127,0x1c},//g
	{0x419,0x128,0x10},//h
	{0x41a,0x125,0x1c},//e
	{0x41b,0x126,0x10},//f
	{0x41c,0x11f,0},
	{0x41d,0x120,0},
	{0x41e,0x11c,0},
	{0x41f,0x11d,0},
	{0x420,0x112,0},
	{0x421,0x113,0},
	{0x422,0x10c,0},
	{0x423,0x10e,0},
	//half width katakanas (exclude JIS X 0201 katakanas)
	{0x424,0x2cd,0},
	{0x425,0x283,0},
	{0x426,0x285,0},
	{0x427,0x287,0},
	{0x428,0x289,0},
	{0x429,0x28b,0},
	{0x42a,0x28d,0},
	{0x42b,0x28f,0},
	{0x42c,0x291,0},
	{0x42d,0x293,0},
	{0x42e,0x295,0},
	{0x42f,0x297,0},
	{0x430,0x299,0},//430
	{0x431,0x29c,0},
	{0x432,0x29e,0},
	{0x433,0x2a0,0},
	{0x434,0x2a7,0},
	{0x435,0x2a8,0},
	{0x436,0x2aa,0},
	{0x437,0x2ab,0},
	{0x438,0x2ad,0},
	{0x439,0x2ae,0},
	{0x43a,0x2b0,0},
	{0x43b,0x2b1,0},
	{0x43c,0x2b3,0},
	{0x43d,0x2b4,0},
	{0x43e,0x2c5,0},
	{0x43f,0x2c7,0},
	{0x440,0x2c8,0},
	{0x441,0x2cb, 0},
	{0x442, 0x2cc,0},//442
	{0x443,0x0,0},
	{0x444,0x0,0},
	{0x445,0x0,0},
	{0x446,0x20,0x90},//446 1/4Chr space
	{0x446,0x162,0x2c},//overwrite 446 1/4Chr Triangle
	{0x447,0x0,0},
	{0x448,0x1b,0x90},//448 ? Return symbol?
	{0x449,0x0,0},//449 Horizontal page border?
	{0x44a,0x0,0},
	{0x44b,0x0,0},
	{0x44c,0,0},//44C ?
	{0x44d,0x124,0},//44D Two center dots
	{0x44e,0x0,0},
	{0x44f,0x0,0},
	{0x450,0,0},//450 ?
	{0x451,0x0,0},
	{0x452,0x0,0},
	//half width hiraganas (453-4a5h)
	{0x453,0x21A,0},
	{0x454,0x21B,0},
	{0x455,0x21C,0},
	{0x456,0x21D,0},
	{0x457,0x21E,0},
	{0x458,0x21F,0},
	{0x459,0x220,0},
	{0x45a,0x221,0},
	{0x45b,0x222,0},
	{0x45c,0x223,0},
	{0x45d,0x224,0},
	{0x45e,0x225,0},
	{0x45f,0x226,0},
	{0x460,0x227,0},
	{0x461,0x228,0},
	{0x462,0x229,0},
	{0x463,0x22A,0},
	{0x464,0x22B,0},
	{0x465,0x22C,0},
	{0x466,0x22D,0},
	{0x467,0x22E,0},
	{0x468,0x22F,0},
	{0x469,0x230,0},
	{0x46a,0x231,0},
	{0x46b,0x232,0},
	{0x46c,0x233,0},
	{0x46d,0x234,0},
	{0x46e,0x235,0},
	{0x46f,0x236,0},
	{0x470,0x237,0},
	{0x471,0x238,0},
	{0x472,0x239,0},
	{0x473,0x23A,0},
	{0x474,0x23B,0},
	{0x475,0x23C,0},
	{0x476,0x23D,0},
	{0x477,0x23E,0},
	{0x478,0x23F,0},
	{0x479,0x240,0},
	{0x47a,0x241,0},
	{0x47b,0x242,0},
	{0x47c,0x243,0},
	{0x47d,0x244,0},
	{0x47e,0x245,0},
	{0x47f,0x246,0},
	{0x480,0x247,0},
	{0x481,0x248,0},
	{0x482,0x249,0},
	{0x483,0x24A,0},
	{0x484,0x24B,0},
	{0x485,0x24C,0},
	{0x486,0x24D,0},
	{0x487,0x24E,0},
	{0x488,0x24F,0},
	{0x489,0x250,0},
	{0x48a,0x251,0},
	{0x48b,0x252,0},
	{0x48c,0x253,0},
	{0x48d,0x254,0},
	{0x48e,0x255,0},
	{0x48f,0x256,0},
	{0x490,0x257,0},
	{0x491,0x258,0},
	{0x492,0x259,0},
	{0x493,0x25A,0},
	{0x494,0x25B,0},
	{0x495,0x25C,0},
	{0x496,0x25D,0},
	{0x497,0x25E,0},
	{0x498,0x25F,0},
	{0x499,0x260,0},
	{0x49a,0x261,0},
	{0x49b,0x262,0},
	{0x49c,0x263,0},
	{0x49d,0x264,0},
	{0x49e,0x265,0},
	{0x49f,0x266,0},
	{0x4a0,0x267,0},
	{0x4a1,0x268,0},
	{0x4a2,0x269,0},
	{0x4a3,0x26A,0},
	{0x4a4,0x26B,0},
	{0x4a5,0x26C,0},
	//1/4 Hiraganas
	{ 0x402,0x21a,0x30 },
	{ 0x403,0x21b,0x30 },
	{ 0x404,0x21c,0x30 },
	{ 0x405,0x21d,0x30 },
	{ 0x406,0x21e,0x30 },
	{ 0x407,0x21f,0x30 },
	{ 0x408,0x220,0x30 },
	{ 0x409,0x221,0x30 },
	{ 0x40a,0x222,0x30 },
	{ 0x40b,0x223,0x30 },
	{ 0x40c,0x224,0x30 },
	{ 0x40d,0x225,0x30 },
	{ 0x40e,0x226,0x30 },
	{ 0x40f,0x227,0x30 },
	{ 0x410,0x228,0x30 },
	{ 0x411,0x229,0x30 },
	{ 0x412,0x22a,0x30 },
	{ 0x413,0x22b,0x30 },
	{ 0x414,0x22c,0x30 },
	{ 0x415,0x22d,0x30 },
	{ 0x416,0x22e,0x30 },
	{ 0x417,0x22f,0x30 },
	{ 0x418,0x230,0x30 },
	{ 0x419,0x231,0x30 },
	{ 0x41a,0x232,0x30 },
	{ 0x41b,0x233,0x30 },
	{ 0x41c,0x234,0x30 },
	{ 0x41d,0x235,0x30 },
	{ 0x41e,0x236,0x30 },
	{ 0x41f,0x237,0x30 },
	{ 0x420,0x238,0x30 },
	{ 0x421,0x239,0x30 },
	{ 0x422,0x23a,0x30 },
	{ 0x423,0x23b,0x30 },
	{ 0x424,0x23c,0x30 },
	{ 0x425,0x23d,0x30 },
	{ 0x426,0x23e,0x30 },
	{ 0x427,0x23f,0x30 },
	{ 0x428,0x240,0x30 },
	{ 0x429,0x241,0x30 },
	{ 0x42a,0x242,0x30 },
	{ 0x42b,0x243,0x30 },
	{ 0x42c,0x244,0x30 },
	{ 0x42d,0x245,0x30 },
	{ 0x42e,0x246,0x30 },
	{ 0x42f,0x247,0x30 },
	{ 0x430,0x248,0x30 },
	{ 0x431,0x249,0x30 },
	{ 0x432,0x24a,0x30 },
	{ 0x433,0x24b,0x30 },
	{ 0x434,0x24c,0x30 },
	{ 0x435,0x24d,0x30 },
	{ 0x436,0x24e,0x30 },
	{ 0x437,0x24f,0x30 },
	{ 0x438,0x250,0x30 },
	{ 0x439,0x251,0x30 },
	{ 0x43a,0x252,0x30 },
	{ 0x43b,0x253,0x30 },
	{ 0x43c,0x254,0x30 },
	{ 0x43d,0x255,0x30 },
	{ 0x43e,0x256,0x30 },
	{ 0x43f,0x257,0x30 },
	{ 0x440,0x258,0x30 },
	{ 0x441,0x259,0x30 },
	{ 0x442,0x25a,0x30 },
	{ 0x443,0x25b,0x30 },
	{ 0x444,0x25c,0x30 },
	{ 0x445,0x25d,0x30 },
	{ 0x446,0x25e,0x30 },
	{ 0x447,0x25f,0x30 },
	{ 0x448,0x260,0x30 },
	{ 0x449,0x261,0x30 },
	{ 0x44a,0x262,0x30 },
	{ 0x44b,0x263,0x30 },
	{ 0x44c,0x264,0x30 },
	{ 0x44d,0x265,0x30 },
	{ 0x44e,0x266,0x30 },
	{ 0x44f,0x268,0x30 },
	{ 0x450,0x26b,0x30 },
	{ 0x451,0x26c,0x30 },
	//1/4 Katakanas
	{ 0x452,0x278,0x30 },
	{ 0x453,0x279,0x30 },
	{ 0x454,0x27a,0x30 },
	{ 0x455,0x27b,0x30 },
	{ 0x456,0x27c,0x30 },
	{ 0x457,0x27d,0x30 },
	{ 0x458,0x27e,0x30 },
	{ 0x459,0x27f,0x30 },
	{ 0x45a,0x280,0x30 },
	{ 0x45b,0x281,0x30 },
	{ 0x45c,0x282,0x30 },
	{ 0x45d,0x283,0x30 },
	{ 0x45e,0x284,0x30 },
	{ 0x45f,0x285,0x30 },
	{ 0x460,0x286,0x30 },
	{ 0x461,0x287,0x30 },
	{ 0x462,0x288,0x30 },
	{ 0x463,0x289,0x30 },
	{ 0x464,0x28a,0x30 },
	{ 0x465,0x28b,0x30 },
	{ 0x466,0x28c,0x30 },
	{ 0x467,0x28d,0x30 },
	{ 0x468,0x28e,0x30 },
	{ 0x469,0x28f,0x30 },
	{ 0x46a,0x290,0x30 },
	{ 0x46b,0x291,0x30 },
	{ 0x46c,0x292,0x30 },
	{ 0x46d,0x293,0x30 },
	{ 0x46e,0x294,0x30 },
	{ 0x46f,0x295,0x30 },
	{ 0x470,0x296,0x30 },
	{ 0x471,0x297,0x30 },
	{ 0x472,0x298,0x30 },
	{ 0x473,0x299,0x30 },
	{ 0x474,0x29a,0x30 },
	{ 0x475,0x29b,0x30 },
	{ 0x476,0x29c,0x30 },
	{ 0x477,0x29d,0x30 },
	{ 0x478,0x29e,0x30 },
	{ 0x479,0x29f,0x30 },
	{ 0x47a,0x2a0,0x30 },
	{ 0x47b,0x2a1,0x30 },
	{ 0x47c,0x2a2,0x30 },
	{ 0x47d,0x2a3,0x30 },
	{ 0x47e,0x2a4,0x30 },
	{ 0x47f,0x2a5,0x30 },
	{ 0x480,0x2a6,0x30 },
	{ 0x481,0x2a7,0x30 },
	{ 0x482,0x2a8,0x30 },
	{ 0x483,0x2a9,0x30 },
	{ 0x484,0x2aa,0x30 },
	{ 0x485,0x2ab,0x30 },
	{ 0x486,0x2ac,0x30 },
	{ 0x487,0x2ad,0x30 },
	{ 0x488,0x2ae,0x30 },
	{ 0x489,0x2af,0x30 },
	{ 0x48a,0x2b0,0x30 },
	{ 0x48b,0x2b1,0x30 },
	{ 0x48c,0x2b2,0x30 },
	{ 0x48d,0x2b3,0x30 },
	{ 0x48e,0x2b4,0x30 },
	{ 0x48f,0x2b5,0x30 },
	{ 0x490,0x2b6,0x30 },
	{ 0x491,0x2b7,0x30 },
	{ 0x492,0x2b8,0x30 },
	{ 0x493,0x2b9,0x30 },
	{ 0x494,0x2ba,0x30 },
	{ 0x495,0x2bb,0x30 },
	{ 0x496,0x2bc,0x30 },
	{ 0x497,0x2bd,0x30 },
	{ 0x498,0x2be,0x30 },
	{ 0x499,0x2bf,0x30 },
	{ 0x49a,0x2c0,0x30 },
	{ 0x49b,0x2c1,0x30 },
	{ 0x49c,0x2c2,0x30 },
	{ 0x49d,0x2c3,0x30 },
	{ 0x49e,0x2c4,0x30 },
	{ 0x49f,0x2c6,0x30 },
	{ 0x4a0,0x2c9,0x30 },
	{ 0x4a1,0x2ca,0x30 },
	{ 0x4a2,0x2cb,0x30 },//
	{ 0x4a3,0x2cc,0x30 },//
	{ 0x4a4,0x2cd,0x30 },//
	////1/4 a-z
	//{ 0x4a5,0x1fc,0x30 },
	//{ 0x4a6,0x1fd,0x30 },
	//{ 0x4a7,0x1fe,0x30 },
	//{ 0x4a8,0x1ff,0x30 },
	//{ 0x4a9,0x200,0x30 },
	//{ 0x4aa,0x201,0x30 },
	//{ 0x4ab,0x202,0x30 },
	//{ 0x4ac,0x203,0x30 },
	//{ 0x4ad,0x204,0x30 },
	//{ 0x4ae,0x205,0x30 },
	//{ 0x4af,0x206,0x30 },
	//{ 0x4b0,0x207,0x30 },
	//{ 0x4b1,0x208,0x30 },
	//{ 0x4b2,0x209,0x30 },
	//{ 0x4b3,0x20a,0x30 },
	//{ 0x4b4,0x20b,0x30 },
	//{ 0x4b5,0x20c,0x30 },
	//{ 0x4b6,0x20d,0x30 },
	//{ 0x4b7,0x20e,0x30 },
	//{ 0x4b8,0x20f,0x30 },
	//{ 0x4b9,0x210,0x30 },
	//{ 0x4ba,0x211,0x30 },
	//{ 0x4bb,0x212,0x30 },
	//{ 0x4bc,0x213,0x30 },
	//{ 0x4bd,0x214,0x30 },
	//{ 0x4be,0x215,0x30 },
	////1/4 A-Z
	//{ 0x4bf,0x1dc,0x30 },
	//{ 0x4c0,0x1dd,0x30 },
	//{ 0x4c1,0x1de,0x30 },
	//{ 0x4c2,0x1df,0x30 },
	//{ 0x4c3,0x1e0,0x30 },
	//{ 0x4c4,0x1e1,0x30 },
	//{ 0x4c5,0x1e2,0x30 },
	//{ 0x4c6,0x1e3,0x30 },
	//{ 0x4c7,0x1e4,0x30 },
	//{ 0x4c8,0x1e5,0x30 },
	//{ 0x4c9,0x1e6,0x30 },
	//{ 0x4ca,0x1e7,0x30 },
	//{ 0x4cb,0x1e8,0x30 },
	//{ 0x4cc,0x1e9,0x30 },
	//{ 0x4cd,0x1ea,0x30 },
	//{ 0x4ce,0x1eb,0x30 },
	//{ 0x4cf,0x1ec,0x30 },
	//{ 0x4d0,0x1ed,0x30 },
	//{ 0x4d1,0x1ee,0x30 },
	//{ 0x4d2,0x1ef,0x30 },
	//{ 0x4d3,0x1f0,0x30 },
	//{ 0x4d4,0x1f1,0x30 },
	//{ 0x4d5,0x1f2,0x30 },
	//{ 0x4d6,0x1f3,0x30 },
	//{ 0x4d7,0x1f4,0x30 },
	//{ 0x4d8,0x1f5,0x30 },
	////0-9
	//{ 0x4d9,0x1cb,0x30 },
	//{ 0x4da,0x1cc,0x30 },
	//{ 0x4db,0x1cd,0x30 },
	//{ 0x4dc,0x1ce,0x30 },
	//{ 0x4dd,0x1cf,0x30 },
	//{ 0x4de,0x1d0,0x30 },
	//{ 0x4df,0x1d1,0x30 },
	//{ 0x4e0,0x1d2,0x30 },
	//{ 0x4e1,0x1d3,0x30 },
	//{ 0x4e2,0x1d4,0x30 },
	//1/4 a-z
	{ 0x4a5,0x61,0xb0 },
	{ 0x4a6,0x62,0xb0 },
	{ 0x4a7,0x63,0xb0 },
	{ 0x4a8,0x64,0xb0 },
	{ 0x4a9,0x65,0xb0 },
	{ 0x4aa,0x66,0xb0 },
	{ 0x4ab,0x67,0xb0 },
	{ 0x4ac,0x68,0xb0 },
	{ 0x4ad,0x69,0xb0 },
	{ 0x4ae,0x6a,0xb0 },
	{ 0x4af,0x6b,0xb0 },
	{ 0x4b0,0x6c,0xb0 },
	{ 0x4b1,0x6d,0xb0 },
	{ 0x4b2,0x6e,0xb0 },
	{ 0x4b3,0x6f,0xb0 },
	{ 0x4b4,0x70,0xb0 },
	{ 0x4b5,0x71,0xb0 },
	{ 0x4b6,0x72,0xb0 },
	{ 0x4b7,0x73,0xb0 },
	{ 0x4b8,0x74,0xb0 },
	{ 0x4b9,0x75,0xb0 },
	{ 0x4ba,0x76,0xb0 },
	{ 0x4bb,0x77,0xb0 },
	{ 0x4bc,0x78,0xb0 },
	{ 0x4bd,0x79,0xb0 },
	{ 0x4be,0x7a,0xb0 },
	//1/4 A-Z
	{ 0x4bf,0x41,0xb0 },
	{ 0x4c0,0x42,0xb0 },
	{ 0x4c1,0x43,0xb0 },
	{ 0x4c2,0x44,0xb0 },
	{ 0x4c3,0x45,0xb0 },
	{ 0x4c4,0x46,0xb0 },
	{ 0x4c5,0x47,0xb0 },
	{ 0x4c6,0x48,0xb0 },
	{ 0x4c7,0x49,0xb0 },
	{ 0x4c8,0x4a,0xb0 },
	{ 0x4c9,0x4b,0xb0 },
	{ 0x4ca,0x4c,0xb0 },
	{ 0x4cb,0x4d,0xb0 },
	{ 0x4cc,0x4e,0xb0 },
	{ 0x4cd,0x4f,0xb0 },
	{ 0x4ce,0x50,0xb0 },
	{ 0x4cf,0x51,0xb0 },
	{ 0x4d0,0x52,0xb0 },
	{ 0x4d1,0x53,0xb0 },
	{ 0x4d2,0x54,0xb0 },
	{ 0x4d3,0x55,0xb0 },
	{ 0x4d4,0x56,0xb0 },
	{ 0x4d5,0x57,0xb0 },
	{ 0x4d6,0x58,0xb0 },
	{ 0x4d7,0x59,0xb0 },
	{ 0x4d8,0x5a,0xb0 },
	//0-9
	{ 0x4d9,0x30,0xb0 },
	{ 0x4da,0x31,0xb0 },
	{ 0x4db,0x32,0xb0 },
	{ 0x4dc,0x33,0xb0 },
	{ 0x4dd,0x34,0xb0 },
	{ 0x4de,0x35,0xb0 },
	{ 0x4df,0x36,0xb0 },
	{ 0x4e0,0x37,0xb0 },
	{ 0x4e1,0x38,0xb0 },
	{ 0x4e2,0x39,0xb0 },
	//1/4 Symbols
	{ 0x400,0x189,0x30 },//Κ
	{ 0x401,0x118,0x30 },//X
	{ 0x4d3,0x21,0xa0 },//!
	{ 0x4d4,0x22,0xa0 },//"
	{ 0x4d5,0x23,0xa0 },//#
	{ 0x4d6,0x24,0xa0 },//$
	{ 0x4d7,0x25,0xa0 },//%
	{ 0x4d8,0x26,0xa0 },//&
	{ 0x4d9,0x27,0xa0 },//'
	{ 0x4da,0x28,0xa0 },//(
	{ 0x4db,0x29,0xa0 },//)
	{ 0x4dc,0x2a,0xa0 },//*
	{ 0x4dd,0x2b,0xa0 },//+
	{ 0x4de,0x2c,0xa0 },//,
	{ 0x4df,0x2d,0xa0 },//-
	{ 0x4e0,0x2e,0xa0 },//.
	{ 0x4e1,0x2f,0xa0 },///
	{ 0x4e2,0x3a,0xa0 },//:
	{ 0x4e3,0x3b,0xa0 },//;
	{ 0x4e4,0x3c,0xa0 },//<
	{ 0x4e5,0x3d,0xa0 },//=
	{ 0x4e6,0x3e,0xa0 },//>
	{ 0x4e7,0x3f,0xa0 },//?
	{ 0x4e8,0x40,0xa0 },//@
	{ 0x4e9,0x5b,0xa0 },//[
	{ 0x4ea,0x5c,0xa0 },//YEN
	{ 0x4eb,0x5d,0xa0 },//]
	{ 0x4ec,0x5e,0xa0 },//^
	{ 0x4ed,0x5f,0xa0 },//_
	{ 0x4ee,0x60,0xa0 },//`
	{ 0x4ef,0x7b,0xa0 },//{
	{ 0x4f0,0x7c,0xa0 },//|
	{ 0x4f1,0x7d,0xa0 },//}
	{ 0x4f2,0x7e,0xa0 },//1/4 OVERLINE
	{ 0x4f3,0x102,0x20 },//B
	{ 0x4f4,0x135,0x20 },//u
	{ 0x4f5,0x136,0x20 },//v
	{ 0x4f6,0x101,0x20 },//A
	{ 0x4f7,0x105,0x20 },//E
	{ 0x4f8,0x11b,0x20 },//1/4 [
	{ 0x4f9,0x10a,0x20 },//1/4 J
	{ 0x4fa,0x10b,0x20 },//1/4 K
	{ 0x4fb,0x11f,0x20 },//1/4 BACKSLASH
	{ 0x4fc,0x120,0x20 },//~
	{ 0x4fd,0x137,0x20 },//w
	{ 0x4fe,0x138,0x20 },//x
	{ 0x4ff,0x151,0x20 },//
	{ 0, 0, 0 }//end
};
