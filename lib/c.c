#define __LIBRARY__
#include <unistd.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdarg.h>
#include <signal.h>

#define DIRLEN 16

_syscall3(int, read, int, fd, char *, buf, off_t, count)
_syscall3(int, lseek, int, fd, off_t, offset, int, origin)
_syscall2(int, stat, const char *, filename, struct stat *, stat_buf)
_syscall2(int, fstat, int, fd, struct stat *, stat_buf)
_syscall2(int, ustat, dev_t, dev, struct ustat *, ubuf)
_syscall2(int, lstat, const char *, filename, struct stat *, stat_buf)
_syscall1(int, uselib, const char *, filename)

typedef void __sighandler_t(int);
extern void ___sig_restore();
extern void ___masksig_restore();
sigset_t ___ssetmask(sigset_t mask) {
    long res;
    __asm__("int $0x80":"=a" (res)
        :"0" (__NR_ssetmask), "b" (mask));
    return res;
}

__sighandler_t *signal(int sig, __sighandler_t *handler) {
    __sighandler_t *res;
    __asm__("int $0x80":"=a" (res):
    "0" (__NR_signal), "b" (sig), "c" (handler), "d" ((long) ___sig_restore));
}

int sigaction(int sig, struct sigaction *sa, struct sigaction *old) {
    if (sa->sa_flags & SA_NOMASK)
        sa->sa_restorer = ___sig_restore;
    else
        sa->sa_restorer = ___masksig_restore;
    __asm__("int $0x80":"=a" (sig)
        :"0" (__NR_sigaction), "b" (sig), "c" (sa), "d" (old));
    if (sig >= 0)
        return 0;
    errno = -sig;
    return -1;
}

int sigsuspend(sigset_t *sigmask) {
    int res;
    register int __fooebx __asm__ ("bx") = 0;
    __asm__("int $0x80"
    :"=a"(res)
    :"0"(__NR_sigsuspend), "r"(__fooebx), "c"(0), "d"(*sigmask));
    if (res >= 0)
        return res;
    errno = -res;
    return -1;
}

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