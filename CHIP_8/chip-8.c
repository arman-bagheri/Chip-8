#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <time.h>
#include <stdbool.h>

uint8_t memory[4 * 1024];
uint8_t V[16] = {0};


#define WIDTH 64
#define HEIGHT 32
#define SCALE 10  // Final window size = WIDTH*SCALE x HEIGHT*SCALE

uint8_t buffer[WIDTH * HEIGHT]; // 0 = black, 1 = white

// SDL-related globals
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;

//stack


typedef struct {
	uint16_t items[20];
	int i;
} Stack;

void init_stack(Stack* s){
	s->i = -1;
}

void push_stack(Stack* s, uint16_t x){
	if((s->i)==19){
		printf("stack overflow\n");
	}
	else {
		s->i++;
		s->items[s->i] = x;
	}
}
uint16_t pop_stack(Stack* s){
	if(s->i==-1){
		printf("Stack is empty\n");
	}
	else return s->items[s->i--];
}
void print_stack(Stack* s){
	if(s->i==-1) {
		printf("Stack is empty\n");
		return;
	}
	int counter = s->i;
	while(counter>=0){
		printf("%04x <--", s->items[counter]);
		counter--;
	}
	printf("||\n");
}

Stack routines;

time_t start_time;

uint16_t PC = 0x0200; //12 bits used
uint16_t I = 0;           //12 bits used

uint8_t delay_timer = 0;
int delay_timer_clock;
uint8_t sound_timer = 0;
int sound_timer_clock;
uint16_t opcode;

SDL_Event event;


uint8_t wait();

void load_program(){
	FILE* fptr;
	fptr = fopen("Tank.ch8","rb");
	if(fptr==NULL){
		perror("file not found");
		exit(1);
	}
	int byte = fgetc(fptr);
	int i = 0x0200; //memory index
	while(byte!=EOF){
		memory[i] = (uint8_t)byte;
		i++;
		byte = fgetc(fptr);
	}
	fclose(fptr);
	printf("%d bytes in the program\n", i - 0x200);
}

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

uint8_t keys[16] = {0}; //1 means pressed

void reset_keys(){
	int i;
	for(i=0; i<16; i++)
		keys[i] = 0;
}



