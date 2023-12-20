#include "container.h"
#include "utils.h"
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

// Creates a directory using the mkdir() system call
// prefix is a path to add before the directory, like container root location
// if prefix is null, or empty, no prefix is added
void create_directory(const char *prefix, const char *path, mode_t mode)
{
    static char buffer[PATH_MAX];
    if (prefix == NULL || prefix[0] == '\0')
    {
        strformat(buffer, PATH_MAX, "%s", path);
    }
    else
    {
        strformat(buffer, PATH_MAX, "%s/%s", prefix, path);
    }
    if (mkdir(buffer, mode) == -1)
    {
        errorMessage("%s%s\n", "mkdir() failed to create ", buffer);
    }
}

// Same as create_directory, but does not fail if the directory exists
void create_directory_exists_ok(const char *prefix, const char *path, mode_t mode)
{
    static char buffer[PATH_MAX];
    if (prefix == NULL || prefix[0] == '\0')
    {
        strformat(buffer, PATH_MAX, "%s", path);
    }
    else
    {
        strformat(buffer, PATH_MAX, "%s/%s", prefix, path);
    }
    if (mkdir(buffer, mode) == -1)
    {
        if (errno == EEXIST)
            return;
        errorMessage("%s\n", "mkdir() failed");
    }
}

// Creates the parents of the directory if they do not exist
// Does not modify the mode of the parent and does not error if exists
// behaves like mkdir -p
void create_directory_parents(const char *prefix, const char *path, mode_t mode)
{
    static char buffer[PATH_MAX];
    if (prefix == NULL || prefix[0] == '\0')
    {
        strformat(buffer, PATH_MAX, "%s", path);
    }
    else
    {
        strformat(buffer, PATH_MAX, "%s/%s", prefix, path);
    }
    // TODO
    printf("Not implemented");
    exit(3);
}

// @brief Creates a new directory in [container.containers_path] for this container
// @details containers_path must be set before calling this function.
// Note this function also creates an ID for the container
void container_create(struct Container *container)
{
    char *id_buf = safe_malloc(container->id_length + 1);
    random_id(id_buf, container->id_length);
    id_buf[container->id_length] = '\0';

    char *container_dir = safe_malloc(PATH_MAX);
    strformat(container_dir, PATH_MAX, "%s/%s", container->containers_path, id_buf);

    // If a file/folder by the same name exists, keep generating random IDs till a unique name is
    // found
    while (exists(container_dir))
    {
        random_id(id_buf, container->id_length);
        id_buf[container->id_length] = '\0';
        strformat(container_dir, PATH_MAX, "%s/%s", container->containers_path, id_buf);
    }
    char *args[] = {"mkdir", "-p", container_dir, NULL};
    exec_command("mkdir", args);
    container->container_dir = container_dir;
    container->id = id_buf;
}

// Extracts the image if it is being used for the first time
void container_extract_image(struct Container *container)
{
    char *path = safe_malloc(PATH_MAX);
    strformat(path, PATH_MAX, "%s/__extracted/%s", container->containers_path,
              container->image_name);
    container->image_path = path;
    if (exists(path))
    {
        printf("=> Found existing image cache, not extracting\n");
    }
    else
    {
        printf("=> %s is being used for the first time ... extracting\n", container->image_name);
        create_directory_exists_ok(container->containers_path, "__extracted", 0755);
        create_directory(NULL, path, 0755);
        char image_archive_path[PATH_MAX];
        strformat(image_archive_path, PATH_MAX, "%s/%s.tar.gz", container->images_path,
                  container->image_name);
        char *tar_args[] = {"tar", "xf", image_archive_path, "-C", path, NULL};
        exec_command("tar", tar_args);
    }
}

void container_create_overlayfs(struct Container *container)
{
    // Create overlayfs
    // https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt
    char *workdir = safe_malloc(PATH_MAX);
    char *rootdir = safe_malloc(PATH_MAX);
    char *diffdir = safe_malloc(PATH_MAX);

    strformat(workdir, PATH_MAX, "%s/work", container->container_dir, container->id);
    strformat(rootdir, PATH_MAX, "%s/root", container->container_dir, container->id);
    strformat(diffdir, PATH_MAX, "%s/diff", container->container_dir, container->id);

    create_directory(NULL, workdir, 0755);
    create_directory(NULL, rootdir, 0755);
    create_directory(NULL, diffdir, 0755);

    char *opts_string = safe_malloc(PATH_MAX * 4);
    strformat(opts_string, PATH_MAX * 4, "lowerdir=%s,upperdir=%s,workdir=%s",
              container->image_path, diffdir, workdir);
    if (mount("overlay", rootdir, "overlay", 0, opts_string) == -1)
    {
        fprintf(stderr, "Error mounting overlay %s\n", strerror(errno));
        exit(1);
    }
    container->root = rootdir;
    free(opts_string);
    free(diffdir);
    free(workdir);
}

void container_delete(struct Container *container)
{
    // Create a temporary namespace first
    // strformat(exec_args, EARGS_MAX, "link delete vb%s", container->id);
    // exec_command_fail_ok("ip", exec_args);
    // strformat(exec_args, EARGS_MAX, "netns delete ns%s", container->id);
    // exec_command_fail_ok("ip", exec_args);
    printf("Run the following commands for cleanup (as root)\n");
    printf("========================\n");
    printf("ip link delete vb%s\n", container->id);
    printf("ip netns delete ns%s\n", container->id);

    printf("=> Removing container\n");
    // Delete container
    char *rmargs[] = {"rm", "-rf", container->container_dir, NULL};
    exec_command_fail_ok("rm", rmargs);
}

// Creates a mount point within the root of the container
void container_create_mountpoint(struct Container *container, const char *mpoint, mode_t mode,
                                 const char *fstype, char *options)
{
    static char path[PATH_MAX];
    strformat(path, PATH_MAX, "%s/%s", container->root, mpoint);
    create_directory_exists_ok(NULL, path, mode);
    if (mount(NULL, path, fstype, 0, options) == -1)
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
    static char to_path[PATH_MAX];

    if (prefix_from)
    {
        strformat(from_path, PATH_MAX, "%s/%s", container->root, from);
    }
    else
    {
        strformat(from_path, PATH_MAX, "%s", from);
    }

    if (prefix_to)
    {
        strformat(to_path, PATH_MAX, "%s/%s", container->root, to);
    }
    else
    {
        strformat(to_path, PATH_MAX, "%s", to);
    }
    if (symlink(from_path, to_path) == -1)
    {
        fprintf(stderr, "Error creating symlink between %s->%s\n", from_path, to_path);
    }
}

void container_create_mounts(struct Container *container)
{
    // Mount /proc, /sys and /dev
    container_create_mountpoint(container, "proc", 0555, "proc", NULL);
    container_create_mountpoint(container, "sys", 0555, "sysfs", NULL);
    container_create_mountpoint(container, "dev", 0755, "tmpfs", NULL);
    container_create_mountpoint(container, "dev/pts", 0755, "devpts", NULL);

    // Mount other devices in /dev
    container_create_node(container, "dev/urandom", S_IFCHR | 0666, 1, 9);
    container_create_node(container, "dev/random", S_IFCHR | 0666, 1, 8);
    container_create_node(container, "dev/full", S_IFCHR | 0666, 1, 7);
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
    // TODO:
    // Add the following to /dev
    // drwxrwxrwt 1777 mqueue
    // drwxrwxrwt 1777 shm
}
