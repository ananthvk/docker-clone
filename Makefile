CC=gcc
CFLAGS=-O0 -ggdb3 -Wall -Wextra -pedantic -fsanitize=address,undefined -pedantic -Wno-unused-parameter -Wno-unused-variable
build: main.c run.c run.h  utils.c utils.h  container.c container.h
	$(CC) $(CFLAGS) main.c run.c utils.c container.c -o container -std=gnu11

run: build
	./container
	