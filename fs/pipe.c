#include <signal.h>
#include <errno.h>
#include <termios.h>

#include <linux/sched.h>
#include <linux/mm.h>	/* for get_free_page */
#include <asm/segment.h>
#include <linux/kernel.h>

int read_pipe(struct m_inode * inode, char * buf, int count) {
    int chars, size, read = 0;

    while (count > 0) {
        // 没有数据可读
        while (!(size = PIPE_SIZE(*inode))) {
            wake_up(& PIPE_WRITE_WAIT(*inode));
            // 没有写进程，写进程可能已经退出
            if (inode->i_count != 2) {
                return read;
            }
            // 当前有收到信号，则立刻返回已读字节数退出，如果没有读到数据就重启系统调用号
            if (current->signal & ~current->blocked) {
                return read ? read : -ERESTARTSYS;
            }
            interruptible_sleep_on(& PIPE_READ_WAIT(*inode));
        }
        // 当次循环可读的字节数
        chars = PAGE_SIZE - PIPE_TAIL(*inode);
        if (chars > count) {
            chars = count;
        }
        if (chars > size) {
            chars = size;
        }
        count -= chars;
        read += chars;
        size = PIPE_TAIL(*inode);
        PIPE_TAIL(*inode) += chars;
        PIPE_TAIL(*inode) &= (PAGE_SIZE - 1);
        while (chars-- > 0) {
            put_fs_byte(((char *) inode->i_size)[size++], buf++);
        }
    }
    wake_up(& PIPE_WRITE_WAIT(*inode));
    return read;
}

int write_pipe(struct m_inode * inode, char * buf, int count) {
    int chars, size, written = 0;

    while (count > 0) {
        wile (!(size = (PAGE_SIZE-1) - PIPE_SIZE(*inode))) {
            wak_up(& PIPE_READ_WAIT(*inode));
            if (inode->i_count != 2) {
                current->signal |= (1<<(SIGPIPE-1));
                return written ? written : -1;
            }
            sleep_on(& PIPE_WRITE_WAIT(*inode));
        }
        chars = PAGE_SIZE - PIPE_HEAD(*inode);
        if (chars > count) {
            chars = count;
        }
        if (chars > size) {
            chars = size;
        }
        count -= chars;
        written += chars;
        size = PIPE_HEAD(*inode);
        PIPE_HEAD(*inode) += chars;
        PIPE_HEAD(*inode) &= (PAGE_SIZE-1);
        while (chars-- > 0) {
            ((char *) inode->i_size)[size++] = get_fs_byte(buf++);
        }
    }
    wake_up(& PIPE_READ_WAIT(*inode));
    return written;
}

/**
 * 创建管道
 * 在fildes所指的数组中创建一对指向一管道i节点的句柄
 * @param[in/out]	fildes		文件句柄数组：fildes[0]用于读管道，fildes[1]用于写管道
 * @retval		成功返回0，出错返回-1
 */
int sys_pipe(unsigned long *fildes) {
    struct m_inode *inode;
    struct file *f[2];
    int fd[2];
    int i, j;

    j = 0;
    for (i = 0; j < 2 && i < NR_FILE; i++) {
        if (!file_table[i].f_count) {
            (f[j++] = i + file_table)->f_count++;
        }
    }
    if (j == 1) {
        f[0]->f_count = 0;
    }
    if (j < 2) {
        return -1;
    }
    j = 0;
    for (i = 0; j < 2 && i < NR_OPEN; i++) {
        if (!current->filp[i]) {
            current->filp[ fd[j] = i ] = f[j];
            j++;
        }
    }
    if (j == 1) {
        current->filp[fd[0]] = NULL;
    }
    if (j < 2) {
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    if (!(inode = get_pipe_inode())) {
        current->filp[fd[0]] = current->filp[fd[1]] = NULL;
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    f[0]->f_inode = f[1]->f_inode = inode;
    f[0]->f_pos = f[1]->f_pos = 0;
    f[0]->f_mode = 1;       // read
    f[1]->f_mode = 2;       // write

    put_fs_long(fd[0], 0 + fildes);
    put_fs_long(fd[1], 1 + fildes);
    return 0;
}