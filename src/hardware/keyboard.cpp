/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *  Copyright (C) 2016-2022 akm
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
#include "cpu.h"

#include "dosbox.h"
#include "keyboard.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "jega.h"

#define KEYBUFSIZE 32
#define KEYDELAY 0.300f			//Considering 20-30 khz serial clock and 11 bits/char

enum KeyCommands {
	CMD_NONE,
	CMD_SETLEDS,
	CMD_SETTYPERATE,
	CMD_SETOUTPORT,
	CMD_SELSCANCODES //for PS/55
};

static struct {
	Bit8u buffer[KEYBUFSIZE];
	Bitu used;
	Bitu pos;
	struct {
		KBD_KEYS key;
		Bitu wait;
		Bitu pause,rate;
	} repeat;
	KeyCommands command;
	Bit8u p60data;
	bool p60changed;
	bool active;
	bool scanning;
	bool scheduled;
	bool kanaLocked = false;//for AX
	Bit8u scancodeset = 02;//for PS/55
	bool rAltPressed = false;//for PS/55
	bool lShiftPressed = false;//for PS/55
	bool lCtrlPressed = false;//for PS/55
	bool key5576special = false;//for PS/55
} keyb;

static void KEYBOARD_SetPort60(Bit8u val) {
	keyb.p60changed=true;
	keyb.p60data=val;
	//LOG(LOG_KEYBOARD, LOG_NORMAL)("SetPort60 called: val=%0X", val);
	if (machine==MCH_PCJR) PIC_ActivateIRQ(6);
	else PIC_ActivateIRQ(1);
}

static void KEYBOARD_TransferBuffer(Bitu /*val*/) {
	keyb.scheduled = false;
	if (!keyb.used) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Transfer started with empty buffer");
		return;
	}
	KEYBOARD_SetPort60(keyb.buffer[keyb.pos]);
	if (++keyb.pos >= KEYBUFSIZE) keyb.pos -= KEYBUFSIZE;
	keyb.used--;
}


void KEYBOARD_ClrBuffer(void) {
	keyb.used=0;
	keyb.pos=0;
	PIC_RemoveEvents(KEYBOARD_TransferBuffer);
	keyb.scheduled=false;
}

static void KEYBOARD_AddBuffer(Bit8u data) {
	if (keyb.used>=KEYBUFSIZE) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Buffer full, dropping code");
		return;
	}
	Bitu start=keyb.pos+keyb.used;
	if (start>=KEYBUFSIZE) start-=KEYBUFSIZE;
	keyb.buffer[start]=data;
	keyb.used++;
	/* Start up an event to start the first IRQ */
	if (!keyb.scheduled && !keyb.p60changed) {
		keyb.scheduled=true;
		PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
	}
}


static Bitu read_p60(Bitu /*port*/,Bitu /*iolen*/) {
	keyb.p60changed = false;
	if (!keyb.scheduled && keyb.used) {
		keyb.scheduled = true;
		PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
	}
	//LOG(LOG_KEYBOARD, LOG_NORMAL)("read_p60 called: val=%0X", keyb.p60data);
	return keyb.p60data;
}

