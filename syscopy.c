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

    close(src_fd);
    return EXIT_SUCCESS;
}
