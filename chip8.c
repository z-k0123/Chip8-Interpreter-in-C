// TODO: Fix the flickering issue in pong rom. Debug opcodes: 8xy4 8xy5 8xy6 8xy7 8xyE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define scale 10

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;

unsigned short opcode; //2 bytes each
unsigned char memory[4096]; //1 byte each
unsigned char V[16];
unsigned short I; //index register
unsigned short pc; //program counter
unsigned char delay_timer;
unsigned char sound_timer;
unsigned short stack[16]; // Stack for subroutine calls
unsigned char SP; //stack pointer
unsigned char screen[SCREEN_WIDTH * SCREEN_HEIGHT];
unsigned char keypad[16];
int drawFlag = 0;
const unsigned int FONTSETSTART = 0x50;
unsigned char fontset[80] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


int main(int argc, char *argv[]){
    if (argc < 2) {
        printf("Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }
    printf("Starting...\n");
    initialize();
    load_rom(argv[1]);
	
    for (int i = 0; i < 10; i++) {
    printf("memory[%X] = %02X\n", 0x200 + i, memory[0x200 + i]);
    }
	
    setupGraphics();

    while(1){
        handle_input(keypad);
        emulateCycle();
        SDL_Delay(16);
        if(drawFlag){
            render(renderer,texture,screen);
            drawFlag = 0;
        }
    }
    return 0;
}

void initialize(){ //set the emulator's memory, registers, and state to their default values.
    printf("Initializing...\n");
    srand(time(NULL));
    pc=0x200;
    opcode=0;
    I=0;
    SP=0;
    memset(memory,0,sizeof(memory));
    memset(V,0,sizeof(V));
    memset(stack,0,sizeof(stack));
    memset(screen,0,sizeof(screen));
    memset(keypad, 0, sizeof(keypad));
    delay_timer=0;
    sound_timer=0;

    for(int i = 0; i<80; i++){
        memory[FONTSETSTART + i] = fontset[i];
    }
}
void load_rom(const char *filename){
    FILE *rom = fopen(filename, "rb");
    printf("Loading rom...\n");

    if(!rom){
        printf("Error: Couldn't open file %s\n",filename);
        exit(1); }

    if(rom){
        fseek(rom,0,SEEK_END); //Move the file pointer to the end of the file.
        long fileSize = ftell(rom); //Since the pointer is at the end, ftell() returns the byte offset from the start of the file, which equals the file size.
        rewind(rom);
        printf("File size: %ld \n",fileSize);

        unsigned char *buffer = malloc(fileSize * sizeof(unsigned char)); //Allocates a buffer (unsigned char*) of size (fileSize) to temporarily store the ROM data.
        if(!buffer){
            printf("Error: Memory allocation failed. \n");
            fclose(rom);
            exit(1);
        }

        size_t read_rom = fread(buffer, sizeof(unsigned char), fileSize, rom); // Read the rom into the buffer

        if(read_rom != fileSize){
            printf("Error: Couldn't read file correctly. \n");
            fclose(rom);
            exit(1);
        }

        for(int i=0; i<fileSize; i++){
            memory[0x200 + i] = buffer[i];
        }

        free(buffer);
        fclose(rom);
        }
}

void execute_opcode(){
    switch(opcode & 0xF000){

        case 0x0000:
            switch(opcode & 0x00FF){
                case 0x00E0: //CLS Clear the display.
                    memset(screen,0,sizeof(screen));
                    break;

                case 0x00EE: //Return from a subroutine.
                    if(SP == 0) {
                        printf("Stack underflow! SP = %hu!\n",SP);
                        exit(1);
                    }
                    SP--;
                    pc = stack[SP];
                    break;

                default: printf("Unknown opcode: 0x%04X\n", opcode); break;

                }
        break; //NOTE TO SELF: I forgot the break statement here and it resulted in opcode always returning 0x0000.

        case 0x1000: //JP addr Jump to location nnn.
            pc = opcode & 0x0FFF;
            break;

        case 0x2000: //CALL addr Call subroutine at nnn.
            if(SP >= 16) {
                printf("Stack overflow!\n");
                exit(1);
            }
            stack[SP] = pc;  //**************************
            SP++;
            pc = opcode & 0x0FFF;
            break;

        case 0x3000: //3xkk SE Vx, byte. Skip next instruction if Vx = kk.
            if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)){
                pc += 2;
            }
            break;

        case 0x4000: //4xkk Skip next instruction if Vx != kk.
            if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)){
                pc += 2;
            }
            break;

        case 0x5000: //5xy0 Skip next instruction if Vx = Vy.
            if ((opcode & 0x000F) == 0){
            if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]){
                pc +=2;
            }}
            break;

        case 0x6000: //6xkk Set Vx = kk.
            V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            break;

        case 0x7000: //7xkk Set Vx = Vx + kk.
            V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            break;

        case 0x8000:
            switch (opcode & 0x000F){
                case 0x0000: //8xy0 - LD Vx, Vy Set Vx = Vy.
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0001: //8xy1 - OR Vx, Vy Set Vx = Vx OR Vy.
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] | V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0002: //8xy2 - AND Vx, Vy Set Vx = Vx AND Vy.
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] & V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0003: //8xy3 - XOR Vx, Vy Set Vx = Vx XOR Vy.
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] ^ V[(opcode & 0x00F0) >> 4];
                    break;

                case 0x0004: //8xy4 - ADD Vx, Vy Set Vx = Vx + Vy, set VF = carry.
                    {int result = V[(opcode & 0x0F00) >> 8] + V[(opcode & 0x00F0) >> 4];
                    V[(opcode & 0x0F00) >> 8] = result & 0xFF;
                    V[0xF] = (result > 255) ? 1 : 0;
                    break;}

                case 0x0005: //8xy5 - SUB Vx, Vy Set Vx = Vx - Vy, set VF = NOT borrow.
                    V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4]) ? 1 : 0;
                    break;

                case 0x0006: //SHR Vx {, Vy} Set Vx = Vx SHR 1. If the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0. Then Vx is divided by 2.
                    V[0xF] = (V[(opcode & 0x0F00) >> 8] & 0x01) ? 1 : 0;
                    V[(opcode & 0x0F00) >> 8] >>= 1;
                    break;

                case 0x0007: //8xy7 SUBN Vx, Vy Set Vx = Vy - Vx, set VF = NOT borrow.
                    V[0xF] = (V[(opcode & 0x00F0) >> 4] >= V[(opcode & 0x0F00) >> 8]) ? 1 : 0;
                    V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x000E: //8xyE SHL Vx {, Vy} Set Vx = Vx SHL 1. If the most-significant bit of Vx is 1, then VF is set to 1, otherwise to 0. Then Vx is multiplied by 2.
                    V[0xF] = (V[(opcode & 0x0F00)  >> 8] & 0x80) ? 1 : 0; // NOTE TO SELF: I made a mistake of equaling MSB of Vx to 1 instead of 0x80
                    V[(opcode & 0x0F00) >> 8] <<= 1;
                    break;
                default: printf("Unknown opcode: 0x%04X\n", opcode); break;
            }
            break;

        case 0x9000: //9xy0 SNE Vx, Vy Skip next instruction if Vx != Vy.
            if(V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]){
                pc += 2;
            }
            break;

        case 0xA000: //LD I, addr Annn Set I = nnn.
            I = opcode & 0x0FFF;
            break;

        case 0xB000: //JP V0, addr Jump to location nnn + V0.
            pc = (opcode & 0x0FFF) + V[0];
            break;

        case 0xC000:{ //Cxkk - RND Vx, byte Set Vx = random byte AND kk.
             int randByte = rand() % 256;
             V[(opcode & 0x0F00) >> 8] = randByte & (opcode & 0x00FF);
            }
            break;

        case 0xD000:{ //Dxyn DRW Vx, Vy, nibble Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
            unsigned char x = V[(opcode & 0x0F00) >> 8]; //x coordinate
            unsigned char y = V[(opcode & 0x00F0) >> 4]; //y coordinate
            unsigned char height = opcode & 0x000F; //n
            unsigned char pixel;
            unsigned char x_wrap = x % SCREEN_WIDTH;
            unsigned char y_wrap = y % SCREEN_HEIGHT;
            V[0xF] = 0;

            for(unsigned char row=0; row<height; row++){
                pixel = memory[I + row];

                for(unsigned char col=0; col<8; col++){ //Each sprite row is 8 pixels
                        if((pixel & (0x80 >> col)) != 0){
                            if(screen[((y_wrap + row) * SCREEN_WIDTH) + (x_wrap + col)] != 0){
                                V[0xF] = 1;
                            }
                            screen[((y_wrap + row) * SCREEN_WIDTH) + (x_wrap + col)] ^= 1;
                        }
                }
            }
            drawFlag = 1;
            break;}

        case 0xE000:
            switch (opcode & 0x00FF){
                case 0x009E: //Ex9E SKP Vx Skip next instruction if key with the value of Vx is pressed.
                    if(keypad[V[(opcode & 0x0F00) >> 8]]){
                        pc += 2;
                    }
                    break;

                case 0x00A1: //ExA1 SKNP Vx Skip next instruction if key with the value of Vx is not pressed.
                    if(keypad[V[(opcode & 0x0F00) >> 8]] == 0){
                        pc += 2;
                    }
                    break;
            }
            break;

        case 0xF000:
            switch (opcode & 0x00FF){
                case 0x0007: //Fx07 LD Vx, DT Set Vx = delay timer value.
                    V[(opcode & 0x0F00) >> 8] = delay_timer;
                    break;

                case 0x000A:{ //Fx0A - LD Vx, K Wait for a key press, store the value of the key in Vx.
                    int keyPressed = 0;
                    for(int i=0;i<16;i++){
                        if(keypad[i]){
                            V[(opcode & 0x0F00) >> 8] = i;
                            keyPressed = 1;
                            break;}
                    }
                    if(!keyPressed){
                        pc -= 2;
                    }
                    while (keypad[V[(opcode & 0x0F00) >> 8]]) {
                        handle_input(keypad); // update keypad status by polling events
                        SDL_Delay(10);         // small delay to prevent a busy loop
                    }

                    break;}

                case 0x0015: //Fx15 LD DT, Vx Set delay timer = Vx.
                    delay_timer = V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x0018: //Fx18 LD ST, Vx Set sound timer = Vx.
                    sound_timer = V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x001E: //Fx1E ADD I, Vx Set I = I + Vx.
                    I += V[(opcode & 0x0F00) >> 8];
                    break;

                case 0x0029: //Fx29 Set I = location of sprite for digit Vx.
                    I = FONTSETSTART + (V[(opcode & 0x0F00) >> 8] * 5);
                    break;

                case 0x0033:{ //Fx33 Store BCD representation of Vx in memory locations I, I+1, and I+2.
                    unsigned char number = V[(opcode & 0x0F00) >> 8];
                    memory[I+2] = number % 10; //ones

                    number /= 10; //tens
                    memory[I+1] = number % 10;

                    number /= 10; //hundreds
                    memory[I] = number % 10;
                    break;}

                case 0x0055:{ //Fx55 Store registers V0 through Vx in memory starting at location I.
                    unsigned char x = ((opcode & 0x0F00) >> 8); // NOTE TO SELF: I made a mistake of equaling x to Vx instead "x" from Fx55
                    for(int i = 0; i<=x; i++){
                        memory[I+i] = V[i];
                    }
                    break;}

                case 0x0065: //Fx65 Read registers V0 through Vx from memory starting at location I.
                    for(int i = 0; i<=((opcode & 0x0F00) >> 8); i++){ // NOTE TO SELF: Same mistake as Fx55
                        V[i] = memory[I+i];
                    }
                    break;
                default: printf("Unknown opcode: 0x%04X\n", opcode); break;
            }
            break;

        default:
            printf("Unknown opcode: 0x%04X\n", opcode);
            break; }
}

