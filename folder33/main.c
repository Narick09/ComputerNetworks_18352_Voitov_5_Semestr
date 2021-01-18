#include <errno.h>
#include <signal.h> // sigaction
#include <sys/types.h> // Man recomends to include this header alongside with socket.h altough it is not required
#include <sys/socket.h> // socket, bind
#include <netinet/in.h> // sockaddr_in, in_port_t, in_addr
#include <arpa/inet.h> // inet_aton, inet_ntoa
#include <fcntl.h> // fcntl
#include <poll.h> // poll, pollfd, nfds_t
#include <unistd.h> // close
#include <stdio.h> // perror
#include <stdlib.h> // malloc
#include <string.h> // memset

#include "buffer.h"
#include "protocol.h"

// Maximal number of pending connections.
#define SOCK_BACKLOG 50
#define MAX_CLIENTS 510
#define BUFFER_SIZE 1024

struct client {
    int src_fd;
    int dst_fd;
    unsigned src_dst_active : 1;
    unsigned dst_src_active : 1;
    struct protocol *protocol;
    struct buffer src_dst_buf;
    struct buffer dst_src_buf;
    struct sockaddr_in src_address;
    struct sockaddr_in dst_address;
};

struct sockaddr_in listen_addr;
struct sockaddr_in dst_address;

int listening_socket;
int clients_count = 0; // Number of connections (i.e. in-out socket pairs, excluding main listening socket).
struct client *clients[MAX_CLIENTS];
struct pollfd fds[1 + 2 * MAX_CLIENTS];

#define src_ind(slot) (2 * slot + 1)
#define dst_ind(slot) (2 * slot + 2)

void copy_slot(int ind_from, int ind_to) {
    if (ind_to == ind_from)
        return;

    clients[ind_to] = clients[ind_from];
    fds[src_ind(ind_to)] = fds[src_ind(ind_from)];
    fds[dst_ind(ind_to)] = fds[dst_ind(ind_from)];
}

int can_add_slot() {
    return clients_count < MAX_CLIENTS;
}

int add_slot(struct client *client) {
    int slot = clients_count++;
    clients[slot] = client;
    fds[src_ind(slot)].fd = client->src_fd;
    fds[src_ind(slot)].events = POLLIN;
    fds[dst_ind(slot)].fd = client->dst_fd;
    fds[dst_ind(slot)].events = POLLIN;
    return slot;
}

void clear_slot(int slot) {
    clients[slot] = NULL;
}

void shrink_slots() {
    int i = 0;
    for (int j = 0; j < clients_count; j++) {
        if (clients[j] != NULL) {
            copy_slot(j, i);
            i++;
        }
    }
    clients_count = i;
}

struct client *make_client(const struct sockaddr_in *addr, int src_fd, int dst_fd) {
    struct client *client = malloc(sizeof(struct client));
    if (client == NULL) {
        return NULL;
    }

    if (buf_init(&client->dst_src_buf, BUFFER_SIZE) == -1) {
        free(client);
        return NULL;
    }
    if (buf_init(&client->src_dst_buf, BUFFER_SIZE) == -1) {
        free(client);
        buf_destroy(&client->dst_src_buf);
        return NULL;
    }

    client->src_address = *addr;
    memset(&client->dst_address, 0, sizeof(client->dst_address));
    client->protocol = NULL;
    client->src_fd = src_fd;
    client->dst_fd = dst_fd;
    client->src_dst_active = 1;
    client->dst_src_active = 1;

    return client;
}

void destroy_client(struct client *client) {
    buf_destroy(&client->src_dst_buf);
    buf_destroy(&client->dst_src_buf);
    free(client);
}

// Returns slot of created client if operation succeed.
// -2 is returned if no slots available
// -1 if system error occured.
int add_client(const struct sockaddr_in *addr, int src_fd, int dst_fd) {
    if (!can_add_slot()) {
        return -2;
    }

    struct client *c = make_client(addr, src_fd, dst_fd);
    if (c == NULL)
        return -1;

    return add_slot(c);
}

void remove_client(int slot) {
    struct client *client = clients[slot];
    clear_slot(slot);

    if (close(client->src_fd) == -1)
        perror("close");
    if (close(client->dst_fd) == -1)
        perror("close");

    destroy_client(client);
}

