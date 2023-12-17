;For testing attribution in AX (JEGA) text mode
;Copyright (c) 2023 akm
;This content is under the MIT License.

;directive for NASM
[BITS 16]
SIZE equ 4000
Y_ALIGN equ 800		;=80*2*5

	global start
	section .text
start:
	;set offset address for the COM program
	org	0x100
	mov	byte [attr_count], 0
	;backup the current register, and set EGA attribute
	mov	ah, 0xba
	call	crtc_read
	mov	byte [bak_rmod2], al
	and	al, 0x7f
	call	crtc_write
prtattr:
	;open ATTR.BIN
	xor	ax, ax
	mov	ah, 0x3d	;DOS: open an existing file
					;al=0 (read mode)
	mov	dx, Name_Binfile
	int	0x21
	jc	err	;jump if error
	mov	[hndl], ax
	
	;get size of the file
	xor	dx,dx		;CX:DX (offset address) = 0
	xor cx,cx
	mov	bx, [hndl]
	mov	ax, 0x4202	;seek to end of file
	int	0x21
	jc	err
	mov	[fsize], ax
	or	dx, dx		;file size must be <64K bytes
	jnz	err
	add	ax, Y_ALIGN
	jo	err
	cmp	ah, 0x10	;file size must be <4096 bytes
	jae	err
	mov	ax, 0x4200	;seek to start of file
	int	0x21
	
	;read data from the file
	mov	dx, rdata
	mov	cx, [fsize]	;size of read data
	mov	bx, [hndl]
	mov	ah, 0x3f	;DOS: read data from a file
	int	0x21
	jc	err
	
	;transfer data from buffer to video memory
	push	es
	mov	di, 0xB800
	mov	es, di
	mov	di, Y_ALIGN
	mov	ax, rdata
	mov	si, ax
	mov	cx, [fsize]
	rep	movsb		;repeat cx times moving bytes from DS:SI to ES:DI 
	pop	es
	;close a file
	mov	bx, [hndl]
	mov	ah, 0x3e	;DOS: close a file
	int	0x21
	
	mov	dx, Msg_Test1
	call	print
	;prompt press any key
	mov	ah, 1
	int	0x21
	
	;change from blink to intensity attribute
	mov	ah, 0xba
	mov	al, byte [bak_rmod2]
	or	al, 0x20
	call	crtc_write
	
	mov	dx, Msg_Test2
	call	print
	;prompt press any key
	mov	ah, 1
	int	0x21
	
	;set Basic JEGA attribute
	mov	ah, 0xba
	mov	al, byte [bak_rmod2]
	or	al, 0x80
	call	crtc_write
	
	mov	dx, Msg_Test3
	call	print
	;prompt press any key
	mov	ah, 1
	int	0x21
	
	;set Extended JEGA attribute
	mov	ah, 0xba
	mov	al, byte [bak_rmod2]
	or	al, 0xc0
	call	crtc_write
	
	mov	dx, Msg_Test4
	call	print
	;prompt press any key
	mov	ah, 1
	int	0x21
	
	mov	dx, Msg_Blank
	call	print
	xor	ax, ax
	jmp	exit

err:
	mov	al, 1
	jmp	exit

crtc_read:;[Args] ah = index; [Ret] al = value;
	push	dx
	push	es
	push	0x40
	pop	es
	mov	dx, [es:0x63]
	xchg	al, ah
	out	dx, al;write index
	inc	dx
	xchg	al, ah
	in	al, dx;read data
	pop	es
	pop	dx
	ret
crtc_write:;[Args] ah = index; al = value;
	push	dx
	push	es
	push	0x40
	pop	es
	mov	dx, [es:0x63]
	xchg	al, ah
	out	dx, al;write index
	inc	dx
	xchg	al, ah
	out	dx, al;write data
	pop	es
	pop	dx
	ret
print:;dx = address to the message
	push	ax
	push	ds
	mov	ax, cs
	mov	ds, ax
	mov	ah, 9
	int	0x21
	pop	ds
	pop	ax
	ret

exit:
	;restore register
	push	ax
	mov	ah, 0xba
	mov	al, byte [bak_rmod2]
	call	crtc_write
	pop	ax
	
	mov	ah, 0x4c	;DOS: terminate with return code
	int	0x21

	section .data
Name_Binfile:	db	"ATRX.BIN",0
Msg_Test1:	db	0x1b, "[sAttr Mode: EGA (Blink)    $"
Msg_Test2:	db	0x1b, "[uAttr Mode: EGA (Intensity)$"
Msg_Test3:	db	0x1b, "[uAttr Mode: JEGA Basic     $"
Msg_Test4:	db	0x1b, "[uAttr Mode: JEGA Extended  $"
Msg_Blank:	db	0x1b, "[u                          $"

	section .bss
hndl:	resw	1
fsize:	resw	1
bak_rmod2:	resb	1
attr_count:	resb	1
rdata:	resb	SIZE