#ifndef CONTAINER_CONFIG_H
#define CONTAINER_CONFIG_H
// path to store created containers
#define CONTAINER_PATH "containers"
// path which contains the images
#define IMAGE_PATH "images"
#define CONTAINER_ID_LENGTH 10
#define STACK_SIZE 8*1024*1024 // 8MiB of stack size for child process
#define ARG_MAX_LEN 4096
#define BRIDGE_NAME "docker0"
#define BRIDGE_GATEWAY "172.17.0.1"
#define CONTAINER_IP "172.17.0.8/16"
#endif
