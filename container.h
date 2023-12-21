#ifndef CONTAINER_CONTAINER_H
#define CONTAINER_CONTAINER_H
struct Container{
    // ID of the container
    char *id;
    // Folder in which images are stored
    char *images_path;
    // image name (without extension) from which this container is created
    char *image_name;
    // Folder in which containers are stored and created
    char *containers_path;
    // ID length
    int id_length;
    
    // The directory in which files related to this container are stored
    char *container_dir;
    // The directory which acts as lowerdir for overlayfs
    char *image_path;
    // The path to the root of this container
    char *root;
};

void container_create(struct Container *container);
void container_extract_image(struct Container *container);
void container_create_overlayfs(struct Container *container);
void container_create_mounts(struct Container *container);
void container_delete(struct Container *container);
void container_connect_to_bridge(struct Container *container, int pid);
#endif // COTNAINER_CONTAINER_H