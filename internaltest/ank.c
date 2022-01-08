#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

void main(void){
    if(_setmode( _fileno( stdout ), _O_BINARY ) == -1){
	fprintf(stderr, "can't set stdin in binary mode \n");
	return;
    }
	printf("   0 1 2 3 4 5 6 7 8 9 A B C D E F");
	for(int i=0;i<256;i++){
		if(i%16==0) printf("\r\n%X0 ", i / 16);
		if ((i >= 0x81 && i <= 0x9f) || (i >= 0xe0)) printf("  ");
		else{
			switch (i) {
				case 0x07://
				case 0x08:
				case 0x09://
				case 0x0a:
				case 0x0c:
				case 0x0d:
				case 0x1a:
				case 0x1b://
					printf("  ");
				break;
				default:
					putchar(i);
					printf(" ");
				break;
			}
		}
	}
}