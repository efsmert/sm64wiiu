#ifndef THREAD_H
#define THREAD_H

#include <stddef.h>

enum ThreadState {
    INVALID = 0,
    STOPPED = 1,
    RUNNING = 2,
};

struct ThreadHandle {
    int state;
};

int init_thread_handle(struct ThreadHandle *handle, void *(*entry)(void *), void *arg, void *sp, size_t sp_size);
void cleanup_thread_handle(struct ThreadHandle *handle);
int join_thread(struct ThreadHandle *handle);

#endif
