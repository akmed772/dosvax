DOSVAX
=======

Source code is located on GitHub <https://github.com/akmed772/dosvax>.

You can obtain the binary file for Windows from my homepage at <http://radioc.web.fc2.com/>.

### License

GNU GPLv2 if there is no statement in the document or source code. See also ABOUT_LIBS.

### About

This is an emulator that reproduces the AX specification personal computer commonly called the Japanese PC/AT compatible machine. It is a folk of DOSBox, and is possible to run IBM PC software and DOS/V, but the goal is to run applications developed for AX machines. Please prepare the Japanese MS-DOS for AX. The BIOS ROM is not required, but you need to make Japanese FONTX2 font files to display Japanese characters.

Also, it experimentally implements the PS/55 text mode emulation equivalent to the IBM Japan PS/55 model 5550-S/T/V.

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
* MOUSE.SYS that comes with MS-DOS 3.21 cannot work because DOSBox doesn't support the bus mouse. Use MOUSE.COM that comes with IBM DOS J4.0/V and later.

### System Requirements

* Windows PC
* Microsoft Windows 11 (maybe this works on Windows 7 or later)
* Microsoft Visual C++ 2019 Redistributable Package (x86)
* For AX emulation: 8x19 pixel Japanese SBCS font and 16x16 pixel DBCS font file in FONTX2 format
* For PS/55 emulation: 12x24 pixel Japanese SBCS font and 24x24 pixel DBCS font file in FONTX2 format

### How to use

Run dosbox.exe. Dosbox_debug.exe has a debug monitor function, but the operation speed becomes a bit slower. Please refer to the DOSBox documentation for basic usage. The following describes focusing on extensions of DOSVAX.

#### Key assignments (AX)

You can change the key bindings with Ctrl + F1 while DOSBox is running. Some keys, such as the AX key, are not assigned by default.

```
Key         JP mode   EN mode
RAlt        ??????      RAlt
Menu        ??????      Space
RCtrl       ????????????  RCtrl
(No assign: AX, ?????????)
```

#### Additional configuration in dosbox.conf

```
[dosbox] section
jfontsbcs=     Path to 8x19 FONTX2 SBCS font file
jfontdbcs=     Path to 16x16 FONTX2 DBCS font file
jfontsbcs24=   Path to 12x24 FONTX2 SBCS font file
               or Path to 13x30 DOS K3.x SBCS font file ($SYSHN24.FNT)
jfontsbex24=   Path to 13x30 DOS K3.x Extended SBCS font file ($SYSEX24.FNT) (option)
jfontdbcs24=   Path to 24x24 FONTX2 DBCS font file
machine=ega    Enable AX emulation, and start with the AX English mode.
machine=jega   Start with AX Japanese mode. It can display Japanese without the AX version of MS-DOS, but some applications do not work correctly.
machine=svga_ps55  Enable PS/55 emulation. Please read README_PS55.txt.
machine= for other options, see the original DOSBox documentation.

[dos] section
keyboardlayout=jp If you use the Japanese 106/109 keyboard layout instead of the US English 101 keyboard layout, this will correctly assign the symbol keys. Host conversion, Kanji keys, etc. doesn't work.
```

### Version History

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
  - Support DOS J4.0 XMAEM.SYS (80386 XMA Emulator). (Do not use $BANK386.SYS. The emulator unexpectedly shut down.)
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

Copyright (C) 2016-2022 akm.

=============================README.txt END=============================