void update_timer(){
    if (delay_timer > 0) {
        delay_timer--;
    }
    if (sound_timer > 0) {
        sound_timer--;
    }
}

void handle_input(unsigned char *keypad){
    int isRunning = 1;
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        if(event.type == SDL_QUIT){
            isRunning = 0;
            exit(0);
        } else if ((event.type == SDL_KEYDOWN) || (event.type == SDL_KEYUP)){
            unsigned char key_state = (event.type == SDL_KEYDOWN) ? 1 : 0; //if a key is pressed -> key_state = 1
            switch(event.key.keysym.sym){
                case SDLK_1: keypad[0x1] = key_state; printf("Keypad[0x1] = %d\n", keypad[0x1]); break;
                case SDLK_2: keypad[0x2] = key_state; printf("Keypad[0x2] = %d\n", keypad[0x2]); break;
                case SDLK_3: keypad[0x3] = key_state; printf("Keypad[0x3] = %d\n", keypad[0x3]); break;
                case SDLK_4: keypad[0xC] = key_state; printf("Keypad[0xC] = %d\n", keypad[0xC]); break;
                case SDLK_q: keypad[0x4] = key_state; printf("Keypad[0x4] = %d\n", keypad[0x4]); break;
                case SDLK_w: keypad[0x5] = key_state; printf("Keypad[0x5] = %d\n", keypad[0x5]); break;
                case SDLK_e: keypad[0x6] = key_state; printf("Keypad[0x6] = %d\n", keypad[0x6]); break;
                case SDLK_r: keypad[0xD] = key_state; printf("Keypad[0xD] = %d\n", keypad[0xD]); break;
                case SDLK_a: keypad[0x7] = key_state; printf("Keypad[0x7] = %d\n", keypad[0x7]); break;
                case SDLK_s: keypad[0x8] = key_state; printf("Keypad[0x8] = %d\n", keypad[0x8]); break;
                case SDLK_d: keypad[0x9] = key_state; printf("Keypad[0x9] = %d\n", keypad[0x9]); break;
                case SDLK_f: keypad[0xE] = key_state; printf("Keypad[0xE] = %d\n", keypad[0xE]); break;
                case SDLK_z: keypad[0xA] = key_state; printf("Keypad[0xA] = %d\n", keypad[0xA]); break;
                case SDLK_x: keypad[0x0] = key_state; printf("Keypad[0x0] = %d\n", keypad[0x0]); break;
                case SDLK_c: keypad[0xB] = key_state; printf("Keypad[0xB] = %d\n", keypad[0xB]); break;
                case SDLK_v: keypad[0xF] = key_state; printf("Keypad[0xF] = %d\n", keypad[0xF]); break;
            }
        }
    }
}

