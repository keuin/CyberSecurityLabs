#ifndef __NFHS_H
#define __NFHS_H

#include "nfh.h"
#include "util.h"
#include <dirent.h>
#include <unistd.h>

fsm_context *server_new(char *host, u_int16_t port);
void server_delete(fsm_context *ctx);

#endif