#include<stdio.h>
#include<SDL2/SDL.h>

#define WINDOW_TITLE "Chip 8"
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE_FACTOR 20

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
    char romName;
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

int chip8Initialize(chip8mem* chip8, char romName)
{
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
    chip8->stack_ptr = -1;
}

emulateInstruction(chip8mem* chip8)
{
    chip8->inst.opcode = (chip8->ram[chip8->pc] << 8) | chip8->ram[chip8->pc+1];
    chip8->pc += 2;

    chip8->inst.nnn = chip8->inst.opcode & 0x0FFF;
    chip8->inst.kk = chip8->inst.opcode & 0x0FF;
    chip8->inst.n = chip8->inst.opcode & 0x0F;
    chip8->inst.x = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.y = (chip8->inst.opcode >> 4) & 0x0F;

    switch((chip8->inst.opcode>>12))
    {
        case 0x0:
            if(chip8->inst.kk == 0XE0)
            {
                memset(&chip8->display[0], 0, sizeof(chip8->display));
                chip8->pc = chip8->stack[chip8->stack_ptr--];
            }
        case 0x01:
            chip8->pc = chip8->inst.nnn;
    }
}

int main()
{
    struct emulator em;
    em.window = NULL;
    em.renderer = NULL;
    if(sdlInitialize(&em))
    {
        printf("Error\n");
        emCleanup(&em, EXIT_FAILURE);
        exit(1);
    }
    while (1) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                emCleanup(&em, EXIT_SUCCESS);
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    emCleanup(&em, EXIT_SUCCESS);
                    break;
                default:
                    break;
                }
            default:
                break;
            }
        }
        emulateInstruction(&em);
        SDL_RenderClear(em.renderer);
        /*SDL_SetRenderDrawColor(em.renderer, 255, 0, 0, 255);
        SDL_RenderClear(em.renderer);   //Changes colour to red
        SDL_RenderPresent(em.renderer);*/
        SDL_RenderPresent(em.renderer);

        SDL_Delay(16);
    }

    emCleanup(&em, EXIT_SUCCESS); 
    return 0;
}