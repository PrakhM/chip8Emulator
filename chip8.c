#include<stdio.h>
#include<SDL2/SDL.h>
#include<time.h>
#include<string.h>

#define WINDOW_TITLE "Chip 8"
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE_FACTOR 20
#define INSTRUCTION_PER_SECOND 500

struct emulator
{
    SDL_Window *window;
    SDL_Renderer *renderer;
};

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
} sdl_t;

typedef struct
{
    uint16_t opcode;
    uint16_t nnn; //12 byte
    uint8_t n; //4 byte
    uint8_t x; //4 byte
    uint8_t y; //4 byte
    uint8_t kk; //8 byte
} instructions;

typedef struct
{
    char *romName;
    uint8_t ram[4096];
    uint8_t V[16]; //last reg (VF) not to be used by anything else
    uint16_t I;
    uint8_t soundTimer; //When non zero, decremented at 60Hz
    uint8_t delayTimer; //When non zero, decremented at 60Hz
    uint16_t pc;
    uint16_t stack[16];
    uint8_t stack_ptr;
    int keypad[16];
    instructions inst;
    uint8_t display[64*32];
} chip8mem;


int sdlInitialize(struct emulator *em)
{
    if(SDL_Init(SDL_INIT_EVERYTHING))
    {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }
    em->window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH * SCALE_FACTOR, SCREEN_HEIGHT * SCALE_FACTOR, 0);
    if(!em->window)
    {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return 1;       
    }

    em->renderer = SDL_CreateRenderer(em->window, -1, 0);
    if(!em->renderer)
    {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return 1;       
    }
    return 0;
}

void emCleanup(struct emulator *em, int exitStatus)
{
    SDL_DestroyRenderer(em->renderer);
    SDL_DestroyWindow(em->window);
    SDL_Quit();
    exit(exitStatus);
}

int chip8Initialize(chip8mem* chip8, char romName[])
{
    memset(chip8, 0, sizeof(chip8mem));
    uint32_t entryPoint = 0x200;
    chip8->pc = entryPoint;
    uint8_t font[] = 
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
    memcpy(&chip8->ram[0], font, sizeof(font));
    FILE* rom = fopen(romName, "rb");
    if(!rom)
    {
        printf("ROM could not be loaded\n");
        return 1;
    }
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entryPoint;
    rewind(rom);

    if (rom_size > max_size) 
    {
        SDL_Log("Rom file %s is too big! Rom size: %llu, Max size allowed: %llu\n", 
                romName, (long long unsigned)rom_size, (long long unsigned)max_size);
        return 1;
    }
    
    if (fread(&chip8->ram[entryPoint], rom_size, 1, rom) != 1) //Loading ROM
    {
        SDL_Log("Could not read Rom file %s into CHIP8 memory\n", 
                romName);
        return 1;
    }
    chip8->romName = romName;
    fclose(rom);
    srand(time(NULL));
    chip8->stack_ptr = -1;
    return 0;
}

