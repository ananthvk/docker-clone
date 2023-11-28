#include "container.h"
#include "utils.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
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

void container_create_mounts(struct Container *container)
{
    char path[PATH_MAX];
    // Mount /proc
    int status = snprintf(path, PATH_MAX, "%s/proc", container->root);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    if (mkdir(path, 0755) == -1)
    {
        perror("Error creating path for /proc");
        exit(1);
    }
    if (mount(NULL, path, "proc", 0, NULL) == -1)
    {
        perror("mount proc");
        exit(1);
    }

    // Mount /sys
    status = snprintf(path, PATH_MAX, "%s/sys", container->root);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    if (mkdir(path, 0775) == -1)
    {
        perror("Error creating path for /sys");
        exit(1);
    }
    if (mount(NULL, path, "sysfs", 0, NULL) == -1)
    {
        perror("mount sys");
        exit(1);
    }

    // Mount /dev
    status = snprintf(path, PATH_MAX, "%s/dev", container->root);
    if (status < 0 || status >= PATH_MAX)
    {
        fprintf(stderr, "Either the path was too large or snprintf failed.\n");
        exit(1);
    }
    if (mkdir(path, 0755) == -1)
    {
        perror("Error creating path for /dev");
        exit(1);
    }
    if (mount(NULL, path, "tmpfs", 0, NULL) == -1)
    {
        perror("mount dev");
        exit(1);
    }
}