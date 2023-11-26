#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define IMAGE_PATH "images/debian"

void sigint_handler(int s) {}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: ./container <program_location> [args]\n");
        exit(1);
    }
    printf("===> Mounting /proc, /sys, /dev\n");
    if (mount("/proc", IMAGE_PATH "/proc/", "proc", MS_PRIVATE, NULL) == -1)
    {
        perror("mount proc");
    }
    if (mount("/sys", IMAGE_PATH "/sys/", 0, MS_BIND |MS_REC| MS_PRIVATE, NULL) == -1)
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
    
    if(chdir(IMAGE_PATH) == -1){
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
            // if(umount(IMAGE_PATH "/dev/pts/") == -1){perror("umount dev/pts");};
            // if(umount(IMAGE_PATH "/dev/") == -1){perror("umount dev");};
            // if(umount(IMAGE_PATH "/sys/") == -1){perror("umount sys");};
            // if(umount(IMAGE_PATH "/proc/") == -1){perror("umount proc");};
            printf("===> DONE\n");
        }
    }
}

// TODO: Support custom environment variables
