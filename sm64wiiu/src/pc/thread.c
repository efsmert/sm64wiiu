#include "thread.h"

int init_thread_handle(struct ThreadHandle *handle, void *(*entry)(void *), void *arg, void *sp, size_t sp_size) {
    (void)sp;
    (void)sp_size;
    if (handle == NULL) {
        return -1;
    }
    handle->state = RUNNING;
    if (entry != NULL) {
        entry(arg);
    }
    handle->state = STOPPED;
    return 0;
}

void cleanup_thread_handle(struct ThreadHandle *handle) {
    if (handle != NULL) {
        handle->state = STOPPED;
    }
}

int join_thread(struct ThreadHandle *handle) {
    if (handle == NULL) {
        return -1;
    }
    handle->state = STOPPED;
    return 0;
}
