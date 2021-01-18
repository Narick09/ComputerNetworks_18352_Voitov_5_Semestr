#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <netinet/in.h>

#include "buffer.h"

#define PROT_CHECK_TRUE 0
#define PROT_CHECK_MAYBE 1
#define PROT_CHECK_FALSE 2

struct protocol {
    // Protocol name
    char *name;
    // Pointer to the function that check if given packet belong to protocol
    int (*check)(const struct buffer *buf);
};

struct target {
    struct protocol *protocol;
    struct sockaddr_in dst_address;
};

extern const int protocols_count;
extern struct protocol *protocols;
extern int targets_count;
extern struct target *targets;

int load_targets();

#endif
