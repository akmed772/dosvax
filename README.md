DOSVAX
=======
You can obtain binary and source code from GitHub <https://github.com/akmed772/dosvax>.

### License

GNU GPLv2 if there is no statement in the document or source code. See also ABOUT_LIBS.

### About

This is an emulator that reproduces the AX specification personal computer commonly called the Japanese PC/AT compatible machine. It is a folk of DOSBox, and is possible to run IBM PC software and DOS/V, but the goal is to run applications developed for AX machines. Please prepare the Japanese MS-DOS for AX. The BIOS ROM is not required, but you need to make Japanese FONTX2 font files to display Japanese characters.

Also, it experimentally implements the PS/55 emulation equivalent to the IBM Japan PS/55 model 5550-S/T/V. (PS/55 was developed aside from the AX, so there is no compatibility in architectures.)

### Features (AX)

* Implement Japanese text / graphic functions equivalent to the AX-1 system specification
* Support Japanese-English mode switching (bilingual)
* Assign additional keys specific to the AX keyboard

### Unsupported / Limitations (AX)

* AX-2 extensions (text graphics superimposition)
* User-defined characters using bank switching
* Additional functions of the printer BIOS
* Incomplete implementation of Kana shift lock
* Incomplete support for Japanese 106 keyboard due to the limitation of the component (SDL 1.2) on which DOSBox depends.
* MOUSE.SYS of MS-DOS 3.21 cannot work because DOSBox doesn't support the bus mouse. Use MOUSE.COM of IBM DOS J4.0/V and later.

### System Requirements

* Windows PC
* Microsoft Windows 11 (maybe this works on Windows 7 or later)
* Microsoft Visual C++ 2019 Redistributable Package (x86)
* For AX emulation: 8x19 pixel Japanese SBCS font and 16x16 pixel DBCS font file in FONTX2 format
* For PS/55 emulation: 12x24 pixel Japanese SBCS font and 24x24 pixel DBCS font file in FONTX2 format

### How to use

Run dosbox.exe. Dosbox_debug.exe has a debug monitor feature, but the operation speed becomes a bit slower. Please refer to the DOSBox documentation for basic usage. The following explains extensions in DOSVAX.

#### Key assignments (AX)

You can change the key bindings with Ctrl + F1 while DOSBox is running. Some keys, such as the AX key, are not assigned by default.

```
Key         JP mode   EN mode
RAlt        漢字      RAlt
Menu        変換      Space
RCtrl       英数カナ  RCtrl
(No assign: AX, 無変換)
```

#### Additional configuration in dosbox.conf

```
[dosbox] section
jfontsbcs=     Path to 8x19 FONTX2 SBCS font file (AX)
jfontdbcs=     Path to 16x16 FONTX2 DBCS font file (AX)
jfontsbcs24=   Path to 12x24 FONTX2 SBCS font file (PS/55)
               or Path to 13x30 DOS K3.x SBCS font file ($SYSHN24.FNT)
jfontsbex24=   Path to 13x30 DOS K3.x Extended SBCS font file ($SYSEX24.FNT) (PS/55, option)
jfontdbcs24=   Path to 24x24 FONTX2 DBCS font file (PS/55)
jfont24rom=    Path to 24x24 Font ROM binary file (instead of above PS/55 font files)
machine=ega    Enable AX emulation, and start with the AX English mode.
machine=jega   Start with AX Japanese mode. It can display Japanese without the AX version of MS-DOS, but some applications do not work correctly.
machine=svga_ps55  Enable PS/55 emulation. Please read README_PS55.txt.
machine=svga_ps55mono  Enable PS/55 monochrome emulation.
machine= for other options, see the original DOSBox documentation.

[dos] section
keyboardlayout=jp If you use the Japanese 106/109 keyboard layout instead of the US English 101 keyboard layout, this will correctly assign the symbol keys. Host conversion, Kanji keys, etc. doesn't work.
```

### Version History
* Build 4483PS14 (2024/01/18)
  - Fix an issue that the PS/55 text mode initialization in the Video BIOS clear memory outside attribute buffer.
  - Added a BitBlt operation to run Windows 3.0 (IBMJ OEM) in the standard mode with the PS/55 high-resolution display driver. (this is buggy for Windows 3.1)
* Build 4483PS13 (2023/12/20)
  - Correct text blinking in the JEGA drawing function.
  - Add a new key bind that shows a dialog to mount a floppy image during the emulator is in operation. (experimental)
  - Add parameters for the 5.25-inch 640 KB DSQD format in the Disk Geometry List (this format can be recognized by IBM DOS K3.x only).
  - Fix an issue that FORMAT command of DOS K3.3 makes data of memory broken. This is because the Block Data Structure (BDS) corrupts when DOS initialization calls Int 13h, AH = 08h and the BH register is not cleared.
  - Fix an issue that some Int 13h functions return an ambiguous error code.
