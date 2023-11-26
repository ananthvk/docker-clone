#include "run.h"
#include "config.h"
#include "container.h"
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

void cmd_run(int argc, char *argv[])
{
    if (argc == 0)
    {
        printf("Usage: ./container run [options] image_name command [command options]\n");
        exit(1);
    }
    struct Container container;
    container.containers_path = CONTAINER_PATH;
    container.image_name = argv[0];
    container.images_path = IMAGE_PATH;
    container.id_length = CONTAINER_ID_LENGTH;
    printf("=> Creating container\n");
    create_container_root_and_id(&container);
    printf("=> Created container %s [%s] \n", container.image_name, container.id);
    extract_image_container(&container);
    printf("=> Extracted image to container\n");
    free(container.id);
    free(container.root);
}