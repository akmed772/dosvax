DOSVAX_PS55
Dedicated for IBM Enthusiasts.

 Copyright (c) 2022 akm
 This document is under the MIT License.

- MIT License -
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

- Infrequency Questions -

Q. How to use?
A. Prepare 12 x 24 and 24 x 24 pixel FONTX2 Japanese font files.
	Q. How to create it?
		1. You need to prepare DOS J5.02/V or later on a hard drive (or a virtual machine).
		To install 24-pixel fonts automatically:
			During the DOS/V installation, change the printer option from IBM 5575/5577 to ESC/P 24-dot English.
		To install 24-pixel fonts manually:
			Extract $JPNHN24.FNT and $JPNZN24.FNT from a setup disk of DOS, and put them into the root directory.
			Edit C:\CONFIG.SYS, and add the /24=ON switch to the DEVICE command for $FONT.SYS.
		2. Download [FONTX](http://www.hmsoft.co.jp/lepton/software/dosv/fontx.htm)
		3. Run MKXFNT.EXE in DOS, and it generates JPNHN24X.FNT and JPNZN24X.FNT.
	
	MS-DOS 6.2/V doesn't have a 24-pixel font. It is included in the MS-DOS 6.2/V Supplemental Disk that was sold directly from Microsoft Japan.

Open dosbox.conf, and set "machine=svga_ps55" and paths for font files in [dosbox] section.

The PS/55 Display Adapter is disabled when booting. If you don't use a bootable DOS disk, you can switch to the DBCS video mode by using the additional internal command "MODE".

	"MODE 0" : Enable the Display Adapter, and set the video mode 08h (Monochrome Character).
	"MODE 3" : Enable the Display Adapter, and set the video mode 0Eh (Color Character).

Q. Pressing some keys put a different character with the US keyboard.
A. Try key configuration (Ctrl + F1). However, the Japanese keyboard layout has a different key assign in its shift position, so you cannot fix it completely.

Q. What a blight green screen! Only the text cursor appears.
A. Make sure that font files and their paths in dosbox.conf are correct.

Q. The cursor doesn't blink in the video mode 8 or E.
A. This is default in Japanese DOS. Some applications may change its blinking speed.

Q. Some DBCS characters are displayed as a square symbol during the text is scrolling. Is this correct?
A. Yes. It's expected.

Q. I created font files from DOS/V. How do I remove space between line-drawing characters?
A. You can't. The 24-pixel SBCS font of DOS/V have 12 x 24 pixel font while PS/55 renders characters in a box of 13 x 29 pixels. However, if you have DOS K3.x, use $SYSHN24.FNT instead of JPNHN24X.FNT to fix it.

Q. The emulator shuts down when booting DOS K3.x.
A. You need disable $BANK386.SYS in CONFIG.SYS.

Q. A printer error occurs when booting DOS J4.0.
A. You need disable Disney Sound Source emulation to run DOS J4.0 and later. Check dosbox.conf.

Q. The install program of DOS J4.0 exits with an error.
A. Create a hard disk image with bximage (included in Bochs), and mount it before BOOT.

Q. It seems the EMM386 device driver prevents DOS from booting. Is this necessary for Japanese DOS or DOS/V to work? 
A. No. EMM386 (EMS memory) is used by the Japanese input method (IBMMKK).

Q. The DOSBox emulation hangs up or shuts down when an application is running.
A. Japanese DOS has many extra functions that are not supported by the English version of DOS and DOSBox. Try booting the Japanese DOS and run it. However, a few softwares use a graphics mode that is not supported by DOSVAX.

Q. What's the Japanese DOS? Is it different from Japanese DOS/V?
A. In this document, the Japanese DOS refers to IBM DOS K3.x, J.4.0x and J5.0x. The display driver of Japanese DOS is designed for IBM proprietary video card called the Display Adapter while DOS/V is designed to run on the generic VGA. Not only the video memory address, the Japanese DOS is very different from DOS/V, and most applications developed for Japanese DOS will not work on DOS/V. The opposite is the same too.

Q. I have DOS K3.x or Jx.x for PS/55, but it cannot run on the emulator.
A. There are some variants of the Display Adapter, but all adapters doesn't have the BIOS. Instead of the hardware coding, the Japanese IBM DOS provides BIOS functions, and absorbs differences in individual hardwares. This means there are various versions of Japanese DOS, and some of them won't run on the emulator. DOSVAX emulates the PS/55 Model 5550-S, T and V. Please check the system requirements of your operating system and software, and it must support above hardwares.

Q. Does this have compatibility with the Multistation 5550 (known as IBM 5550 outside Japan)?
A. I've never use the Multistation 5550, but I guess PS/55 Japanese DOS is designed to run 5550 applications. Low-level softwares such as operating systems may not run.

Q. Can this emulator run Windows 3.x?
A. No. Windows uses a graphics mode, but the emulator doesn't support it

Q. Can this emulator run OS/2 J1.x?
A. I don't know.

Q. Can this emulator run this software?
A. I don't know. Try it!

Q. Can you fix this bug?
A. Sorry, I don't want to continue development. Too lazy.

Q. Where can I find the reference diskette?
Q. I don't have the DOS, so please upload its disk image.
Q. My PS/55 computer is broken. Can you repair it?
A. I can't respond such requests.
