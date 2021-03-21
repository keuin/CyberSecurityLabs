#ifndef __NFHC_H
#define __NFHC_H

#include "nfh.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <libgen.h>

fsm_context *client_new(char *host, u_int16_t port);
void client_delete(fsm_context *ctx);

#endif