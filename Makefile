CC=gcc
CFLAGS=-O0 -ggdb3 -Wall -Wextra -pedantic -fsanitize=address,undefined -pedantic -Wno-unused-parameter -Wno-unused-variable
build:
	$(CC) $(CFLAGS) main.c -o main.bin

run: build
	./main.bin
	