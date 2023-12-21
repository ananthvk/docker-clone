#define _GNU_SOURCE
#include "run.h"
#include "config.h"
#include "container.h"
#include "string.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * @short Parses the passed arguments and runs the specified image in a new container.
 * @param argc number of arguments after run subcommand
 * @param argv arguments after the run subcommand, null terminated
 */
struct container_args
{
    struct Container container;
    int argc;
    char **argv;
};

static int run_container(void *data)
{
    if (data == NULL)
    {
        printf("data was null, internal error");
        exit(1);
    }
    struct container_args *c = (struct container_args *)(data);
    struct Container container = (c->container);
    int argc = c->argc;
    char **argv = c->argv;
    if (sethostname(container.id, container.id_length) == -1)
    {
        perror("Could not set hostname");
        exit(1);
    }
    // Mount the root file system as private so that mount events do not propagate in and out of
    // it
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1)
    {
        perror("mount root");
        exit(1);
    }
    container_extract_image(&container);
    container_create_overlayfs(&container);
    container_create_mounts(&container);

    char path[PATH_MAX];
    // TODO: Check if snprintf fails
    snprintf(path, PATH_MAX, "%s/old-root%s", container.root, container.id);
    mkdir(path, 0777);

    if (syscall(SYS_pivot_root, container.root, path) == -1)
    {
        perror("Pivot root");
        exit(1);
    }

    if (chdir("/") == -1)
    {
        perror("chdir");
        exit(1);
    }

    // Remove old mount
    snprintf(path, PATH_MAX, "/old-root%s", container.id);
    if (umount2(path, MNT_DETACH) == -1)
    {
        perror("umount2");
        exit(1);
    }

    if (rmdir(path) == -1)
    {
        perror("rmdir");
        exit(1);
    }

    if (execvp(argv[1], argv + 1))
    {
        perror("execvp");
        exit(1);
    }
    return 0;
}

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
    // Clone the process and create the container
    struct container_args data;
    data.argc = argc;
    data.argv = argv;
    data.container = container;

    char *stack = safe_malloc(STACK_SIZE);
    char *stack_top = stack + STACK_SIZE; // Since stack grows downwards
    // +-----+----+ <- char* stack_top (for the child, it grows in downard direction)
    // |     |    | 0x5000
    // |     |    | 0x4000
    // |     |    | 0x3000
    // |     |    | 0x2000
    // |     v    | 0x1000
    // +----------+ <- char* stack
    // |          |
    // |          |
    // |          |
    // |          |
    // |          |
    // +----------+

    // Clone - new namespace, new uts for a new hostname, sigchld so that the parent is notified
    // if the child exits
    pid_t pid =
        clone(&run_container, stack_top,
              CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNET | SIGCHLD, (void *)&data);
    // After adding CLONE_NEWPID, running ps -e inside the container
    // does not show any process running on the host
    // ps -e from outside the container shows the processes inside the container
    // kill -9 also works from outside the container

    if (pid == -1)
    {
        perror("clone, container creation");
        exit(1);
    }
    printf("=> PID of container: %d\n", pid);
    // Connect the created container to an existing docker0 bridge for development
    // TODO: Create a new bridge for this application, along with routing
    container_connect_to_bridge(&container, (int)pid);
    int status;
    waitpid(pid, &status, 0);
    container_delete(&container);
    // Free resources
    free(container.id);
    free(container.root);
    free(container.container_dir);
    free(container.image_path);
    free(stack);
}

// sudo debootstrap --arch amd64 jammy images/ubuntu 'http://archive.ubuntu.com/ubuntu/
// https://cdimage.ubuntu.com/ubuntu-base/
// https://cdimage.ubuntu.com/ubuntu-base/jammy/daily/current/jammy-base-amd64.tar.gz
// TODO: Add sigint