CFLAGS=-Wall -Wextra -Werror -lSDL2

all:
	gcc chip8.c -o chip8 $(CFLAGS)