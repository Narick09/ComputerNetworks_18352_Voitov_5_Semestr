#include "protocol.h"
#include "protocol_defs.h"

#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

const int protocols_count = 3;
struct protocol protocols_arr[] = {
        { "TLS", tls_check },
        { "HTTP", http_check },
        { "SSH", ssh_check }
};
struct protocol *protocols = protocols_arr;

struct target_info {
    char *name;
    char *hostname;
    char *port;
};

int targets_count = 3;
struct target_info target_info[] = {
        { "TLS", "www.google.com", "443" },
        { "HTTP", "localhost", "631" },
        { "SSH", "localhost", "22" }
};
struct target targets_arr[3], *targets = targets_arr;

struct protocol *get_protocol_by_name(char *name) {
    for (int i = 0; i < protocols_count; i++) {
        if (strcmp(name, protocols[i].name) == 0)
            return &protocols[i];
    }
    return NULL;
}

int get_addr(char *hostname, char *port, struct sockaddr_in *addr) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addrinfo;
    int err = getaddrinfo(hostname, port, &hints, &addrinfo);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s]n", gai_strerror(err));
        return -1;
    }

    memcpy(addr, addrinfo->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(addrinfo);
    return 0;
}

int load_targets() {
    for (int i = 0; i < targets_count; i++) {
        struct target_info *ti = &target_info[i];
        struct protocol *protocol = get_protocol_by_name(ti->name);
        if (protocol == NULL)
            return -1;

        struct sockaddr_in addr;
        if (get_addr(ti->hostname, ti->port, &addr) == -1)
            return -1;

        targets[i].protocol = protocol;
        targets[i].dst_address = addr;
    }
    return 0;
}