void processor(){
		//fetch
	opcode = (memory[PC]<<8)|memory[PC+1];
	PC += 2; //will be changed on some instructions

	if(opcode==0x00e0){//clear screen
		int i;
		for(i=0; i<64*32; i++)
			buffer[i] = 0;
	}
	else if(opcode==0x00ee){//return
		if(routines.i==-1){
			printf("stack is empty\n");
			exit(1);
		}
		PC = pop_stack(&routines);
	}
	else if((opcode&0xf000)==0x1000){ //jump to NNN
		PC = opcode & 0x0fff;
	}
	else if((opcode&0xf000)==0x2000){ //subroutine at NNN
		push_stack(&routines, PC);
		PC = opcode & 0x0fff;
	}
	else if((opcode&0xf000)==0x3000){//3xnn skip if equal
		if((V[(opcode>>8)&0x000f])==((uint8_t)(opcode&0x00ff))){
			PC += 2;
		}
	}
	else if((opcode&0xf000)==0x4000){//4xnn skip if not equal
		if((V[(opcode>>8)&0x000f])!=((uint8_t)(opcode&0x00ff))){
			PC += 2;
		}
	}
	else if((opcode&0xf00f)==0x5000){//skip if vx is vy
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		if(V[x]==V[y]) PC += 2;
	}

	else if((opcode&0xf000)==0x6000){//6xnn store nn in vx
		int x = (opcode>>8) & 0x000f;
		V[x] = (uint8_t)(opcode&0x00ff);
	}

	else if((opcode&0xf000)==0x7000){//7xnn add nn to vx
		int x = (opcode>>8) & 0x000f;
		V[x] += (uint8_t)(opcode&0x00ff);
	}

	else if((opcode&0xf00f)==0x8000){//8xy0
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		V[x] = V[y];
	}
	else if((opcode&0xf00f)==0x8001){//8xy1
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		V[x] |= V[y];
	}
	else if((opcode&0xf00f)==0x8002){//8xy2
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		V[x] &= V[y];
	}
	else if((opcode&0xf00f)==0x8003){//8xy3
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		V[x] ^= V[y];
	}
	else if((opcode&0xf00f)==0x8004){//8xy4
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		V[x] += V[y];
		if(V[x]<V[y]){
			V[0xf] = 1;
		}
		else {
			V[0xf] = 0;
		}
	}
	else if((opcode&0xf00f)==0x8005){//8xy5
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		uint8_t carry;
		if(V[x]>=V[y]){
			carry = 1;
		}
		else {
			carry = 0;
		}
		V[x] = V[x] - V[y];
		V[0xf] = carry;
	}
	else if((opcode&0xf00f)==0x8006){//8xy6
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		uint8_t carry;
		if(V[y]&0x01){
			carry = 1;
		}
		else {
			carry = 0;
		}
		V[x] = V[y]>>1;
		V[0xf] = carry;
	}
	else if((opcode&0xf00f)==0x8007){//8xy7
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		uint8_t carry;
		if(V[y]>=V[x]){
			carry = 1;
		}
		else {
			carry = 0;
		}
		V[x] = V[y] - V[x];
		V[0xf] = carry;
	}
	else if((opcode&0xf00f)==0x800e){//8xye
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		uint8_t carry;
		if(V[y]&0x80){
			carry = 1;
		}
		else {
			carry = 0;
		}
		V[x] = V[y]<<1;
		V[0xf] = carry;
	}
	else if((opcode&0xf00f)==0x9000){ //skip if not equal
		int x = (opcode>>8) & 0x000f;
		int y = (opcode>>4) & 0x000f;
		if(V[x]!=V[y]) PC += 2;
	}	
	else if((opcode&0xf000)==0xa000){
		I = opcode & 0x0fff;
	}

	else if((opcode&0xf000)==0xb000){
		PC = ((opcode&0x0fff) + V[0])&0x0fff;
	}
	else if((opcode&0xf000)==0xc000){ //random
		int x = (opcode>>8) & 0x000f;
		uint8_t kk = opcode & 0x00ff;
		V[x] = (rand() % 256) & kk;
	}
	else if((opcode&0xf000)==0xd000){ //draw
		V[0xf] = 0;
		int height = opcode & 0x000f;
		if(height==0){
		
		}
		else {
			int x = (opcode>>8) & 0x000f;
			int y = (opcode>>4) & 0x000f;
			int i;
			int j;
			int pixel_num;
			int line_pixel_num = V[x]%64;
			int line_num = V[y]%32;
			uint8_t byte;
			for(i=0; i<height; i++){
				byte = memory[(I + i)&0x0fff];
				for(j=0; j<8; j++){
					pixel_num = 64 * line_num + (line_pixel_num + j)%64;
					if((buffer[pixel_num]==1)&&(((byte>>(7-j))&0x01)==1)){
						V[0xf] = 1;
					}
					buffer[pixel_num] ^= (byte>>(7-j))&0x01;
				}
				line_num = (line_num+1)%32;
			}

		}
		printf("%x\n", V[0xf]);
	}

	else if((opcode&0xf0ff)==0xe09e){ //skip if keys pressed
		int x = (opcode>>8) & 0x000f;
		int key = V[x] &0x0f;
		if(keys[key])
			PC += 2;
	}
	else if((opcode&0xf0ff)==0xe0a1){ //skip if keys not pressed
		int x = (opcode>>8) & 0x000f;
		int key = V[x] &0x0f;
		if(keys[key]==0)
			PC += 2;
	}

	else if((opcode&0xf0ff)==0xf007){ //delay timer
		int x = (opcode>>8) & 0x000f;
		V[x] = delay_timer;
	}

	else if((opcode&0xf0ff)==0xf00a){ //wait key pressed
		int x = (opcode>>8) & 0x000f;
		V[x] = wait(); //a state much like the normal state, but the cpu does not execute or fetch
	}
	else if((opcode&0xf0ff)==0xf015){ //vx in delay timer
		int x = (opcode>>8) & 0x000f;
		delay_timer = V[x];
	}
	else if((opcode&0xf0ff)==0xf018){ //vx in sound timer
		int x = (opcode>>8) & 0x000f;
		sound_timer = V[x];
	}
	else if((opcode&0xf0ff)==0xf01e){ // I += Vx
		int x = (opcode>>8) & 0x000f;
		I += V[x];
	}
	else if((opcode&0xf0ff)==0xf029){ //sprite select
		int x = (opcode>>8) & 0x000f;
		I = 5 * (V[x]&0x0f);
	}

	else if((opcode&0xf0ff)==0xf033){ //vx bcd to memory
		int x = (opcode>>8) & 0x000f;
		uint16_t number = V[x];
		memory[I + 2] = number%10;
		number  = number / 10;

		memory[I + 1] = number%10;
		number  = number / 10;
		
		memory[I] = number%10;
	}

	else if((opcode&0xf0ff)==0xf055){ //registers 0 to x to memory
		int x = (opcode>>8) & 0x000f;
		int i=0;
		for(i=0; i<=x; i++){
			memory[(I+i)&0x0fff] = V[i];
		}
	}

	else if((opcode&0xf0ff)==0xf065){ //memory to registers
		int x = (opcode>>8) & 0x000f;
		int i=0;
		for(i=0; i<=x; i++){
			V[i] = memory[(I+i)&0x0fff];
		}
	}
	else {
		printf("undefined instruction\n");
		printf("PC   %x\n", PC);
		printf("OPCODE  %x\n", opcode);

	}

}

