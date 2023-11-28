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
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork, container creation");
        exit(1);
    }
    if (pid == 0)
    {
        // An array to store path after combining them
        char combined_path[PATH_MAX + 1] = {'\0'};

        // Fork the process and create the container

        // Unshare to create new namespace for mounts
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
/*
 *
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/admin-guide/devices.txt
 * To add to /dev
# mount -t devpts devpts /dev/pts 
# ln -s /proc/self/fd/0 /dev/stdin
# ln -s /proc/self/fd/1 /dev/stdout
# ln -s /proc/self/fd/2 /dev/stderr
# mknod -m=666 /dev/urandom c 1 9 
# mknod -m=666 /dev/random c 1 8 
# mknod -m=666 /dev/zero c 1 5
# mknod -m=666 /dev/null c 1 3
# mknod -m=666 /dev/tty c 5 0
# mknod -m=620 /dev/console c 5 1
# mknod -m=777 /dev/ptmx c 5 2
# ln -s /proc/kcore /dev/core
# ln -s /proc/fd /dev/fd 

Here are some more which I may need to add (after seeing a docker system)
crw-rw-rw- 666  full
drwxrwxrwt 1777 mqueue
drwxrwxrwt 1777 shm

Done:
lrwxrwxrwx 777  ptmx
lrwxrwxrwx 777  stderr
lrwxrwxrwx 777  stdin
lrwxrwxrwx 777  stdout
drwxr-xr-x 755  pts
crw-rw-rw- 666  urandom
crw-rw-rw- 666  zero
crw-rw-rw- 666  null
crw-rw-rw- 666  tty
crw-rw-rw- 666  random
crw--w---- 620  console
lrwxrwxrwx 777  core
lrwxrwxrwx 777  fd
*/