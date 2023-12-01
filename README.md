# docker-clone
I was learning about docker and wanted to know how it works internally.
This is a toy docker implementation to learn more about docker

# References
I am following the workshop from
[https://github.com/Fewbytes/rubber-docker](https://github.com/Fewbytes/rubber-docker)
but instead of using python, I am going to do it in `C`

# Here is an outline of the process
1. fork()
2. The main process only waits for the forked child to finish. All details here are in the child process.
3. Extract the image from an archive to a directory with a random id (container id)
4. unshare() to create a new namespace.
5. Create a new mount for root and mount it as private.
6. Mount `/proc` and `/sys`. Create a `tmpfs` for `/dev`.
7. Mount `/dev/pts` as `devpts` file system, `/dev/stdin`, `/dev/stdout` and `/dev/stderr`
8. Mount other necessary devices in `/dev` such as zero, urandom, etc
9. chroot() into the container root.
10. chdir() to `/`
11. execvp() the new process.

# The directory structure of the container
## Config
`containers_path` - Path in which to store temporary containers and extracted images
`images_path` - Path in which image files (as tar.gz) are stored
## Created by program
`<containers_path>/__extracted/<image_name>` - Extracted image files are stored here
`<containers_path>/<container_id>/root` - New root of the container (`merged` directory)
`<containers_path>/<container_id>/work` - `workdir` of the overlay
`<containers_path>/<container_id>/diff` - Place modifications by the overlay are stored (`upperdir`)