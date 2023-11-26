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
