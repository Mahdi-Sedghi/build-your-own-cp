#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
        return EXIT_FAILURE;

    int src_fd = open(argv[1], O_RDONLY);
    if (src_fd < 0)
        return EXIT_FAILURE;

    int dst_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        close(src_fd);
        return EXIT_FAILURE;
    }
    close(dst_fd);

    close(src_fd);
    return EXIT_SUCCESS;
}