void emulateInstruction(chip8mem* chip8)
{
    chip8->inst.opcode = (chip8->ram[chip8->pc] << 8) | chip8->ram[chip8->pc+1];
    chip8->pc += 2;

    chip8->inst.nnn = chip8->inst.opcode & 0x0FFF;
    chip8->inst.kk = chip8->inst.opcode & 0x0FF;
    chip8->inst.n = chip8->inst.opcode & 0x0F;
    chip8->inst.x = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.y = (chip8->inst.opcode >> 4) & 0x0F;
    //printf("%.4x\n",chip8->inst.opcode);
    switch((chip8->inst.opcode>>12))
    {
        /*
        7009
        7008
        7004
        7008
        7008
        */
        case 0x0:
            if(chip8->inst.kk == 0xE0)
            {
                memset(&chip8->display[0], 0, sizeof(chip8->display));
            }
            else if(chip8->inst.kk == 0xEE)
            {
                chip8->pc = chip8->stack[chip8->stack_ptr--];
            }
            break;
        case 0x01:
            chip8->pc = chip8->inst.nnn;
            break;
        case 0x02:
            chip8->stack_ptr++;
            chip8->stack[chip8->stack_ptr] = chip8->pc;
            chip8->pc = chip8->inst.nnn;
            break;
        case 0x03:
            if(chip8->inst.kk == chip8->V[chip8->inst.x]) chip8->pc+=2;
            break;
        case 0x04:
            if(chip8->inst.kk != chip8->V[chip8->inst.x]) chip8->pc+=2;
            break;
        case 0x05:
            if(chip8->V[chip8->inst.x] == chip8->V[chip8->inst.y]) chip8->pc+=2;
            break;
        case 0x06:
            chip8->V[chip8->inst.x] = chip8->inst.kk;
            break;
        case 0x07:
            chip8->V[chip8->inst.x] += chip8->inst.kk;
            break;
        case 0x08:
            switch(chip8->inst.n)
            {
                case 0x0:
                    chip8->V[chip8->inst.x] = chip8->V[chip8->inst.y];
                    break;
                case 0x1:
                    chip8->V[chip8->inst.x] = chip8->V[chip8->inst.x] | chip8->V[chip8->inst.y];
                    break;
                case 0x2:
                    chip8->V[chip8->inst.x] = chip8->V[chip8->inst.x] & chip8->V[chip8->inst.y];
                    break;
                case 0x3:
                    chip8->V[chip8->inst.x] = chip8->V[chip8->inst.x] ^ chip8->V[chip8->inst.y];
                    break;
                case 0x4:
                    int add = chip8->V[chip8->inst.x] + chip8->V[chip8->inst.y];
                    chip8->V[chip8->inst.x] = add;
                    if(add > 255)chip8->V[15] = 1;
                    else chip8->V[15] = 0;
                    break;
                case 0x5:
                    if(chip8->V[chip8->inst.x] > chip8->V[chip8->inst.y]) chip8->V[15] = 1;
                    else chip8->V[15] = 0;
                    chip8->V[chip8->inst.x] = chip8->V[chip8->inst.x] - chip8->V[chip8->inst.y];
                    break;
                case 0x6:
                    chip8->V[15] = (chip8->V[chip8->inst.x] & 0x1) ? 1 : 0;
                    chip8->V[chip8->inst.x] >>= 1;
                    break;
                case 0x7:
                    if(chip8->V[chip8->inst.y] > chip8->V[chip8->inst.x]) chip8->V[15] = 1;
                    else chip8->V[15] = 0;
                    chip8->V[chip8->inst.x] = chip8->V[chip8->inst.y] - chip8->V[chip8->inst.x];
                    break;
                case 0xE:
                    chip8->V[15] = (chip8->V[chip8->inst.x] & 0x80) ? 1 : 0;
                    chip8->V[chip8->inst.x] <<= 1;
                    break;
            }
            break;
        case 0x9:
            if(chip8->V[chip8->inst.x] != chip8->V[chip8->inst.y]) chip8->pc += 2;
            break;
        case 0x0A:
            chip8->I = chip8->inst.nnn;
            break;
        case 0x0B:
            chip8->pc = chip8->inst.nnn + chip8->V[0];
            break;
        case 0x0C:
            chip8->V[chip8->inst.x] = (rand() % 256) & chip8->inst.kk;
            break;
        case 0x0D:
            int windowHeight = SCREEN_HEIGHT;
            int windowWidth = SCREEN_WIDTH;
            uint8_t xcoord = chip8->V[chip8->inst.x] % windowWidth;
            uint8_t ycoord = chip8->V[chip8->inst.y] % windowHeight;
            uint8_t xOG = xcoord;
            chip8->V[0xF] = 0;
            for(uint8_t i = 0; i < chip8->inst.n; i++)
            {
                xcoord = xOG;
                uint8_t spriteData = chip8->ram[chip8->I + i];
                for(int8_t j = 7; j>=0; j--)
                {   
                    uint8_t *pixel = &chip8->display[xcoord + ycoord * windowWidth];
                    int changePixel = spriteData & (1 << j);
                    if(changePixel && *pixel)
                    {
                        chip8->V[0xF] = 1;
                    }
                    *pixel ^= changePixel;
                    if(++xcoord >= windowWidth) break;
                }
                if(++ycoord >= windowHeight) break;
            }
            break;
        case 0x0E:
            if(chip8->inst.kk == 0x9E)
            {
                chip8->pc += 2*chip8->keypad[chip8->V[chip8->inst.x]];
            }
            else if(chip8->inst.kk == 0xA1)
            {
                chip8->pc += 2*(!chip8->keypad[chip8->V[chip8->inst.x]]);
            }
            break;
        case 0x0F:
            if(chip8->inst.kk == 0x07)
            {
                chip8->V[chip8->inst.x]= chip8->delayTimer;
            }
            else if(chip8->inst.kk == 0x0A)
            {
                static int any_key_pressed = 0;
                static uint8_t key = 0xFF;

                for (uint8_t i = 0; key == 0xFF && i < sizeof chip8->keypad; i++) 
                    if (chip8->keypad[i]) {
                        key = i;
                        any_key_pressed = 1;
                        break;
                    }
                if (!any_key_pressed) chip8->pc -= 2; 
                else {
                    if (chip8->keypad[key])
                        chip8->pc -= 2;
                    else {
                        chip8->V[chip8->inst.x] = key;
                        key = 0xFF;        
                        any_key_pressed = 0;
                    }
                }           
            }
            else if(chip8->inst.kk == 0x15)
            {
                chip8->delayTimer = chip8->V[chip8->inst.x];
            }
            else if(chip8->inst.kk == 0x18)
            {
                chip8->soundTimer = chip8->V[chip8->inst.x];
            }
            else if(chip8->inst.kk == 0x1E)
            {
                chip8->I += chip8->V[chip8->inst.x];
            }
            else if(chip8->inst.kk == 0x29)
            {
                chip8->I = chip8->V[chip8->inst.x] * 5;
            }
            else if(chip8->inst.kk == 0x33)
            {
                uint8_t val = chip8->V[chip8->inst.x];
                
                chip8->ram[chip8->I] =  val/100;
                val = val%100;
                chip8->ram[chip8->I+1] =  val/10;
                val = val%10;
                chip8->ram[chip8->I + 2] =  val;
            }
            else if(chip8->inst.kk == 0x55)
            {
                for(int i = 0; i<=chip8->inst.x; i++)
                {
                    chip8->ram[chip8->I + i] = chip8->V[i];
                }
            }
            else if(chip8->inst.kk == 0x65)
            {
                for(int i = 0; i<=chip8->inst.x; i++)
                {
                    chip8->V[i] = chip8->ram[chip8->I + i];
                }
            }
            break;
        default:
            printf("Invalid\n");
    }
}

