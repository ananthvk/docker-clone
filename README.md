# docker-clone

## Purpose of this project
I wanted to explore linux namespaces and understand how docker works internally.
This project is a toy docker implementation in `C`

## Features
- An `overlayfs` is used to create containers from images, thereby saving time on extraction of the image.
- Uses `pivot_root` to change the root of the container
- Creates a new `UTS`, `PID` and `NET` namespace for the container
- A `veth` pair is used to connect the container to an existing `docker0` bridge

## Usage
First compile the application
```
$ gcc -O3 main.c run.c utils.c container.c -o container -std=gnu11
```
Then run the program with
```
$ sudo ./container run <image_name> <command>
```
Example:
```
$sudo ./container run ubuntu bash
```
Note: The image file must be present in the `images/` directory with the following structure `images/<image_name>.tar.gz`
The image file must be a compressed root filesystem
Get an alpine linux image from [here](https://alpinelinux.org/downloads/) and save it as `images/alpine.tar.gz`
For example, `images/ubuntu.tar.gz`
## TODOS
- Better handling of command line arguments
- Command to build, create, view and download containers and images
- Create a new bridge with routing instead of reusing `docker0`
- Configuration files
- `cgroups`, `setuid` and `seccomp`

## References
This project is based on [https://github.com/Fewbytes/rubber-docker](https://github.com/Fewbytes/rubber-docker), but is implemented in C.
