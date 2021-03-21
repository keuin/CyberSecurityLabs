/***********************************
 *  NFH Server CUI Implementation  *
 ***********************************/

#include "nfhs.h"

int main(int argc, char **argv)
{
    setbuf(stdout, 0);
    DEBUGS(fprintf(stderr, "**** DEBUG OUTPUT IS ENABLED ****\n"));

    char *host = "0.0.0.0";
    u_int16_t port = 3789;
    
    // server [host] [port]
    if (argc == 3)
    {
        host = argv[2];
        port = atoi(argv[3]);
        if (!port)
            port = 3789;
    }

    printf("Starting server on %s:%hd...", host, port);

    fsm_context *ctx = server_new(host, port);
    if (!ctx)
    {
        fprintf(stderr, "Cannot spawn server instance.\n");
        return -1;
    }

    // main loop
    int failed = ctx->vf_fsm(ctx);

    server_delete(ctx);
    return failed;
}