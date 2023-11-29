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
    char *rmargs[] = {"rm", "-rf", container->root, NULL};

    pid_t pid = fork();
    if (pid == 0)
    {
        if (execvp("rm", rmargs) == -1)
        {
            perror("rmdir: Could not remove container");
        }
    }

    char path[PATH_MAX];
    // Remove overlay directory
    snprintf(path, PATH_MAX, "%schanges/%s", container->cache_path, container->id);
    rmargs[2] = path;
    printf("=> Deleting %s\n", path);
    pid = fork();
    if (pid == 0)
    {
        if (execvp("rm", rmargs) == -1)
        {
            perror("rmdir: Could not remove cache");
        }
    }
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
