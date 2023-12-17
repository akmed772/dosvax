//Copyright (c) 2021 akm
//This content is under the MIT License.
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

void main(void){
	int x = 0;
	char xbar[] = "                                                                    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F ";
    if(_setmode( _fileno( stdout ), _O_BINARY ) == -1){
	fprintf(stderr, "can't set stdout in binary mode \n");
	return;
    }
	printf(xbar);
	x=strlen(xbar) / 2;
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
			for(int j=0;j<30;j++){
				putchar(' ');
				putchar(7);
				x++;
			}
			printf("%X", i >> 4);
			putchar(7);
			printf("%X", i & 0xF);
			putchar(7);
			putchar(' ');
			putchar(0);
			x+=3;
		}
		putchar(0x83);
		putchar(i);
		putchar(0x41);
		putchar(i);
		x+=2;
	}
}