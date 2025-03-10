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
    uint8_t ram[4096];
    uint8_t V[16]; //last reg (VF) not to be used by anything else
    uint16_t I;
    uint8_t soundTimer; //When non zero, decremented at 60Hz
    uint8_t delayTimer; //When non zero, decremented at 60Hz
    uint16_t pc;
    uint16_t stack[16]
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