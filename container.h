#ifndef CONTAINER_CONTAINER_H
#define CONTAINER_CONTAINER_H
struct Container{
    // ID of the container
    char *id;
    // Folder in which images are stored
    char *images_path;
    // image name (without extension) from which this container is created
    char *image_name;
    // The path to the root of this container
    char *root;
    // Folder in which containers are stored and created
    char *containers_path;
    // ID length
    int id_length;
};

void create_container_root_and_id(struct Container *container);

void extract_image_container(struct Container *container);
#endif // COTNAINER_CONTAINER_H