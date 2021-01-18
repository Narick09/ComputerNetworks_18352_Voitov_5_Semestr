#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

#include "protocol.h"

int tls_check(const struct buffer *buf);
int http_check(const struct buffer *buf);
int ssh_check(const struct buffer *buf);

#endif
