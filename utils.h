#ifndef CONTAINER_UTIL_H
// Commonly used utility functions
#include <stdio.h>
#include <stdlib.h>

#define CONTAINER_UTIL_H

#define errorMessage(fmt, ...)                                                                     \
    do                                                                                             \
    {                                                                                              \
        perror("");                                                                                \
        fprintf(stderr, fmt, __VA_ARGS__);                                                         \
        exit(EXIT_FAILURE);                                                                        \
    } while (0)


void *safe_malloc(size_t size);
char *random_id(char *buffer, int length);
int exists(const char *path);
int strformat(char *buffer, size_t bufflen, const char *fmt, ...);
void exec_command(char *command, char **args);
void exec_command_fail_ok(char *command, char **args);
#endif // CONTAINER_UTIL_H