//Copyright (c) 2021 akm
//This content is under the MIT License.
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

void main(void){
	int x = 0;
    if(_setmode( _fileno( stdout ), _O_BINARY ) == -1){
	fprintf(stderr, "can't set stdin in binary mode \n");
	return;
    }
	printf("        0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F ");
	x=35;
	while(x<80){
		putchar(' ');
		putchar(0);
		x++;
	}
	x=0;
	for(int i=0;i<256;i++){
		if(i%16==0) {
			while(x<80){//fill space until the end of the line
				putchar(' ');
				putchar(0);
				x++;
			}
			x=0;
			putchar(' ');
			putchar(0);
			printf("%X", i >> 4);
			putchar(0);
			printf("%X", i & 0xF);
			putchar(0);
			putchar(' ');
			putchar(0x10);
			x+=4;
		}
		putchar('A');
		putchar(i);
		putchar(' ');
		putchar(0);
		x+=2;
	}
}