uint8_t chip8_fontset[80] = {
    // 0
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    // 1
    0x20, 0x60, 0x20, 0x20, 0x70,
    // 2
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    // 3
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    // 4
    0x90, 0x90, 0xF0, 0x10, 0x10,
    // 5
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    // 6
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    // 7
    0xF0, 0x10, 0x20, 0x40, 0x40,
    // 8
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    // 9
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    // A
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    // B
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    // C
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    // D
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    // E
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    // F
    0xF0, 0x80, 0xF0, 0x80, 0x80
};

uint32_t get_current_time_ms() {
    return SDL_GetTicks(); // milliseconds since SDL init
}


uint32_t last_timer_tick;


void update_timers() {
	uint32_t now;
    now = get_current_time_ms();
    uint32_t delta = now - last_timer_tick;

    if(delta>=20){
    	if (delay_timer > 0) delay_timer--;
        if (sound_timer > 0) sound_timer--;
        last_timer_tick = now;
    }         
}



// Initialize SDL window, renderer, and texture
bool init_display() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("Pixel Buffer",
              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
              WIDTH * SCALE, HEIGHT * SCALE, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
        return 1;
    }

    texture = SDL_CreateTexture(renderer,
               SDL_PIXELFORMAT_RGBA8888,
               SDL_TEXTUREACCESS_STREAMING,
               WIDTH, HEIGHT);
    if (!texture) {
        SDL_Log("SDL_CreateTexture Error: %s", SDL_GetError());
        return 1;
    }

    return 0;
}

void display() {
    uint32_t* pixels;
    int pitch;

    SDL_PixelFormat* fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
	uint32_t white = SDL_MapRGBA(fmt, 255, 255, 255, 255); // R, G, B, A
	uint32_t black = SDL_MapRGBA(fmt, 0, 0, 0, 255);

    if (SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch) != 0) {
        SDL_Log("SDL_LockTexture Error: %s", SDL_GetError());
        return;
    }

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            uint8_t val = buffer[y * WIDTH + x];
            uint32_t color = val ? white : black;  // white or black
            pixels[y * (pitch / 4) + x] = color;
        }
    }

    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


uint8_t wait(){
	while(1){
		//if key pressed return to normal state
		while (SDL_PollEvent(&event)) {
     	   if (event.type == SDL_QUIT) exit(0);
     	   if (event.type == SDL_KEYDOWN){
     	   		SDL_Keycode key = event.key.keysym.sym;
     	   		if(key == SDLK_1){
     	   			return 0x01;
     	   		}
     	   		else if(key == SDLK_2){
     	   			return 0x02;
     	   		}
     	   		else if(key == SDLK_3){
     	   			return 0x03;
     	   		}
     	   		else if(key == SDLK_4){
     	   			return 0x0c;
     	   		}
     	   		else if(key == SDLK_q){
     	   			return 0x04;
     	   		}
     	   		else if(key == SDLK_w){
     	   			return 0x05;
     	   		}
     	   		else if(key == SDLK_e){
     	   			return 0x06;
     	   		}
     	   		else if(key == SDLK_r){
     	   			return 0x0d;
     	   		}
     	   		else if(key == SDLK_a){
     	   			return 0x07;
     	   		}
     	   		else if(key == SDLK_s){
     	   			return 0x08;
     	   		}
     	   		else if(key == SDLK_d){
     	   			return 0x09;
     	   		}
     	   		else if(key == SDLK_f){
     	   			return 0x0e;
     	   		}
     	   		else if(key == SDLK_z){
     	   			return 0x0a;
     	   		}
     	   		else if(key == SDLK_x){
     	   			return 0x00;
     	   		}
     	   		else if(key == SDLK_c){
     	   			return 0x0b;
     	   		}
     	   		else if(key == SDLK_v){
     	   			return 0x0f;
     	   		}
     	   }
    	}
		update_timers();
		display();
	}
}