unsigned short fetch_opcode(unsigned char *memory, unsigned short pc) {
    opcode = (memory[pc] << 8) | (memory[pc + 1]);
    printf("fetching opcode 0x%04X\n",opcode);
    return opcode;
}

void emulateCycle(){
    printf("Emulating cycle...\n");
    printf("PC = 0x%04X\n", pc);
    opcode = fetch_opcode(memory, pc);
    printf("PC: 0x%04hX | Opcode: 0x%04hX\n", pc, opcode);
    pc += 2;
    execute_opcode(opcode);
    update_timer();
}
int setupGraphics() {
    printf("Setting up graphics...\n");

    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        printf("SDL_Init Error %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH*scale , SCREEN_HEIGHT*scale , SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (renderer == NULL) {
        SDL_DestroyWindow(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    if (texture == NULL) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        printf("SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    return 0;
}

void render(SDL_Renderer *renderer, SDL_Texture *texture, unsigned char *screen){
    SDL_SetRenderDrawColor(renderer, 200, 0,0,255);
    SDL_RenderClear(renderer);

    void *pixels; //points to a block of memory provided by SDL
    int pitch; //number of pixels per row
    SDL_LockTexture(texture, NULL, &pixels, &pitch); //for direct access to the textures pixel data

    int *pixelBuffer = (int *)pixels;

    for(int y = 0; y<SCREEN_HEIGHT; y++){
        for(int x = 0; x<SCREEN_WIDTH; x++){
            int color = (screen[y * SCREEN_WIDTH + x]) ? 0xFFFFFFFF : 0x000000FF;
            pixelBuffer[y * (pitch / sizeof(Uint32)) + x] = color; //NOTE TO SELF: CHECK THIS
        }
    }

    SDL_UnlockTexture(texture); // CHECK HERE!!!!!!!!
    // Copy the texture to the renderer and present it
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
