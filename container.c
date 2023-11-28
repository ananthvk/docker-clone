#include "container.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>

// @brief Creates a new directory in [container.containers_path] for the root of the container
// @details containers_path must be set before calling this function.
// Note this function also creates an ID for the container
void container_create(struct Container *container)
{
    if (container == NULL)
    {
        fprintf(stderr, "Critical: container was null\n");
        exit(3);
    }

    // Create a single ID which is null terminated for the container
    char *id_buf = safe_malloc(container->id_length + 1);
    random_id(id_buf, container->id_length);
    id_buf[container->id_length] = '\0';

    // It is assumed that containers_path ends with a '/'
    char *root = safe_malloc(strlen(container->containers_path) + container->id_length +
                             1); // +1 for null char
    strcpy(root, container->containers_path);
    strcat(root, id_buf);

    // If a file/folder by the same name exists, keep generating random IDs, until it finds
    // one which does not exist
    while (exists(root))
    {
        random_id(id_buf, container->id_length);
        strcpy(root, container->containers_path);
        strcat(root, id_buf);
    }
    container->root = root;
    container->id = id_buf;
    char *args[] = {"mkdir", "-p", root, NULL};
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        // In child process
        if (execvp("mkdir", args) == -1)
        {
            perror("mkdir: create container");
        }
    }
    int status;
    waitpid(pid, &status, 0);
}

// @brief Extracts the image from container.images_path/container.image_name.tar.gz
// @details create_container_root_and_id must be called before this function
void container_extract_image(struct Container *container)
{
    // Join container->images_path with container->image_name
    size_t img_path_length = strlen(container->images_path) + strlen(container->image_name) + 7;
    // +1 for '\0' character and +7 for adding extension '.tar.gz'
    char *image_path = safe_malloc(img_path_length + 1);
    strcpy(image_path, container->images_path);
    strcat(image_path, container->image_name);
    strcat(image_path, ".tar.gz");
    char *extract_args[] = {"tar", "xf", image_path, "-C", container->root, NULL};
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        // In child process
        if (execvp("tar", extract_args) == -1)
        {
            perror("tar: Error during extraction\n");
        }
        // TODO: If error, quit here
    }
    int status;
    waitpid(pid, &status, 0);
    free(image_path);
}

void container_delete(struct Container *container)
{
    printf("=> Removing container\n");
    char *rmargs[] = {"rm", "-rf", container->root, NULL};

    if (execvp("rm", rmargs) == -1)
    {
        perror("rmdir: Could not remove container");
    }
}

// Creates a mount point within the root of the container
void container_create_mountpoint(struct Container *container, const char *mpoint, mode_t mode,
                                 const char *fstype)
{
    static char path[PATH_MAX];
    int status = snprintf(path, PATH_MAX, "%s/%s", container->root, mpoint);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    // Note that the parent directory should be created before creating a child directory
    if (mkdir(path, mode) == -1)
    {
        fprintf(stderr, "Error creating directory %s: %s\n", path, strerror(errno));
        exit(1);
    }
    if (mount(NULL, path, fstype, 0, NULL) == -1)
    {
        fprintf(stderr, "Error mounting %s: %s\n", path, strerror(errno));
        exit(1);
    }
}

// Note: mode must be a combination of S_IF* with a permission such as 0755 using OR
void container_create_node(struct Container *container, const char *node_path, mode_t mode,
                           unsigned int devmajor, unsigned int devminor)
{
    static char path[PATH_MAX];
    int status = snprintf(path, PATH_MAX, "%s/%s", container->root, node_path);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    dev_t device = makedev(devmajor, devminor);
    // References:
    // https://man7.org/linux/man-pages/man7/inode.7.html
    // https://man7.org/linux/man-pages/man2/mknod.2.html
    // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/admin-guide/devices.txt
    if (mknod(path, mode, device) == -1)
    {
        fprintf(stderr, "Error creating node at %s: %s\n", path, strerror(errno));
        exit(1);
    }
}

// Creates a symlink from->to, if prefix_from is 1, the container's root path is prepended
// to from, and if prefix_to is 1, the container's root path is prepended to to.
void container_create_symlink(struct Container *container, int prefix_from, char *from,
                              int prefix_to, char *to)
{
    static char from_path[PATH_MAX];
    int status;

    if (prefix_from)
    {
        status = snprintf(from_path, PATH_MAX, "%s/%s", container->root, from);
    }
    else
    {
        status = snprintf(from_path, PATH_MAX, "%s", from);
    }
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }

    static char to_path[PATH_MAX];

    if (prefix_to)
    {
        status = snprintf(to_path, PATH_MAX, "%s/%s", container->root, to);
    }
    else
    {
        status = snprintf(to_path, PATH_MAX, "%s", to);
    }
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }

    if (symlink(from_path, to_path) == -1)
    {
        fprintf(stderr, "Error creating symlink between %s->%s\n", from_path, to_path);
    }
}

void container_create_mounts(struct Container *container)
{
    // Mount /proc, /sys and /dev
    container_create_mountpoint(container, "proc", 0555, "proc");
    container_create_mountpoint(container, "sys", 0555, "sysfs");
    container_create_mountpoint(container, "dev", 0755, "tmpfs");
    container_create_mountpoint(container, "dev/pts", 0755, "devpts");

    // Mount other devices in /dev
    container_create_node(container, "dev/urandom", S_IFCHR | 0666, 1, 9);
    container_create_node(container, "dev/random", S_IFCHR | 0666, 1, 8);
    container_create_node(container, "dev/zero", S_IFCHR | 0666, 1, 5);
    container_create_node(container, "dev/null", S_IFCHR | 0666, 1, 3);
    container_create_node(container, "dev/tty", S_IFCHR | 0666, 5, 0);
    container_create_node(container, "dev/console", S_IFCHR | 0620, 5, 1);
    container_create_node(container, "dev/ptmx", S_IFCHR | 0620, 5, 2);

    // Create symlinks such as /dev/stdin
    container_create_symlink(container, 0, "/proc/self/fd/0", 1, "dev/stdin");
    container_create_symlink(container, 0, "/proc/self/fd/1", 1, "dev/stdout");
    container_create_symlink(container, 0, "/proc/self/fd/2", 1, "dev/stderr");
    container_create_symlink(container, 0, "/proc/kcore", 1, "dev/kcore");
    container_create_symlink(container, 0, "/proc/fd", 1, "dev/fd");
}