int main(int argc, char *argv[])
{
    argc = argc;
    struct emulator em;
    em.window = NULL;
    em.renderer = NULL;
    if(sdlInitialize(&em))
    {
        printf("Error\n");
        emCleanup(&em, EXIT_FAILURE);
        exit(1);
    }
    chip8mem chip8;
    chip8Initialize(&chip8, argv[1]);
    while (1) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            //printf("\nIn Poll Event -> ");
            switch (event.type) 
            {
                case SDL_QUIT:
                    emCleanup(&em, EXIT_SUCCESS);
                    break;
                case SDL_KEYDOWN:
                    //printf("Key down event -> ");
                    switch (event.key.keysym.sym)
                    {
                        case SDL_SCANCODE_ESCAPE:
                            emCleanup(&em, EXIT_SUCCESS);
                            break;
                        case SDLK_1: chip8.keypad[0x1] = 1; break;
                        case SDLK_2: chip8.keypad[0x2] = 1; break;
                        case SDLK_3: chip8.keypad[0x3] = 1; break;
                        case SDLK_4: chip8.keypad[0xC] = 1; break;

                        case SDLK_q: chip8.keypad[0x4] = 1; break;
                        case SDLK_w: chip8.keypad[0x5] = 1; break;
                        case SDLK_e: chip8.keypad[0x6] = 1; break;
                        case SDLK_r: chip8.keypad[0xD] = 1; break;

                        case SDLK_a: chip8.keypad[0x7] = 1; break;
                        case SDLK_s: chip8.keypad[0x8] = 1; break;
                        case SDLK_d: chip8.keypad[0x9] = 1; break;
                        case SDLK_f: chip8.keypad[0xE] = 1; break;

                        case SDLK_z: chip8.keypad[0xA] = 1; break;
                        case SDLK_x: chip8.keypad[0x0] = 1; break;
                        case SDLK_c: chip8.keypad[0xB] = 1; break;
                        case SDLK_v: chip8.keypad[0xF] = 1; break;

                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_1: chip8.keypad[0x1] = 0; break;
                        case SDLK_2: chip8.keypad[0x2] = 0; break;
                        case SDLK_3: chip8.keypad[0x3] = 0; break;
                        case SDLK_4: chip8.keypad[0xC] = 0; break;

                        case SDLK_q: chip8.keypad[0x4] = 0; break;
                        case SDLK_w: chip8.keypad[0x5] = 0; break;
                        case SDLK_e: chip8.keypad[0x6] = 0; break;
                        case SDLK_r: chip8.keypad[0xD] = 0; break;

                        case SDLK_a: chip8.keypad[0x7] = 0; break;
                        case SDLK_s: chip8.keypad[0x8] = 0; break;
                        case SDLK_d: chip8.keypad[0x9] = 0; break;
                        case SDLK_f: chip8.keypad[0xE] = 0; break;

                        case SDLK_z: chip8.keypad[0xA] = 0; break;
                        case SDLK_x: chip8.keypad[0x0] = 0; break;
                        case SDLK_c: chip8.keypad[0xB] = 0; break;
                        case SDLK_v: chip8.keypad[0xF] = 0; break;
                        default: break;
                    }
                    break;
                    
                default:
                    break;
            }
        }

        uint64_t before = SDL_GetPerformanceCounter();
        for(int i = 0; i <= INSTRUCTION_PER_SECOND/60; i++)
        {
            emulateInstruction(&chip8);
        }
        SDL_RenderClear(em.renderer);
        /*SDL_SetRenderDrawColor(em.renderer, 255, 0, 0, 255);
        SDL_RenderClear(em.renderer);   //Changes colour to red
        SDL_RenderPresent(em.renderer);*/
        SDL_Rect rect = {.x = 0, .y = 0, .w = SCALE_FACTOR, .h = SCALE_FACTOR};
        for(uint32_t i = 0; i < sizeof(chip8.display); i++)
        {
            rect.x = (i % SCREEN_WIDTH) * SCALE_FACTOR;
            rect.y = (i / SCREEN_WIDTH) * SCALE_FACTOR;
            if(chip8.display[i])
            {
                SDL_SetRenderDrawColor(em.renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(em.renderer, &rect);
            }
            else
            {
                SDL_SetRenderDrawColor(em.renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(em.renderer, &rect);
            }
        }
        SDL_RenderPresent(em.renderer);
        uint64_t after = SDL_GetPerformanceCounter();
        uint64_t freq = SDL_GetPerformanceFrequency();
        float diff = 1000.0f * (after - before)/freq;
        SDL_Delay(16.67f > diff? (uint64_t)16.67f - diff: 0);
        if(chip8.delayTimer > 0) chip8.delayTimer--;
        if(chip8.soundTimer > 0) chip8.soundTimer--;
    }

    emCleanup(&em, EXIT_SUCCESS); 
    return 0;
}