void accept_connection() {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int client_socket = accept(listening_socket, (struct sockaddr *) &addr, &addrlen);
    if (client_socket == -1) {
        // Ignore any errors, but warn about unexpected ones.
        switch (errno) {
            case EAGAIN:
            case EINTR: // Should never happen because after poll accept does not block.
            case ENETDOWN: // Manual recomends to treat following error codes as EAGAIN.
            case EPROTO:
            case ENOPROTOOPT:
            case EHOSTDOWN:
            case ENONET:
            case EHOSTUNREACH:
            case EOPNOTSUPP:
            case ENETUNREACH:
                // Just try again later.
                break;

            default:
                perror("WARNING accept");
                return;
        }
    }

    fprintf(stderr, "Connection from %s:%hu\n",
            inet_ntoa(addr.sin_addr),
            ntohs(addr.sin_port));

    if (!can_add_slot()) {
        // Should never hapen, because POLLIN flag for listening socket is not set
        // if we have MAX_CLIENTS connections.
        fprintf(stderr, "Connection limit exceded\n");
        goto close_socket;
    }

    int slot = add_client(&addr, client_socket, -1);
    if (slot == -1) {
        perror("add_client");
        goto close_socket;
    }

    return;

    close_socket:
    fprintf(stderr, "Closing incoming connection...\n");
    if (close(client_socket) == -1) {
        perror("close");
        return;
    }
    fprintf(stderr, "Connection aborted\n");
}

#define CAUSE_NONE 0
#define CAUSE_EOF 1
#define CAUSE_ERROR 2
int get_rw_error_cause(int res, int err) {
    if (res == -1) {
        switch (err) {
            case EAGAIN:
            case EINTR:
                // Ignore these errors.
                break;
            case ECONNRESET:
            case EPIPE:
                return CAUSE_EOF;
            default:
                return CAUSE_ERROR;
        }
    } else if (res == 0) {
        return CAUSE_EOF;
    }
    return CAUSE_NONE;
}

void try_shutdown(int fd, int dir) {
    if (fd < 0)
        fd = ~fd;

    if (shutdown(fd, dir) == -1 && errno != ENOTCONN)
        // ENOTCONN appears when remote host closes both directions
        perror("shutdown");
}

void clear_pollfd_flags(struct pollfd *fd, short mask) {
    if ((fd->events &= ~mask) == 0 && fd->fd >= 0) {
        // We don't want to receive POLLHUP after we shutdown reading from socket.
        fd->fd = ~fd->fd; // Negative fd values are ignored by pooll
    }
}

void set_pollfd_flags(struct pollfd *fd, short mask) {
    if (!mask)
        return;

    if (fd->fd < 0)
        fd->fd = ~fd->fd;

    fd->events |= mask;
}

int receive(
        struct pollfd *in_fd,
        struct pollfd *out_fd,
        struct buffer *buf) {
    if (in_fd->revents & POLLIN && !buf_full(buf)) {
        ssize_t res = buf_read(in_fd->fd, buf);
        switch (get_rw_error_cause(res, errno)) {
            case CAUSE_ERROR:
                perror("read");
                /* FALLTHROUGH */
            case CAUSE_EOF:
                clear_pollfd_flags(in_fd, POLLIN);
                clear_pollfd_flags(out_fd, POLLOUT);
                try_shutdown(out_fd->fd, SHUT_WR);
                return -1;
        }
    }
    return 0;
}

int transmit(
        struct pollfd *in_fd,
        struct pollfd *out_fd,
        struct buffer *buf) {
    if (out_fd->revents & POLLOUT && !buf_empty(buf)) {
        ssize_t res = buf_write(out_fd->fd, buf);
        switch (get_rw_error_cause(res, errno)) {
            case CAUSE_ERROR:
                perror("write");
                /* FALLTHROUGH */
            case CAUSE_EOF:
                clear_pollfd_flags(in_fd, POLLIN);
                clear_pollfd_flags(out_fd, POLLOUT);
                try_shutdown(in_fd->fd, SHUT_RD);
                return -1;
        }
    }
    return 0;
}

int transfer(
        struct pollfd *in_fd,
        struct pollfd *out_fd,
        struct buffer *buf) {

    if (receive(in_fd, out_fd, buf) == -1)
        return -1;
    if (transmit(in_fd, out_fd, buf) == -1)
        return -1;

    if (buf_empty(buf))
        clear_pollfd_flags(out_fd, POLLOUT);
    else
        set_pollfd_flags(out_fd, POLLOUT);

    return 0;
}

struct target *select_target(const struct buffer *buf, int *possible) {
    *possible = 0;
    for (int i = 0; i < targets_count; i++) {
        struct target *target = &targets[i];
        int check_res = target->protocol->check(buf);
        if (check_res == PROT_CHECK_TRUE)
            return target;
        if (check_res == PROT_CHECK_MAYBE)
            *possible = 1;
    }
    return NULL;
}

int connect_client_to(
        struct client *client,
        struct pollfd *dst_fd,
        struct target *target) {
    client->protocol = target->protocol;
    client->dst_address = target->dst_address;

    fprintf(stderr, "Creating tunnel from %s:%hu to %s:%hu (protocol: %s)\n",
            inet_ntoa(client->src_address.sin_addr),
            ntohs(client->src_address.sin_port),
            inet_ntoa(client->dst_address.sin_addr),
            ntohs(client->dst_address.sin_port),
            client->protocol->name);

    // Create and connect socket
    client->dst_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->dst_fd == -1) {
        perror("socket");
        return -1;
    }
    if (connect(client->dst_fd,
                (struct sockaddr *) &client->dst_address,
                sizeof(client->dst_address)) == -1) {
        perror("connect");
        if (close(client->dst_fd) == -1)
            perror("close");
        return -1;
    }
    // Register socket for poll
    dst_fd->fd = client->dst_fd;
    set_pollfd_flags(dst_fd, POLLOUT);

    return 0;
}

