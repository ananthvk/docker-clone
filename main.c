#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include<signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void sigint_handler(int s){
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: ./container <program_location> [args]\n");
        exit(1);
    }
    
    signal(SIGINT, sigint_handler);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }

    if (pid == 0)
    {
        // argv[1] is the program to execute
        // Forward all other arguments to the new program
        printf("===> Executing %s [%d]\n", argv[1], getpid());
        if (execvp(argv[1], argv + 1) == -1)
        {
            perror("execvp");
            exit(1);
        }
    }
    else
    {
        // Wait for the child process to finish
        int status = 0;
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid");
        }
        if (WIFEXITED(status))
        {
            const int exit_status = WEXITSTATUS(status);
            printf("===> %s exited with exit code %d\n", argv[1], exit_status);
        }
    }
}