* Build 4483PS12 (2023/12/15)
  - Fix following issues to improve emulation in AX mode
    - EGA registers were not readable. Original EGA's registers are not readable, but Super EGA's are readable.
    - The bit 6 of RMOD2 (Select Blink or Intensity) register didn't work.
    - Scancodes of some JP specific keys were changed by the keyboard controller when the Keyboard BIOS was in the US mode. The entire operation should be done by the Keyboard BIOS.
    - Set FFh at D0000-EFFFFh to use it as either EMS memory or UMB.
    - Added a resolution to support a mouse correctly when the video mode is 52h or 53h (JEGA graphics mode).
  - Following changes to improve emulation in PS/55 mode
    - Set the segment address of Extended BIOS Data Area at the highest memory under 640k, and adjust size of DOSBox MCB.
    - Set the BIOS signature to run the BASIC interpreter of DOS J5.0x/V.
    - Add a key bind for NumLock.(But the real IBM 5576-00* keyboards bind NumLock to Shift + ScrollLock.)
    - Change video mode determination for color graphics mode in DOS K3.x.
  - Merge changes from DOSBox SVR r4481 to r4483.
* Build 4481PS11 (2023/12/04)
  - Add the PS/55 Monochrome Monitor mode (machine=svga_ps55mono).
  - Add Font ROM dump utility (getfps5.com) for PS/55 machines and its loader for the emulator.
* Build 4481PS10 (2023/11/28)
  - Add a patch program (dcbpatch.com) for DOS J4.0/J5.0 to run graphics applications built by IBM BASIC Interpreter or IBM BASIC Compiler. It changes a flag in the Display Configuration Block of DOS I/O workspace to disable IBM 5550's Graphics Support Functions that are not supported by DOSVAX.
  - Disable the input method (IME) to avoid disturbing keystrokes. (sdlmain.c)
* Build 4481PS09 (2023/01/31)
  - Fix an issue that JEGA initialization has not been called since the Build 4467PS01.
  - Merge changes from DOSBox SVR r4467 to r4481.
* Build 4467PS08 (2022/02/11)
  - Improve the text cursor drawing method.
  - Change the register latch operation of the Attribute Controller.
  - Ignore the unhandled exception in BIOS Int 17h (Printer).
* Build 4467PS07 (2022/01/16)
  - Generate 1/4 fonts used by DOS Bunsho Program.
* Build 4467PS06 (2022/01/13)
  - Add keyboard scancode set 81h used by DOS/V. (The default scancode set inherits the AT keyboard unlike the 5576-002.)
  - Generate half-width DBCS fonts that are used by DOS Bunsho Program. (some extra fonts still cannot display correctly)
  - Fix an issue that the keyboard didn't reset the command latch when receiving the system command F0h (Select Alternate Scan Codes).
  - Fix an issue that the emulator puts an error when the AX key is pressed in the PS/55 mode.
  - Fix an issue that EMM386 may not detect free memory.
* Build 4467PS05 (2022/01/10)
  - Fix the cursor color at the position where the reverse attribute is set.
* Build 4467PS04 (2021/12/31)
  - Partially support for DOS J4.0 XMAEM.SYS (80386 XMA Emulator). (For Japanese DOS K3.x users, do not use $BANK386.SYS. The emulator unexpectedly shuts down.)
  - Change the key binding setting (Since the specification of mapper.txt has changed, please overwrite the new file and setup the key binding again).
  - Fix the 5576-001 keyboard emulation trigger key to Left Shift + Left Ctrl + Right Alt (front) keys.
* Build 4467PS03 (2021/12/27)
  - Fix alignment and blanks when using FONTX2 12x24 (SBCS) font.
  - Enable to load PS/55 Extended Half-width Characters (not used in most applications, not required in DOS K3.x).
  - Fix a bug that the vertical ruled line breaks when attributes blinking, vertical ruled line and underline are specified at the same time (in PS/55 text mode).
* Build 4467PS02 (2021/12/25)
  - Partially support for PS/55 monochrome graphics and 16-color graphics modes (IBM 5550 Graphic Support Functions and BIOS are not implemented).
  - Improve videomode determination in PS/55 emulation (this is incomplete in DOS K3.3 and K3.4).
  - Add PS/55 key binding (Ctrl + F1).
* Build 4467PS01 (2021/12/17)
  - Add the emulation equivalent to IBMJ PS/55 model 5550-S/T/V. See README_PS55.txt for usage.
* For older version history, refer to README_ja.md (in Japanese).

Copyright (C) 2016-2023 akm.

=============================README.txt END=============================