/***********************************
 *  NFH Client CUI Implementation  *
 ***********************************/

#include "nfhc.h"
#include "util.h"

int main()
{
    setbuf(stdout, 0);
    DEBUGS(fprintf(stderr, "**** DEBUG OUTPUT IS ENABLED ****\n"));
    // Initialize an empty FSM instance,
    // then start a new connection to specified NFH server.
    // Interact with user via standard I/O.

    char host[64];
    uint16_t port;

    printf("Host:");
    while (scanf("%63s", host) != 1)
        ;

    printf("Port:");
    while (scanf("%hd", &port) != 1)
        ;

    fsm_context *ctx = client_new(host, port);
    if (!ctx)
    {
        fprintf(stderr, "Cannot spawn client instance.\n");
        return -1;
    }

    // main loop
    int failed = ctx->vf_fsm(ctx);

    client_delete(ctx);
    return failed;
}