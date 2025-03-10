#include<stdio.h>
#include<SDL2/SDL.h>
#define WINDOW_TITLE "01 Open Window"
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 400

struct emulator
{
    SDL_Window *window;
    SDL_Renderer *renderer;
};

int sdlInitialize(struct emulator *em)
{
    if(SDL_Init(SDL_INIT_EVERYTHING))
    {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }
    em->window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
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

void emCleanup(struct emulator *em)
{
    SDL_DestroyRenderer(em->renderer);
    SDL_DestroyWindow(em->window);
    SDL_Quit();
}

int main()
{
    struct emulator em;
    em.window = NULL;
    em.renderer = NULL;
    printf("Hello World!\n");
    if(sdlInitialize(&em))
    {
        printf("Error\n");
        emCleanup(&em);
        exit(1);
    }
    SDL_Delay(5000);
    emCleanup(&em);
    printf("Success\n");
    return 0;
}