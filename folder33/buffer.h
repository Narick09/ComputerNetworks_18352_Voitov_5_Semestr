#ifndef ROUND_BUFFER_H
#define ROUND_BUFFER_H

#include <sys/types.h> // ssize_t

struct buffer {
    size_t offset;
    size_t length;
    size_t capacity;
    void *buffer;
};

int buf_init(struct buffer *buf, size_t capacity);
void buf_destroy(const struct buffer *buf);

int buf_full(const struct buffer *buf);
int buf_empty(const struct buffer *buf);

void *buf_get_ptr(const struct buffer *buf);

ssize_t buf_write(int fd, struct buffer *buf);
ssize_t buf_read(int fd, struct buffer *buf);

#endif
