;For testing attribution in PS/55 text mode
;Copyright (c) 2021 akm
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
	;check the current videomode is 8 (mono) or E (color)
	mov	ax, 0x0f00
	int	0x10
	cmp	al, 0x08
	je	videomode_ok
	cmp	al, 0x0e
	je	videomode_ok
	jmp	err
videomode_ok:
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
	mov	di, 0xE000
	mov	es, di
	mov	di, Y_ALIGN		;=80*2*6
	mov	ax, rdata
	mov	si, ax
	mov	cx, [fsize]
	rep	movsb		;repeat cx times moving byte at DS:SI to ES:DI 
	pop	es
	;close a file
	mov	bx, [hndl]
	mov	ah, 0x3e	;DOS: close a file
	int	0x21
	
	xor	ax, ax
	jmp	exit

err:
	mov	al, 1
	jmp	exit
	
exit:
	mov	ah, 0x4c	;DOS: terminate with return code
	int	0x21

	section .data
Name_Binfile:	db	"ATTR.BIN",0

	section .bss
hndl:	resw	1
fsize:	resw	1
rdata:	resb	SIZE