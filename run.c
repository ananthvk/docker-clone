#define _GNU_SOURCE
#include "run.h"
#include "config.h"
#include "container.h"
#include "string.h"
#include "utils.h"
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * @short Parses the passed arguments and runs the specified image in a new container.
 * @param argc number of arguments after run subcommand
 * @param argv arguments after the run subcommand, null terminated
 */

void cmd_run(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: ./container run [options] image_name command [command options]\n");
        printf("Only %d argument(s) supplied\n", argc);
        exit(1);
    }
    struct Container container;
    container.containers_path = CONTAINER_PATH;
    container.image_name = argv[0];
    container.images_path = IMAGE_PATH;
    container.id_length = CONTAINER_ID_LENGTH;
    printf("=> Creating container\n");
    container_create(&container);
    printf("=> Created container %s [%s] \n", container.image_name, container.id);
    container_extract_image(&container);
    printf("=> Extracted image to container\n");
    // Fork the process and create the container
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork, container creation");
        exit(1);
    }
    if (pid == 0)
    {
        // Unshare to create new namespace for new mounts
        if (unshare(CLONE_NEWNS) == -1)
        {
            perror("unshare: could not create new namespace, are you running as sudo?");
            exit(1);
        }

        // Mount the root file system as private so that mount events do not propagate in and out of
        // it
        if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1)
        {
            perror("mount root");
            exit(1);
        }

        container_create_mounts(&container);

        if (chroot(container.root) == -1)
        {
            perror("chroot");
            exit(1);
        }

        if (chdir(container.root) == -1)
        {
            perror("chdir");
            exit(1);
        }
        if (execvp(argv[1], argv + 1))
        {
            perror("execvp");
            exit(1);
        }
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        container_delete(&container);
        // Free resources
        free(container.id);
        free(container.root);
    }
}