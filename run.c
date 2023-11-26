#include "run.h"
#include "config.h"
#include "string.h"
#include "utils.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * @short Parses the passed arguments and runs the specified image in a new container.
 * @param argc number of arguments after run subcommand
 * @param argv arguments after the run subcommand, null terminated
 */

void container_run(int argc, char *argv[])
{
    if (argc == 0)
    {
        printf("Usage: ./container run [options] image_name command [command options]\n");
        exit(1);
    }
    // We have gotten the image name, now extract the image into the containers directory with a
    // unique id If the directories do not exist, create them.
    const char *image_name = argv[0];
    char *container_id = safe_malloc(CONTAINER_ID_LENGTH + 1);
    container_id[CONTAINER_ID_LENGTH] = '\0';
    random_id(container_id, CONTAINER_ID_LENGTH);
    char *container_root = safe_malloc(strlen(CONTAINER_PATH) + CONTAINER_ID_LENGTH + 1);
    container_root[0] = '\0';
    strcat(container_root, CONTAINER_PATH);
    strcat(container_root, container_id);

    while (exists(container_root))
    {
        container_id = random_id(container_id, CONTAINER_ID_LENGTH);
        strcat(container_root, CONTAINER_PATH);
        strcat(container_root, container_id);
    }

    printf("=> Creating container %s [%s] \n", image_name, container_id);
    if (mkdir(container_root, 0770) == -1) // Mode 770, full access to owner and group, others can't access
    {
        perror("mkdir could not create directory for container check if " CONTAINER_PATH " exists");
        exit(1);
    }

    char *image_path = safe_malloc(strlen(IMAGE_PATH) + strlen(image_name) + 1 + 7); // +1 for null char, +7 for .tar.gz
    image_path[0] = '\0';
    strcat(image_path, IMAGE_PATH);
    strcat(image_path, image_name);
    strcat(image_path, ".tar.gz");

    // Extract the contents of image to this root directory
    char *extract_args[] = {"tar", "xf", image_path, "-C", container_root,  NULL};
    execvp("tar", extract_args);
    free(container_id);
    free(image_path);
    free(container_root);
}