int identify_protocol(int slot) {
    struct pollfd *src_fd = &fds[src_ind(slot)];
    struct pollfd *dst_fd = &fds[dst_ind(slot)];
    struct client *client = clients[slot];

    if (receive(src_fd, dst_fd, &client->src_dst_buf))
        return -1;

    int possible;
    struct target *target = select_target(&client->src_dst_buf, &possible);

    if (target != NULL) {
        return connect_client_to(client, dst_fd, target);
    }
    if (!possible || buf_full(&client->src_dst_buf))
        return -1;
    return 0;
}

int parse_port(const char *str, in_port_t *res) {
    char *end;
    errno = 0;
    long r = strtol(str, &end, 10);
    if (r < 0 || r > 65535)
        return -1;
    *res = (in_port_t) r;
    return 0;
}

void print_usage_and_exit(const char *name) {
    fprintf(stderr, "USAGE %s <listen-port>\n", name);
    exit(1);
}

void parse_args(int argc, char *const argv[]) {
    if (argc != 2) {
        print_usage_and_exit(argv[0]);
    }

    in_port_t listen_port;
    if (parse_port(argv[1], &listen_port) == -1) {
        fprintf(stderr, "Invalid listening port value\n");
        print_usage_and_exit(argv[0]);
    }

    struct in_addr listen_ip_addr;
    listen_ip_addr.s_addr = htonl(INADDR_ANY);

    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr = listen_ip_addr;
}

volatile int term_signal_received = 0;
void termination_signal_handler(int sig) {
    term_signal_received = 1;
}

void setup_signals() {
    struct sigaction action;
    action.sa_handler = termination_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);

    action.sa_handler = SIG_IGN;

    sigaction(SIGPIPE, &action, NULL);
}

void close_listening_socket() {
    if (close(listening_socket) == -1)
        perror("close");
}

int start_listening_socket() {
    listening_socket = socket(AF_INET,  SOCK_STREAM, 0);
    if (listening_socket == -1) {
        perror("Create listening socket");
        return 1;
    }

    if (bind(listening_socket,
             (struct sockaddr *) &listen_addr,
             sizeof(listen_addr)) == -1) {
        perror("Bind listening socket");
        close_listening_socket();
        return 1;
    }

    if (listen(listening_socket, SOCK_BACKLOG) == -1) {
        perror("Cannot start listening");
        close_listening_socket();
        return 1;
    }

    return 0;
}

void prepare_poll_structures() {
    memset(fds, 0, sizeof(fds));

    // Setup listenig socket events.
    fds[0].fd = listening_socket;
}

int main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    setup_signals();
    if (load_targets() == -1) {
        fprintf(stderr, "Cannot read configuration\n");
        return 1;
    }

    int start_res = start_listening_socket();
    if (start_res != 0)
        return start_res;

    prepare_poll_structures();

    while (!term_signal_received) {
        if (clients_count < MAX_CLIENTS)
            fds[0].events = POLLIN;
        else
            fds[0].events = 0;

        nfds_t nfds = 1 + 2 * clients_count;
        int cnt = poll(fds, nfds, -1);
        if (cnt == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        int saved_clients_count = clients_count;

        // Accept connections.
        if (fds[0].revents & POLLIN) {
            accept_connection();
        }

        for (int i = 0; i < saved_clients_count; i++) {
            struct pollfd *src_fd = &fds[src_ind(i)];
            struct pollfd *dst_fd = &fds[dst_ind(i)];
            struct client *client = clients[i];

            int closing = 0;
            if (client->protocol == NULL) {
                if (identify_protocol(i) == -1)
                    closing = 1;
            } else {
                if (client->src_dst_active) {
                    int res = transfer(src_fd, dst_fd, &client->src_dst_buf);

                    if (res == -1)
                        client->src_dst_active = 0;
                }
                if (client->dst_src_active) {
                    int res = transfer(dst_fd, src_fd, &client->dst_src_buf);

                    if (res == -1)
                        client->dst_src_active = 0;
                }
                closing = !client->src_dst_active && !client->dst_src_active;
            }
            if (closing) {
                fprintf(stderr, "Disconnecting %s:%hu...\n",
                        inet_ntoa(client->src_address.sin_addr),
                        ntohs(client->src_address.sin_port));

                remove_client(i);
            }
        }
        shrink_slots();
    }

    if (term_signal_received)
        fprintf(stderr, "Termination signal received\n");

    fprintf(stderr, "Terminating connections\n");
    for (int i = 0; i < clients_count; i++) {
        remove_client(i);
    }

    fprintf(stderr, "Closing listening socket\n");
    close_listening_socket();

    return 0;
}
