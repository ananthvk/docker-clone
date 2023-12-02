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
        // Pass arguments after ./container run
        cmd_run(argc - 2, argv + 2);
    }
    else
    {
        printf("Invalid command, run ./container help to view the help.\n");
    }
}
// TODO: Support custom environment variables
