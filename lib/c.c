#define __LIBRARY__
#include <unistd.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdarg.h>

#define DIRLEN 16

_syscall3(int, read, int, fd, char *, buf, off_t, count)
_syscall3(int, lseek, int, fd, off_t, offset, int, origin)
_syscall2(int, stat, const char *, filename, struct stat *, stat_buf)
_syscall2(int, fstat, int, fd, struct stat *, stat_buf)
_syscall2(int, ustat, dev_t, dev, struct ustat *, ubuf)
_syscall2(int, lstat, const char *, filename, struct stat *, stat_buf)

extern int vsprintf(char * buf, const char * fmt, va_list args);

static char printbuf[512];
static void printf(const char *fmt, ...) {
	va_list args;
	int i;

	va_start(args, fmt);
    i = vsprintf(printbuf, fmt, args);
	write(1, printbuf, i);
	va_end(args);
}

void list_file(const char *path) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        printf("open dir fail");
        return;
    }
    struct dir_entry *ptr;
    struct stat statbuf;
    int ret = fstat(fd, &statbuf);
    if (ret) {
        printf("fstat error");
        return;
    }
    printf("from fstat => size: %d\n", statbuf.st_size);
    ret = stat(path, &statbuf);
    if (ret) {
        printf("call stat error");
        return;
    }
    printf("from stat \t => size: %d\n", statbuf.st_size);
    ret = lstat(path, &statbuf);
    if (ret) {
        printf("call lstat error");
        return;
    }
    printf("from lstat \t => size: %d\n", statbuf.st_size);
    char buf[DIRLEN];
    int size = 0;
    int loc = 0;
    while (1) {
        if (size <= loc) {
            size = read(fd, buf, DIRLEN);
        }
        if (size <= 0) {
            printf("\n");
            return;
        }
        ptr = (struct dir_entry *) buf;
        loc += sizeof(*ptr);
        if (!ptr->inode) {
            printf("it is null#####\n");
            continue;
        }
        printf("%s ", ptr->name);
    }
    return;
}