uint32_t last_cpu_tick;

void cpu_tick(){
	uint32_t now = get_current_time_ms();
	uint32_t delta = now - last_cpu_tick;
	if(delta>=2){
		processor();
		last_cpu_tick = now;
	}
}

int main(){
	//initializing
	srand(time(NULL));
	//my_load();
	load_program();
	if(init_display()==1){
		printf("couldn't initialize graphics\n");
		exit(1);
	}
	init_stack(&routines);
	int i=0;
	// int beep_playing;
	for(i=0; i<=0xf; i++) V[i] = 0;
	for (int i = 0; i < 80; i++) {
    	memory[i] = chip8_fontset[i];
	}

	delay_timer = 0x00;
	sound_timer = 0x00;
	reset_keys();

	//display test
	// for(i=0; i<64*32; i++){
	// 	buffer[i] = i%2;
	// }

	last_timer_tick = get_current_time_ms();
	last_cpu_tick = get_current_time_ms();

	while (1){
		while (SDL_PollEvent(&event)) {
     	   if (event.type == SDL_QUIT) exit(0);

     	   if (event.type == SDL_KEYDOWN){
     	   		SDL_Keycode key = event.key.keysym.sym;
     	   		if(key == SDLK_1){
     	   			keys[0x1] = 1;
     	   		}
     	   		if(key == SDLK_2){
     	   			keys[0x2] = 1;
     	   		}
     	   		if(key == SDLK_3){
     	   			keys[0x3] = 1;
     	   		}
     	   		if(key == SDLK_4){
     	   			keys[0xc] = 1;
     	   		}
     	   		if(key == SDLK_q){
     	   			keys[0x4] = 1;
     	   		}
     	   		if(key == SDLK_w){
     	   			keys[0x5] = 1;
     	   		}
     	   		if(key == SDLK_e){
     	   			keys[0x6] = 1;
     	   		}
     	   		if(key == SDLK_r){
     	   			keys[0xd] = 1;
     	   		}
     	   		if(key == SDLK_a){
     	   			keys[0x7] = 1;
     	   		}
     	   		if(key == SDLK_s){
     	   			keys[0x8] = 1;
     	   		}
     	   		if(key == SDLK_d){
     	   			keys[0x9] = 1;
     	   		}
     	   		if(key == SDLK_f){
     	   			keys[0xe] = 1;
     	   		}
     	   		if(key == SDLK_z){
     	   			keys[0xa] = 1;
     	   		}
     	   		if(key == SDLK_x){
     	   			keys[0x0] = 1;
     	   		}
     	   		if(key == SDLK_c){
     	   			keys[0xb] = 1;
     	   		}
     	   		if(key == SDLK_v){
     	   			keys[0xf] = 1;
     	   		}
     	   }

     	   if (event.type == SDL_KEYUP){
     	   		SDL_Keycode key = event.key.keysym.sym;
     	   		if(key == SDLK_1){
     	   			keys[0x1] = 0;
     	   		}
     	   		if(key == SDLK_2){
     	   			keys[0x2] = 0;
     	   		}
     	   		if(key == SDLK_3){
     	   			keys[0x3] = 0;
     	   		}
     	   		if(key == SDLK_4){
     	   			keys[0xc] = 0;
     	   		}
     	   		if(key == SDLK_q){
     	   			keys[0x4] = 0;
     	   		}
     	   		if(key == SDLK_w){
     	   			keys[0x5] = 0;
     	   		}
     	   		if(key == SDLK_e){
     	   			keys[0x6] = 0;
     	   		}
     	   		if(key == SDLK_r){
     	   			keys[0xd] = 0;
     	   		}
     	   		if(key == SDLK_a){
     	   			keys[0x7] = 0;
     	   		}
     	   		if(key == SDLK_s){
     	   			keys[0x8] = 0;
     	   		}
     	   		if(key == SDLK_d){
     	   			keys[0x9] = 0;
     	   		}
     	   		if(key == SDLK_f){
     	   			keys[0xe] = 0;
     	   		}
     	   		if(key == SDLK_z){
     	   			keys[0xa] = 0;
     	   		}
     	   		if(key == SDLK_x){
     	   			keys[0x0] = 0;
     	   		}
     	   		if(key == SDLK_c){
     	   			keys[0xb] = 0;
     	   		}
     	   		if(key == SDLK_v){
     	   			keys[0xf] = 0;
     	   		}
     	   }
    	}
		cpu_tick();
		update_timers();
		display();
	}

	return 0;
}