static void write_p60(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	//LOG(LOG_KEYBOARD, LOG_NORMAL)("Port 60 write with val %" sBitfs(X), val);
	switch (keyb.command) {
	case CMD_NONE:	/* None */
		/* No active command this would normally get sent to the keyboard then */
		KEYBOARD_ClrBuffer();
		switch (val) {
		case 0xed:	/* Set Leds */
			keyb.command=CMD_SETLEDS;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xee:	/* Echo */
			KEYBOARD_AddBuffer(0xee);	/* Echo */
			break;
		case 0xf0:	/* Select alternate scan codes (for PS/55) */
			keyb.command = CMD_SELSCANCODES;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf2:	/* Identify keyboard */
			/* AT's just send acknowledge */
			if (IS_PS55_ARCH) {
				//for PS/55
				KEYBOARD_AddBuffer(0xAB);
				KEYBOARD_AddBuffer(0x90);//90 AB = 5576-002
			}
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf3: /* Typematic rate programming */
			keyb.command=CMD_SETTYPERATE;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf4:	/* Enable keyboard,clear buffer, start scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Clear buffer,enable Scaning");
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			keyb.scanning=true;
			break;
		case 0xf5:	 /* Reset keyboard and disable scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, disable scanning");
			keyb.scanning=false;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf6:	/* Reset keyboard and enable scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, enable scanning");
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			keyb.scanning=false;
			break;
		case 0x05:// for AX;to avoid freezing AX version of Windows 2.1
			KEYBOARD_AddBuffer(0x00);	/* ? */
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		default:
			/* Just always acknowledge strange commands */
			LOG(LOG_KEYBOARD,LOG_ERROR)("60:Unhandled command %" sBitfs(X),val);
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
		}
		return;
	case CMD_SETOUTPORT:
		MEM_A20_Enable((val & 2)>0);
		keyb.command = CMD_NONE;
		break;
	case CMD_SETTYPERATE: 
		{
			static const int delay[] = { 250, 500, 750, 1000 };
			static const int repeat[] =
				{ 33,37,42,46,50,54,58,63,67,75,83,92,100,
				  109,118,125,133,149,167,182,200,217,233,
				  250,270,303,333,370,400,435,476,500 };
			keyb.repeat.pause = delay[(val>>5)&3];
			keyb.repeat.rate = repeat[val&0x1f];
			keyb.command=CMD_NONE;
		} /* Now go to setleds as it does what we want */
		/* FALLTHROUGH */
	case CMD_SETLEDS:
		keyb.command=CMD_NONE;
		KEYBOARD_ClrBuffer();
		KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
		break;
	case CMD_SELSCANCODES:
		//for PS/55
		if (val == 0) {//request codesetnum from system
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			KEYBOARD_AddBuffer(keyb.scancodeset);
		}
		else if (val == 0x8a) {
			keyb.scancodeset = val;
			keyb.repeat.rate = 92;//11
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			LOG(LOG_KEYBOARD, LOG_NORMAL)("Switched to scancode set 8A");
		}
		else if (val == 0x81) {
			keyb.scancodeset = val;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			LOG(LOG_KEYBOARD, LOG_NORMAL)("Switched to scancode set 81");
		}
		else
		{
			KEYBOARD_AddBuffer(0xfe); /* Resend */
			return;
		}
		keyb.repeat.key = KBD_NONE;
		keyb.repeat.pause = 500;
		keyb.repeat.wait = 0;
		keyb.command = CMD_NONE;
		break;
	}
}

extern bool TIMER_GetOutput2(void);
static Bit8u port_61_data = 0;
static Bitu read_p61(Bitu /*port*/,Bitu /*iolen*/) {
	if (TIMER_GetOutput2()) port_61_data |= 0x20;
	else                    port_61_data &=~0x20;
	port_61_data ^= 0x10;
	return port_61_data;
}

extern void TIMER_SetGate2(bool);
static void write_p61(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	if ((port_61_data ^ val) & 3) {
		if ((port_61_data ^ val) & 1) TIMER_SetGate2(val&0x1);
		PCSPEAKER_SetType(val & 3);
	}
	port_61_data = val;
}

static Bitu read_p62(Bitu /*port*/,Bitu /*iolen*/) {
	Bit8u ret = ~0x20;
	if (TIMER_GetOutput2()) ret |= 0x20;
	return ret;
}

static void write_p64(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	//LOG(LOG_KEYBOARD, LOG_NORMAL)("Port 64 write with val %" sBitfs(X), val);
	switch (val) {
	case 0xae:		/* Activate keyboard */
		keyb.active=true;
		if (keyb.used && !keyb.scheduled && !keyb.p60changed) {
			keyb.scheduled=true;
			PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
		}
		//LOG(LOG_KEYBOARD,LOG_NORMAL)("Activated");
		break;
	case 0xad:		/* Deactivate keyboard */
		keyb.active=false;
		//LOG(LOG_KEYBOARD,LOG_NORMAL)("De-Activated");
		break;
	case 0xd0:		/* Outport on buffer */
		KEYBOARD_SetPort60(MEM_A20_Enabled() ? 0x02 : 0x02);
		break;
	case 0xd1:		/* Write to outport */
		keyb.command=CMD_SETOUTPORT;
		break;
	default:
		LOG(LOG_KEYBOARD,LOG_ERROR)("Port 64 write with val %" sBitfs(X) ,val);
		break;
	}
}

static Bitu read_p64(Bitu /*port*/,Bitu /*iolen*/) {
	Bit8u status = 0x1c | (keyb.p60changed ? 0x1 : 0x0);
	return status;
}

