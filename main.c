#define _GNU_SOURCE
#include "run.h"
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "config.h"

void sigint_handler(int s) {}

int main(int argc, char *argv[])
{
    // Basic initialization
    srand(time(NULL));
    // ======
    // Parse command line args and run the appropriate sub command
    if (argc < 2 || (argc >= 2 && strcmp(argv[1], "help") == 0))
    {
        printf("Usage: ./container command [options]\n\n");
        printf("commands:\n");
        printf("run     Runs the specified image after creating a new container\n");
        printf("        a file called <image_name>.tar.gz must exist within " IMAGE_PATH "\n");
        printf("        Containers will be created in " CONTAINER_PATH "\n");
        exit(1);
    }
    if (strcmp(argv[1], "run") == 0)
    {
        container_run(argc - 2, argv + 2);
    }
    else
    {
        printf("Invalid command, run ./container help to view the help.\n");
    }
    /*
    if (argc < 2)
    {
        printf("Usage: ./container <program_location> [args]\n");
        exit(1);
    }
    printf("===> Mounting /proc, /sys, /dev\n");
    if (mount("/proc", IMAGE_PATH "/proc/", "proc", 0, NULL) == -1)
    {
        perror("mount proc");
    }
    if (mount("/sys", IMAGE_PATH "/sys/", 0, MS_BIND | MS_REC | MS_PRIVATE, NULL) == -1)
    {
        perror("mount sys");
    }
    if (mount("/dev", IMAGE_PATH "/dev/", 0, MS_BIND | MS_REC | MS_PRIVATE, NULL) == -1)
    {
        perror("mount dev");
    }

    printf("===> Chrooting into %s\n", IMAGE_PATH);
    if (chroot(IMAGE_PATH) == -1)
    {
        perror("chroot");
        exit(1);
    }

    if (chdir(IMAGE_PATH) == -1)
    {
        perror("chdir");
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
        printf("===> Creating new namespace\n");
        if(unshare(CLONE_NEWNS) == -1){
            perror("unshare");
            exit(1);
        }
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
            exit(1);
        }
        if (WIFEXITED(status))
        {
            const int exit_status = WEXITSTATUS(status);
            printf("===> %s exited with exit code %d\n", argv[1], exit_status);
            printf("===> Unmounting file systems\n");
            if(umount(IMAGE_PATH "/dev/") == -1){perror("umount dev");};
            if(umount(IMAGE_PATH "/sys/") == -1){perror("umount sys");};
            if(umount(IMAGE_PATH "/proc/") == -1){perror("umount proc");};
            printf("===> DONE\n");
        }
    }
    */
}

// TODO: Support custom environment variables
