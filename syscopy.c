#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

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

        ssize_t nw = write(dst_fd, buf, (size_t)nread);
	if (nw < 0)
		break;

        if (nread < 0) {
            if (errno == EINTR)
                continue;
            goto fail;
        }

    }

    return EXIT_SUCCESS;
}

static void put_uint(unsigned int n)
{
    char b[12];
    int i = (int)sizeof(b);
    do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n);
    

    ssize_t off = 0; 
    while (off < nread) {
        ssize_t nw = write(dst_fd, buf + off, (size_t)(nread - off));
        if (nw < 0) {
            if (errno == EINTR)
		    continue;
            goto fail;
	}
        off += nw;
    }

}

