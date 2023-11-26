#include "utils.h"
#include <unistd.h>

// A safer version of malloc, which exits the program if malloc fails
void *safe_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        exit(2);
    }
    return ptr;
}

// Returns a random hexadecimal id string to be used as container id
// Note: It would be better to use UUID to guarantee uniqueness, but for this simple solution
// it will suffice to check if a directory with the id exists.
char *random_id(char * buffer, int length)
{
    static const char table[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (int i = 0; i < length; i++)
    {
        buffer[i] = table[rand() % 16];
    }
    return buffer;
}

// Returns 1 if a file or folder exists at the given path
int exists(const char *path)
{
    if (access(path, F_OK) == 0)
    {
        return 1;
    }
    return 0;
}
