#include "buffer.h"

#include <unistd.h> // read, write
#include <stdlib.h> // malloc

int buf_init(struct buffer *buf, size_t capacity) {
    void *ptr = malloc(capacity);
    if (ptr == NULL)
        return -1;

    buf->offset = 0;
    buf->length = 0;
    buf->capacity = capacity;
    buf->buffer = ptr;

    return 0;
}

void buf_destroy(const struct buffer *buf) {
    free(buf->buffer);
}

int buf_full(const struct buffer *buf) {
    return buf->offset + buf->length == buf->capacity;
}

int buf_empty(const struct buffer *buf) {
    return buf->length == 0;
}

void *buf_get_ptr(const struct buffer *buf) {
    return buf->buffer + buf->offset;
}

ssize_t buf_write(int fd, struct buffer *buf) {
    if (buf_empty(buf))
        return -2;

    void *ptr = buf->buffer + buf->offset;
    size_t size = buf->length;
    ssize_t res = write(fd, ptr, size);
    if (res != -1) {
        if (res == buf->length) {
            buf->length = 0;
            buf->offset = 0;
        } else {
            buf->length -= res;
            buf->offset = buf->offset + res;
        }
    }

    return res;
}

ssize_t buf_read(int fd, struct buffer *buf) {
    if (buf_full(buf))
        return -2;

    void *ptr = buf->buffer + buf->offset + buf->length;
    size_t size = buf->capacity - buf->offset - buf->length;
    ssize_t res = read(fd, ptr, size);
    if (res != -1)
        buf->length += res;

    return res;
}