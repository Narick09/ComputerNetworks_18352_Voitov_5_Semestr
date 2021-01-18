#include "protocol.h"

#include <stdint.h>
#include <arpa/inet.h>

struct version {
    uint8_t major;
    uint8_t minor;
};

struct tls_record_header {
    uint8_t type;
    struct version version;
    uint16_t length;
};

int tls_check(const struct buffer *buf) {
    if (sizeof(struct tls_record_header) > buf->length)
        return PROT_CHECK_MAYBE;
    struct tls_record_header *hdr = buf_get_ptr(buf);

    uint16_t len = ntohs(hdr->length);
    if (hdr->type == 22 &&
        hdr->version.major == 3 &&
        hdr->version.minor == 3 &&
        len > 50 && len <= 1024)
        return PROT_CHECK_TRUE;

    return PROT_CHECK_FALSE;
}
