#ifndef CONTAINER_UTIL_H
// Commonly used utility functions
#include <stdlib.h>

#define CONTAINER_UTIL_H

void *safe_malloc(size_t size);
char *random_id(char *buffer, int length);
int exists(const char* path);
#endif // CONTAINER_UTIL_H