void KEYBOARD_AddKey(KBD_KEYS keytype, bool pressed) {
	Bit8u ret = 0;
	if (keyb.scancodeset == 0x8a) {//scancode set 8Ah for PS/55 Japanese DOS
		bool relbreak = false;//not all keys produce the break code
		switch (keytype) {
		case KBD_esc:ret = 0x3d; break;
		case KBD_1:ret = 0x24; break;
		case KBD_2:ret = 0x25; break;
		case KBD_3:ret = 0x26; break;
		case KBD_4:ret = 0x27; break;
		case KBD_5:ret = 0x28; break;
		case KBD_6:ret = 0x29; break;
		case KBD_7:ret = 0x2a; break;
		case KBD_8:ret = 0x2b; break;
		case KBD_9:ret = 0x2c; break;
		case KBD_0:ret = 0x2d; break;

		case KBD_minus:ret = 0x2e; break;
		case KBD_equals:ret = 0x2f; break;
		case KBD_backspace:ret = 0x3e; break;
		case KBD_tab:ret = 0x3c; break;

		case KBD_q:ret = 0x18; break;
		case KBD_w:ret = 0x19; break;
		case KBD_e:ret = 0x1a; break;
		case KBD_r:ret = 0x1b; break;
		case KBD_t:ret = 0x1c; break;
		case KBD_y:ret = 0x1d; break;
		case KBD_u:ret = 0x1e; break;
		case KBD_i:ret = 0x1f; break;
		case KBD_o:ret = 0x20; break;
		case KBD_p:ret = 0x21; break;

		case KBD_leftbracket:ret = 0x22; break;//@
		case KBD_rightbracket:ret = 0x23; break;//[
		case KBD_enter:ret = 0x3b; break;
		case KBD_leftctrl:relbreak = true; ret = 0x41; break;

		case KBD_a:ret = 0x0c; break;
		case KBD_s:ret = 0x0d; break;
		case KBD_d:ret = 0x0e; break;
		case KBD_f:ret = 0x0f; break;
		case KBD_g:ret = 0x10; break;
		case KBD_h:ret = 0x11; break;
		case KBD_j:ret = 0x12; break;
		case KBD_k:ret = 0x13; break;
		case KBD_l:ret = 0x14; break;

		case KBD_semicolon:ret = 0x15; break;//;
		case KBD_quote:ret = 0x16; break;//:
		case KBD_grave:ret = 0x17; break;//]
		case KBD_leftshift:relbreak = true; ret = 0x38; break;
		//case KBD_backslash:ret = 0x30; break;//2bh \|ｰ
		case KBD_z:ret = 0x01; break;
		case KBD_x:ret = 0x02; break;
		case KBD_c:ret = 0x03; break;
		case KBD_v:ret = 0x04; break;
		case KBD_b:ret = 0x05; break;
		case KBD_n:ret = 0x06; break;
		case KBD_m:ret = 0x07; break;

		case KBD_comma:ret = 0x08; break;
		case KBD_period:ret = 0x09; break;
		case KBD_slash:ret = 0x0a; break;
		case KBD_rightshift:relbreak = true; ret = 0x39; break;
		case KBD_kpmultiply:ret = 0x64; break;
		case KBD_leftalt:relbreak = true; ret = 0x3a; break;//漢字 (Kanji), same scancode with Left Alt
		case KBD_space:ret = 0x34; break;
		case KBD_capslock:relbreak = true; ret = 0x32; break;

		case KBD_f1:ret = 0x68; break;
		case KBD_f2:ret = 0x69; break;
		case KBD_f3:ret = 0x6a; break;
		case KBD_f4:ret = 0x6b; break;
		case KBD_f5:ret = 0x6c; break;
		case KBD_f6:ret = 0x6d; break;
		case KBD_f7:ret = 0x6e; break;
		case KBD_f8:ret = 0x6f; break;
		case KBD_f9:ret = 0x70; break;
		case KBD_f10:ret = 0x71; break;

		case KBD_kpcomma:ret = 0x66; break;//NumLk -> ,
		case KBD_scrolllock:ret = 0x75; break;//ScrLk -> NumLk/ScrLk

		case KBD_kp7:ret = 0x5d; break;
		case KBD_kp8:ret = 0x5e; break;
		case KBD_kp9:ret = 0x5f; break;
		case KBD_kpminus:ret = 0x67; break;
		case KBD_kp4:ret = 0x5a; break;
		case KBD_kp5:ret = 0x5b; break;
		case KBD_kp6:ret = 0x5c; break;
		case KBD_kpplus:ret = 0x63; break;
		case KBD_kp1:ret = 0x57; break;
		case KBD_kp2:ret = 0x58; break;
		case KBD_kp3:ret = 0x59; break;
		case KBD_kp0:ret = 0x55; break;
		case KBD_kpperiod:ret = 0x56; break;

		case KBD_extra_lt_gt:ret = 0x0b; break;//\|ﾛ underscore (JKB only)
		case KBD_f11:ret = 0x72; break;
		case KBD_f12:ret = 0x73; break;

			//The Extended keys

		case KBD_kpenter: ret = 0x60; break;
		case KBD_rightctrl:
		{
			relbreak = true;
			ret = 0x37;//0x1d
			break;
		}
		case KBD_kpdivide:ret = 0x65; break;
		case KBD_rightalt://前面 (Front)
		{
			relbreak = true;
			ret = 0x31;//0x38
			break;
		}
		//case KBD_rightalt: ret = 0xA7; break; ax
		case KBD_home:ret = 0x4c; break;
		case KBD_up:ret = 0x4e; break;
		case KBD_pageup:ret = 0x52; break;
		case KBD_left:ret = 0x4b; break;
		case KBD_right:ret = 0x4d; break;
		case KBD_end:ret = 0x53; break;
		case KBD_down:ret = 0x4a; break;
		case KBD_pagedown:ret = 0x54; break;
		case KBD_insert:ret = 0x4f; break;
		case KBD_delete:ret = 0x50; break;
		case KBD_pause:ret = 0x47; break;//126
		case KBD_printscreen: ret = 0x74; break;//124
		case KBD_backslash:ret = 0x0b; break;//for AX/JP layout
		case KBD_yen:ret = 0x30; break;//for AX/JP layout
		case KBD_conv:ret = 0x35; break;//for AX/JP layout
		case KBD_nconv:ret = 0x33; break;//for AX/JP layout
		case KBD_ax:ret = 0; break;//AX
		case KBD_hankaku:ret = 0x45; break;//for PS/55
		case KBD_kana:relbreak = true; ret = 0x36; break;//for PS/55

		default:
			E_Exit("Unsupported key press");
			break;
		}
		//5576-001 keyboard emulation enabled once by pressing L-Shift (↑) + L-Ctrl + L-Alt (前面)
		if (keyb.key5576special) {
			switch (keytype) {
			case KBD_f3:ret = 0x76; break;//Cr Bnk/領域呼出 (Call Range)/All Cr/登録 (Register)
			case KBD_f4:ret = 0x77; break;//割込み (Interrupt)
			case KBD_f5:ret = 0x78; break;//UF1
			case KBD_f6:ret = 0x79; break;//UF2
			case KBD_f7:ret = 0x7a; break;//UF3
			case KBD_f8:ret = 0x7b; break;//UF4
			case KBD_f9:ret = 0x7c; break;//EOF/Erase/ErInp
			case KBD_f10:ret = 0x7d; break;//Attn/ /CrSel
			case KBD_f11:ret = 0x7e; break;//PA1/ /DvCncl
			case KBD_f12:ret = 0x7f; break;//PA2/ /PA3
			case KBD_1:ret = 0x48; break;//Clear/ /SysRq
			case KBD_2:ret = 0x49; break;//終了 (Exit)
			case KBD_3:ret = 0x46; break;//メッセージ (Message)/ /応答 (Respond)
			case KBD_4:ret = 0x44; break;//サイズ変換 (Change Size)/ /横倍角 (2x Width)
			case KBD_5:ret = 0x42; break;//単語登録 (Register Word)/ /再交換 (Re-change)
			case KBD_6:ret = 0x43; break;//漢字 (Kanji)/ /番号 (Number)
			case KBD_7:ret = 0x40; break;//取消 (Cancel)
			case KBD_8:ret = 0x51; break;//コピー (Copy)/ /移動 (Move)
			default: break;
			}
		}
		/* Add the actual key in the keyboard queue */
		if (pressed)
		{
			if (keyb.repeat.key == keytype) keyb.repeat.wait = keyb.repeat.rate;
			else keyb.repeat.wait = keyb.repeat.pause;
			keyb.repeat.key = keytype;
			switch (keytype) {//save pressed status for 5576-001 emulation
			case KBD_leftshift:keyb.lShiftPressed = true; break;
			case KBD_leftctrl:keyb.lCtrlPressed = true; break;
			case KBD_rightalt:keyb.rAltPressed = true; break;
			default: 
				if (keyb.key5576special)
				{
					LOG(LOG_KEYBOARD, LOG_NORMAL)("5576-001 key emulation disabled.");
					keyb.key5576special = false;//disable emulation when any other key is pressed
				}
				break;
			}
			if (keyb.lShiftPressed && keyb.lCtrlPressed && keyb.rAltPressed) {
				keyb.key5576special = true;
				LOG(LOG_KEYBOARD, LOG_NORMAL)("5576-001 key emulation enabled.");
			}
		}
		else
		{
			if (keyb.repeat.key == keytype) {
				/* repeated key being released */
				keyb.repeat.key = KBD_NONE;
				keyb.repeat.wait = 0;
			}
			if (relbreak == true) //only a few keys send the break code
			{
				ret += 128;
				switch (keytype) {//save release status for 5576-001 emulation
				case KBD_leftshift:keyb.lShiftPressed = false; break;
				case KBD_leftctrl:keyb.lCtrlPressed = false; break;
				case KBD_rightalt:keyb.rAltPressed = false; break;
				}
			}
			else
				return;//most keys send nothing when the key is released
		}
	}
	else if (IS_PS55_ARCH)
	{
		bool extend = false;
		switch (keytype) {
		case KBD_esc:ret = 1; break;
		case KBD_1:ret = 2; break;
		case KBD_2:ret = 3; break;
		case KBD_3:ret = 4; break;
		case KBD_4:ret = 5; break;
		case KBD_5:ret = 6; break;
		case KBD_6:ret = 7; break;
		case KBD_7:ret = 8; break;
		case KBD_8:ret = 9; break;
		case KBD_9:ret = 10; break;
		case KBD_0:ret = 11; break;

		case KBD_minus:ret = 12; break;
		case KBD_equals:ret = 13; break;
		case KBD_backspace:ret = 14; break;
		case KBD_tab:ret = 15; break;

		case KBD_q:ret = 16; break;
		case KBD_w:ret = 17; break;
		case KBD_e:ret = 18; break;
		case KBD_r:ret = 19; break;
		case KBD_t:ret = 20; break;
		case KBD_y:ret = 21; break;
		case KBD_u:ret = 22; break;
		case KBD_i:ret = 23; break;
		case KBD_o:ret = 24; break;
		case KBD_p:ret = 25; break;

		case KBD_leftbracket:ret = 26; break;
		case KBD_rightbracket:ret = 27; break;
		case KBD_enter:ret = 28; break;
		case KBD_leftctrl:ret = 29; break;

		case KBD_a:ret = 30; break;
		case KBD_s:ret = 31; break;
		case KBD_d:ret = 32; break;
		case KBD_f:ret = 33; break;
		case KBD_g:ret = 34; break;
		case KBD_h:ret = 35; break;
		case KBD_j:ret = 36; break;
		case KBD_k:ret = 37; break;
		case KBD_l:ret = 38; break;

		case KBD_semicolon:ret = 39; break;
		case KBD_quote:ret = 40; break;
		case KBD_grave:ret = 41; break;//29h `~ﾑ｣
		case KBD_leftshift:ret = 42; break;
			//case KBD_backslash:ret = 43; break;//2bh \|ｰ
		case KBD_z:ret = 44; break;
		case KBD_x:ret = 45; break;
		case KBD_c:ret = 46; break;
		case KBD_v:ret = 47; break;
		case KBD_b:ret = 48; break;
		case KBD_n:ret = 49; break;
		case KBD_m:ret = 50; break;

		case KBD_comma:ret = 51; break;
		case KBD_period:ret = 52; break;
		case KBD_slash:ret = 53; break;
		case KBD_rightshift:ret = 54; break;
		case KBD_kpmultiply:ret = 55; break;
		case KBD_leftalt:ret = (keyb.scancodeset == 0x81) ? 0x70 : 56; break;//for PS/55
		case KBD_space:ret = 57; break;
		case KBD_capslock:ret = 58; break;

		case KBD_f1:ret = 59; break;
		case KBD_f2:ret = 60; break;
		case KBD_f3:ret = 61; break;
		case KBD_f4:ret = 62; break;
		case KBD_f5:ret = 63; break;
		case KBD_f6:ret = 64; break;
		case KBD_f7:ret = 65; break;
		case KBD_f8:ret = 66; break;
		case KBD_f9:ret = 67; break;
		case KBD_f10:ret = 68; break;

		case KBD_numlock:ret = 69; break;
		case KBD_scrolllock:ret = 70; break;

		case KBD_kp7:ret = 71; break;
		case KBD_kp8:ret = 72; break;
		case KBD_kp9:ret = 73; break;
		case KBD_kpminus:ret = 74; break;
		case KBD_kp4:ret = 75; break;
		case KBD_kp5:ret = 76; break;
		case KBD_kp6:ret = 77; break;
		case KBD_kpplus:ret = 78; break;
		case KBD_kp1:ret = 79; break;
		case KBD_kp2:ret = 80; break;
		case KBD_kp3:ret = 81; break;
		case KBD_kp0:ret = 82; break;
		case KBD_kpperiod:ret = 83; break;

		case KBD_extra_lt_gt:ret = 86; break;//56h \|ﾛ underscore
		case KBD_f11:ret = 87; break;
		case KBD_f12:ret = 88; break;

			//The Extended keys

		case KBD_kpenter:extend = true; ret = 28; break;
		case KBD_rightctrl:
		{
			extend = true;
			ret = 29;//0x1d
			break;
		}
		case KBD_kpdivide:extend = true; ret = 53; break;
		case KBD_rightalt:
		{
			if (keyb.scancodeset != 0x81) extend = true;
			ret = 56;//0x38
			break;
		}
		//case KBD_rightalt: ret = 0xA7; break; ax
		case KBD_home:extend = true; ret = 71; break;
		case KBD_up:extend = true; ret = 72; break;
		case KBD_pageup:extend = true; ret = 73; break;
		case KBD_left:extend = true; ret = 75; break;
		case KBD_right:extend = true; ret = 77; break;
		case KBD_end:extend = true; ret = 79; break;
		case KBD_down:extend = true; ret = 80; break;
		case KBD_pagedown:extend = true; ret = 81; break;
		case KBD_insert:extend = true; ret = 82; break;
		case KBD_delete:extend = true; ret = 83; break;
		case KBD_pause:
			KEYBOARD_AddBuffer(0xe1);
			KEYBOARD_AddBuffer(29 | (pressed ? 0 : 0x80));
			KEYBOARD_AddBuffer(69 | (pressed ? 0 : 0x80));
			return;
		case KBD_printscreen:
			KEYBOARD_AddBuffer(0xe0);
			KEYBOARD_AddBuffer(42 | (pressed ? 0 : 0x80));
			KEYBOARD_AddBuffer(0xe0);
			KEYBOARD_AddBuffer(55 | (pressed ? 0 : 0x80));
			return;
			//case KBD_underscore:ret = 0x73; break;//for JP layout
			//case KBD_yen:ret = 0x7d; break;//for JP layout
		case KBD_backslash:ret = 86; break;//for AX/JP layout
		case KBD_yen:ret = 43; break;//for AX/JP layout
		case KBD_conv:ret = (keyb.scancodeset == 0x81) ? 0x79 : 57; break;
		case KBD_nconv:ret = (keyb.scancodeset == 0x81) ? 0x7b : 57; break;
		case KBD_ax:ret = 0; break;//AX
		case KBD_hankaku:ret = (keyb.scancodeset == 0x81) ? 0x77 : 127; break;//for PS/55
		case KBD_kana:
			if (keyb.scancodeset == 0x81) {
				extend = true; ret = 0x38;
			}
			else
				ret = 57;
			break;
		case KBD_kpcomma:
			if (keyb.scancodeset == 0x81) {
				extend = true; ret = 51;//scancode set 81 for PS/55
			}
			else
				ret = 0;//scancode set 01 for PS/55
			break;
		default:
			E_Exit("Unsupported key press");
			break;
		}
		/* Add the actual key in the keyboard queue */
		if (pressed) {
			if (keyb.repeat.key == keytype) keyb.repeat.wait = keyb.repeat.rate;
			else keyb.repeat.wait = keyb.repeat.pause;
			keyb.repeat.key = keytype;
		}
		else {
			if (keyb.repeat.key == keytype) {
				/* repeated key being released */
				keyb.repeat.key = KBD_NONE;
				keyb.repeat.wait = 0;
			}
			ret += 128;
		}
		if (extend) KEYBOARD_AddBuffer(0xe0);
	}
	else
	{
		bool extend = false;
		switch (keytype) {
		case KBD_esc:ret = 1; break;
		case KBD_1:ret = 2; break;
		case KBD_2:ret = 3; break;
		case KBD_3:ret = 4; break;
		case KBD_4:ret = 5; break;
		case KBD_5:ret = 6; break;
		case KBD_6:ret = 7; break;
		case KBD_7:ret = 8; break;
		case KBD_8:ret = 9; break;
		case KBD_9:ret = 10; break;
		case KBD_0:ret = 11; break;

		case KBD_minus:ret = 12; break;
		case KBD_equals:ret = 13; break;
		case KBD_backspace:ret = 14; break;
		case KBD_tab:ret = 15; break;

		case KBD_q:ret = 16; break;
		case KBD_w:ret = 17; break;
		case KBD_e:ret = 18; break;
		case KBD_r:ret = 19; break;
		case KBD_t:ret = 20; break;
		case KBD_y:ret = 21; break;
		case KBD_u:ret = 22; break;
		case KBD_i:ret = 23; break;
		case KBD_o:ret = 24; break;
		case KBD_p:ret = 25; break;

		case KBD_leftbracket:ret = 26; break;
		case KBD_rightbracket:ret = 27; break;
		case KBD_enter:ret = 28; break;
		case KBD_leftctrl:ret = 29; break;

		case KBD_a:ret = 30; break;
		case KBD_s:ret = 31; break;
		case KBD_d:ret = 32; break;
		case KBD_f:ret = 33; break;
		case KBD_g:ret = 34; break;
		case KBD_h:ret = 35; break;
		case KBD_j:ret = 36; break;
		case KBD_k:ret = 37; break;
		case KBD_l:ret = 38; break;

		case KBD_semicolon:ret = 39; break;
		case KBD_quote:ret = 40; break;
		case KBD_grave:ret = 41; break;//29h `~ﾑ｣
		case KBD_leftshift:ret = 42; break;
		//case KBD_backslash:ret = 43; break;//2bh \|ｰ
		case KBD_z:ret = 44; break;
		case KBD_x:ret = 45; break;
		case KBD_c:ret = 46; break;
		case KBD_v:ret = 47; break;
		case KBD_b:ret = 48; break;
		case KBD_n:ret = 49; break;
		case KBD_m:ret = 50; break;

		case KBD_comma:ret = 51; break;
		case KBD_period:ret = 52; break;
		case KBD_slash:ret = 53; break;
		case KBD_rightshift:ret = 54; break;
		case KBD_kpmultiply:ret = 55; break;
		case KBD_leftalt:ret = 56; break;
		case KBD_space:ret = 57; break;
		case KBD_capslock:ret = 58; break;

		case KBD_f1:ret = 59; break;
		case KBD_f2:ret = 60; break;
		case KBD_f3:ret = 61; break;
		case KBD_f4:ret = 62; break;
		case KBD_f5:ret = 63; break;
		case KBD_f6:ret = 64; break;
		case KBD_f7:ret = 65; break;
		case KBD_f8:ret = 66; break;
		case KBD_f9:ret = 67; break;
		case KBD_f10:ret = 68; break;

		case KBD_numlock:ret = 69; break;
		case KBD_scrolllock:ret = 70; break;

		case KBD_kp7:ret = 71; break;
		case KBD_kp8:ret = 72; break;
		case KBD_kp9:ret = 73; break;
		case KBD_kpminus:ret = 74; break;
		case KBD_kp4:ret = 75; break;
		case KBD_kp5:ret = 76; break;
		case KBD_kp6:ret = 77; break;
		case KBD_kpplus:ret = 78; break;
		case KBD_kp1:ret = 79; break;
		case KBD_kp2:ret = 80; break;
		case KBD_kp3:ret = 81; break;
		case KBD_kp0:ret = 82; break;
		case KBD_kpperiod:ret = 83; break;

		case KBD_extra_lt_gt:ret = 86; break;//56h \|ﾛ underscore
		case KBD_f11:ret = 87; break;
		case KBD_f12:ret = 88; break;

			//The Extended keys

		case KBD_kpenter:extend = true; ret = 28; break;
		case KBD_rightctrl:
		{
			extend = true;
			ret = 29;//0x1d
			break;
		}
		case KBD_kpdivide:extend = true; ret = 53; break;
		case KBD_rightalt:
		{
			extend = true;
			ret = 56;//0x38
			break;
		}
		//case KBD_rightalt: ret = 0xA7; break; ax
		case KBD_home:extend = true; ret = 71; break;
		case KBD_up:extend = true; ret = 72; break;
		case KBD_pageup:extend = true; ret = 73; break;
		case KBD_left:extend = true; ret = 75; break;
		case KBD_right:extend = true; ret = 77; break;
		case KBD_end:extend = true; ret = 79; break;
		case KBD_down:extend = true; ret = 80; break;
		case KBD_pagedown:extend = true; ret = 81; break;
		case KBD_insert:extend = true; ret = 82; break;
		case KBD_delete:extend = true; ret = 83; break;
		case KBD_pause:
			KEYBOARD_AddBuffer(0xe1);
			KEYBOARD_AddBuffer(29 | (pressed ? 0 : 0x80));
			KEYBOARD_AddBuffer(69 | (pressed ? 0 : 0x80));
			return;
		case KBD_printscreen:
			KEYBOARD_AddBuffer(0xe0);
			KEYBOARD_AddBuffer(42 | (pressed ? 0 : 0x80));
			KEYBOARD_AddBuffer(0xe0);
			KEYBOARD_AddBuffer(55 | (pressed ? 0 : 0x80));
			return;
			//case KBD_underscore:ret = 0x73; break;//for JP layout
			//case KBD_yen:ret = 0x7d; break;//for JP layout
		case KBD_backslash:ret = 86; break;//for AX/JP layout
		case KBD_yen:ret = 43; break;//for AX/JP layout
		case KBD_conv:ret = (INT16_AX_GetKBDBIOSMode() == 0x51) ? 0x5b : 57; break;//for AX/JP layout
		case KBD_nconv:ret = (INT16_AX_GetKBDBIOSMode() == 0x51) ? 0x5a : 57; break;//for AX/JP layout
		case KBD_ax:ret = (INT16_AX_GetKBDBIOSMode() == 0x51) ? 0x5c : 0; break;//for AX layout
			/* System Scan Code for AX keys
			JP mode  Make    Break    US mode  Make    Break
			無変換   5Ah     DAh      Space    39h(57) B9h
			変換     5Bh     DBh      Space    39h(57) B9h
			漢字     E0h-38h E0h-B8h  RAlt     (status flag)
			AX       5Ch     DCh      (unused)
			*/
			/* Character code in JP mode (implemented in KBD BIOS)
			Key         Ch Sh Ct Al
			5A 無変換 - AB AC AD AE
			5B 変換   - A7 A8 A9 AA
			38 漢字   - 3A 3A
			5c AX     - D2 D3 D4 D5
			*/
		case KBD_hankaku:ret = 127; break;//scancode set 01 for PS/55
		case KBD_kana: ret = 57; break;//scancode set 01 for PS/55
		case KBD_kpcomma: ret = 0; break;//scancode set 01 for PS/55
		default:
			E_Exit("Unsupported key press");
			break;
		}
		/* Add the actual key in the keyboard queue */
		if (pressed) {
			if (keyb.repeat.key == keytype) keyb.repeat.wait = keyb.repeat.rate;
			else keyb.repeat.wait = keyb.repeat.pause;
			keyb.repeat.key = keytype;
		}
		else {
			if (keyb.repeat.key == keytype) {
				/* repeated key being released */
				keyb.repeat.key = KBD_NONE;
				keyb.repeat.wait = 0;
			}
			ret += 128;
		}
		if (extend) KEYBOARD_AddBuffer(0xe0);
	}
	KEYBOARD_AddBuffer(ret);
#if C_HEAVY_DEBUG
	LOG(LOG_KEYBOARD, LOG_NORMAL)("buffer added: %02X", ret);
#endif
}

