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

    static unsigned char buf[4096];

    for (;;) {
        ssize_t nread = read(src_fd, buf, sizeof(buf));
        if (nread <= 0)
            break;

        (void)!write(STDERR_FILENO, "read ", 5);
        put_uint((unsigned int)nread);
        (void)!write(STDERR_FILENO, " bytes\n", 7);
    }

    return EXIT_SUCCESS;
}

static void put_uint(unsigned int n)
{
    char b[12];
    int i = (int)sizeof(b);
    do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n);
    (void)!write(STDERR_FILENO, &b[i], sizeof(b) - (size_t)i);
}

