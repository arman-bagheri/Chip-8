#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <time.h>
#include <stdbool.h>

uint8_t memory[4 * 1024];


uint8_t str_to_byte(char* str){
	int i=0;
	uint8_t byte= 0;
	for(i=0; i<8; i++){
		if(str[i] == '1'){
			byte += (0x1 << (7 - i));
		}
	}
	return byte;
}

void my_load(){
	FILE* fptr;
	fptr = fopen("mytest.txt","r");
	if(fptr==NULL){
		perror("file not found");
		exit(1);
	}
	char buffer[10];
	int memory_counter = 0x0200;
	while(fgets(buffer,10,fptr)){
		buffer[strcspn(buffer, "\n")] = '\0';
		memory[memory_counter] = str_to_byte(buffer);
		memory_counter = (memory_counter + 1)&0x0fff;
	}
}



int main(){
	my_load();
	int i;
	for (i=0; i<20; i++){
		printf("%08b\n", memory[0x0200 + i]);
	}
	return 0;
}