static void KEYBOARD_TickHandler(void) {
	if (keyb.repeat.wait) {
		keyb.repeat.wait--;
		if (!keyb.repeat.wait) KEYBOARD_AddKey(keyb.repeat.key,true);
	}
}

void KEYBOARD_Init(Section* /*sec*/) {
	IO_RegisterWriteHandler(0x60,write_p60,IO_MB);
	IO_RegisterReadHandler(0x60,read_p60,IO_MB);
	IO_RegisterWriteHandler(0x61,write_p61,IO_MB);
	IO_RegisterReadHandler(0x61,read_p61,IO_MB);
	if (machine == MCH_CGA || machine == MCH_HERC) IO_RegisterReadHandler(0x62,read_p62,IO_MB);
	IO_RegisterWriteHandler(0x64,write_p64,IO_MB);
	IO_RegisterReadHandler(0x64,read_p64,IO_MB);
	TIMER_AddTickHandler(&KEYBOARD_TickHandler);
	write_p61(0,0,0);
	/* Init the keyb struct */
	keyb.active = true;
	keyb.scanning = true;
	keyb.command = CMD_NONE;
	keyb.p60changed = false;
	keyb.repeat.key = KBD_NONE;
	keyb.repeat.pause = 500;
	keyb.repeat.rate = 33;
	keyb.repeat.wait = 0;
	KEYBOARD_ClrBuffer();
}
