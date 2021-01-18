#include "protocol.h"

#include <string.h>

int http_check(const struct buffer *buf) {
    if (4 > buf->length)
        return PROT_CHECK_MAYBE;

    char *text = buf_get_ptr(buf);

    if (strncmp(text, "GET ", 4) == 0)
        return PROT_CHECK_TRUE;

    return PROT_CHECK_FALSE;
}