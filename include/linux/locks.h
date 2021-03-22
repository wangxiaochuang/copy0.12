#ifndef _LINUX_LOCKS_H
#define _LINUX_LOCKS_H

extern void __wait_on_buffer(struct buffer_head *);

static inline void wait_on_buffer(struct buffer_head * bh) {
    if (bh->b_lock)
        __wait_on_buffer(bh);
}

static inline void lock_buffer(struct buffer_head * bh) {

}

static inline void unlock_buffer(struct buffer_head * bh) {
    
}

#endif