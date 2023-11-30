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
        errorMessage("%s\n", "mkdir() failed");
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

// @brief Creates a new directory in [container.containers_path] for the root of the container
// @details containers_path must be set before calling this function.
// Note this function also creates an ID for the container
void container_create(struct Container *container)
{
    char *id_buf = safe_malloc(container->id_length + 1);
    random_id(id_buf, container->id_length);
    id_buf[container->id_length] = '\0';

    char *root = safe_malloc(PATH_MAX);
    strformat(root, PATH_MAX, "%s/%s", container->containers_path, id_buf);

    // If a file/folder by the same name exists, keep generating random IDs till a unique name is
    // found
    while (exists(root))
    {
        random_id(id_buf, container->id_length);
        strformat(root, PATH_MAX, "%s/%s", container->containers_path, id_buf);
    }

    container->root = root;
    container->id = id_buf;

    char *args[] = {"mkdir", "-p", root, NULL};
    exec_command("mkdir", args);
}

void container_extract_image(struct Container *container)
{
    if (mkdir(container->cache_path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            perror("Could not create cache for container");
            exit(1);
        }
    }

    static char path[PATH_MAX];
    static char img_path[PATH_MAX];
    int status = snprintf(path, PATH_MAX, "%s/extracted", container->cache_path);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }

    if (mkdir(path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            perror("Could not create <cache>/extracted for container");
            exit(1);
        }
    }

    status = snprintf(img_path, PATH_MAX, "%s/extracted/%s", container->cache_path,
                      container->image_name);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }

    int extract_archive = 1;

    if (mkdir(img_path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            perror("Could not create <cache>/extracted/<image> for container");
            exit(1);
        }
        if (errno == EEXIST)
        {
            extract_archive = 0;
        }
    }

    char image_archive_path[PATH_MAX];
    status = snprintf(image_archive_path, PATH_MAX, "%s/%s.tar.gz", container->images_path,
                      container->image_name);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }

    if (extract_archive)
    {
        char *extract_args[] = {"tar", "xf", image_archive_path, "-C", img_path, NULL};
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
        waitpid(pid, &status, 0);
        printf("=> Extracted image to container");
    }
    else
    {
        printf("=> Image already present, not extracting\n");
    }

    // Create the overlayfs
    status = snprintf(path, PATH_MAX, "%s/changes", container->cache_path);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    if (mkdir(path, 0755) == -1)
    {
        if (errno != EEXIST)
        {
            perror("mkdir changes");
            exit(1);
        }
    }
    status = snprintf(path, PATH_MAX, "%s/changes/%s", container->cache_path, container->id);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    if (mkdir(path, 0755) == -1)
    {
        perror("mkdir changes/id");
        exit(1);
    }
    char upper_dir[PATH_MAX];
    status =
        snprintf(upper_dir, PATH_MAX, "%s/changes/%s/upper", container->cache_path, container->id);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    char work_dir[PATH_MAX];
    status =
        snprintf(work_dir, PATH_MAX, "%s/changes/%s/workdir", container->cache_path, container->id);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }

    if (mkdir(work_dir, 0755) == -1)
    {
        perror("mkdir work_dir");
        exit(1);
    }
    if (mkdir(upper_dir, 0755) == -1)
    {
        perror("mkdir upper_dir");
        exit(1);
    }
    char format_string[PATH_MAX * 5];
    status = snprintf(format_string, PATH_MAX * 5, "lowerdir=%s,upperdir=%s,workdir=%s", img_path,
                      upper_dir, work_dir);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    if (mount("overlay", container->root, "overlay", 0, format_string) == -1)
    {
        fprintf(stderr, "Error mounting overlay %s\n", strerror(errno));
        exit(1);
    }
}

void container_delete(struct Container *container)
{
    printf("=> Removing container\n");
    // Delete container
    char *rmargs[] = {"rm", "-rf", container->root, NULL};
    exec_command("rm", rmargs);

    char path[PATH_MAX];
    // Remove overlay directory
    strformat(path, PATH_MAX, "%schanges/%s", container->cache_path, container->id);
    rmargs[2] = path;
    exec_command("rm", rmargs);
}

// Creates a mount point within the root of the container
void container_create_mountpoint(struct Container *container, const char *mpoint, mode_t mode,
                                 const char *fstype, char *options)
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
        if (errno != EEXIST)
        {
            fprintf(stderr, "Error creating directory %s: %s\n", path, strerror(errno));
            exit(1);
        }
        else
        {
            fprintf(stderr, "WARNING %s exists\n", path);
        }
    }
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

    // Create overlayfs
    // https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt
}
