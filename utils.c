#include "utils.h"
#include <unistd.h>
#include<limits.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
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
char *random_id(char *buffer, int length)
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


// This is a thin wrapper around snprintf, but this function calls exit
// if snprintf fails due to insufficient buffer capacity
int strformat(char *buffer, size_t bufflen, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int status = vsnprintf(buffer, bufflen, fmt, args);
    if (status < 0 || status >= PATH_MAX)
    {
        va_end(args);
        errorMessage("%s\n", "vsnprintf: Either the string was too large or snprintf failed.\n");
    }
    va_end(args);
    return status;
}

// Executes the given command after fork() using execvp()
// Incase of any error, calls exit()
void exec_command(char *command, char **args)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        // In child process
        if (execvp(command, args) == -1)
        {
            fprintf(stderr, "exec_command %s: %s\n", command, strerror(errno));
            exit(1);
        }
    }
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        fprintf(stderr, "waitpid (for exec_command %s): %s\n", command, strerror(errno));
        exit(1);
    }
    if (WIFEXITED(status))
    {
        const int exit_status = WEXITSTATUS(status);
        if (exit_status != EXIT_SUCCESS)
        {
            fprintf(stderr, "sub_command %s failed: %s\n", command, strerror(errno));
            exit(1);
        }
    }
}

void exec_command_fail_ok(char *command, char **args)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        // In child process
        if (execvp(command, args) == -1)
        {
            fprintf(stderr, "exec_command %s: %s\n", command, strerror(errno));
        }
    }
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        fprintf(stderr, "waitpid (for exec_command %s): %s\n", command, strerror(errno));
    }
    if (WIFEXITED(status))
    {
        const int exit_status = WEXITSTATUS(status);
        if (exit_status != EXIT_SUCCESS)
        {
            fprintf(stderr, "sub_command %s failed: %s\n", command, strerror(errno));
        }
    }
}