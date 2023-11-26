CC=gcc
CFLAGS=-O0 -ggdb3 -Wall -Wextra -pedantic -fsanitize=address,undefined -pedantic -Wno-unused-parameter -Wno-unused-variable
build: main.c run.c run.h 
	$(CC) $(CFLAGS) main.c run.c -o container

run: build
	./container
	