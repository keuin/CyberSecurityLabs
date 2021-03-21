#include "nfh.h"
#include "util.h"

// private methods
static int __vf_is_accepted_state(fsm_context *ctx);
static int __vf_fsm(fsm_context *ctx);

/**
 * @brief Create a base FSM instance in heap.
 * 
 * @param host the host.
 * @param port the port.
 * @return fsm_context* pointer to the object. If failed, return NULL.
 */
fsm_context *new_fsm_context(char *host, uint16_t port)
{
    // init memory space
    fsm_context *p = malloc(sizeof(fsm_context));
    memset(p, 0, sizeof(fsm_context));
    // init members
    p->state = FSM_INIT;
    int host_len = strlen(host);
    char *buf_host = malloc(host_len + 1);
    if (!buf_host)
    {
        free(p);
        return NULL;
    }
    memcpy(buf_host, host, host_len + 1);
    p->host = buf_host;
    p->port = port;
    p->socket = -1;
    p->state = FSM_INIT;
    p->client_socket = -1; // in server
    p->vf_is_accepted_state = &__vf_is_accepted_state;
    p->vf_fsm = &__vf_fsm;
    return p;
}

/**
 * @brief Release a FSM instance. Once released, the pointer become invalid.
 * 
 * @param ctx The object to be deleted.
 */
void del_fsm_context(fsm_context *ctx)
{
    free(ctx->host);
    free(ctx);
}

/**
 * @brief Send the whole file content via a socket.
 * 
 * @param socket the socket.
 * @param fp the file discriptor.
 * @return int 0 if succeed, non-zero if an error occurred.
 */
int send_file(int socket, FILE *fp)
{
    // send file content in 4k slices
    void *read_buf = malloc(SEND_BUFFER_SIZE);
    if (!read_buf)
    {
        fprintf(stderr, "Failed to allocate %" PRIu64 " bytes.\n", (uint64_t)SEND_BUFFER_SIZE);
        return CLIENT_ERR_MALLOC_FAILURE;
    }

    if (fseek(fp, 0L, SEEK_SET))
    {
        perror("Failed to rewind file");
        free(read_buf);
        return CLIENT_ERR_FAILED_TO_READ_FILE;
    }

    ssize_t total_size_sent = 0;
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start);
    while (!feof(fp))
    {
        size_t sz_read = fread(read_buf, 1, SEND_BUFFER_SIZE, fp);
        if (ferror(fp))
        {
            // failed to read
            perror("An error occurred while reading file");
            // TODO
            free(read_buf);
            return CLIENT_ERR_FAILED_TO_READ_FILE;
        }
        ssize_t sz_sent = write(socket, read_buf, sz_read);
        if (sz_sent == -1 || sz_read != sz_sent)
        {
            fprintf(stderr, "Failed to write socket: %d\n", errno);
            fprintf(stderr, "Sent %zd bytes.\n", total_size_sent);
            free(read_buf);
            return CLIENT_ERR_SOCKET_ERROR;
        }
        total_size_sent += sz_sent;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end);
    free(read_buf);

    // quit by EOF or error
    int ferr;
    if ((ferr = ferror(fp)))
    {
        fprintf(stderr, "An error occurred while reading file: ferrno[%d].\n", ferr);
        return CLIENT_ERR_FAILED_TO_READ_FILE;
    }

    // // check sent size
    // if (total_size_sent != preamble.length)
    // {
    //     fprintf(stderr, "Wrong sent size: %llu bytes rather than %llu bytes.\n"
    //         , total_size_sent, preamble.length);
    //     return CLIENT_ERR_SEND_SIZE_MISMATCH;
    // }
    uint64_t delta_us = (ts_end.tv_sec - ts_start.tv_sec) * 1000000 + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000;
    // 0.95367431640625 == (1000 / 1024) * (1000 / 1024)
    printf("Time elapsed: %.2fs. Average speed: %.2fMB/s.\n", delta_us / 1.0E6, total_size_sent * 0.95367431640625 / delta_us);
    return CLIENT_ERR_SUCCESS;
}

/**
 * @brief Receive file from peer. Save it into given file discriptor.
 * 
 * @param socket the socket to read.
 * @param fp the opened file to save in.
 * @param file_size the file size.
 * @return int 0 if success, non-zero if an error had occurred.
 */
int receive_file(int socket, FILE *fp, u_int64_t file_size)
{
    // FIXME: may go into unrecoverable error, thus timeout is needed

    // initialize buffer
    void *read_buf = malloc(RECV_BUFFER_SIZE);
    if (!read_buf)
    {
        fprintf(stderr, "Failed to malloc %" PRIu64 " bytes.\n", (uint64_t)RECV_BUFFER_SIZE);
        return CLIENT_ERR_MALLOC_FAILURE;
    }

    // read from socket
    __DEBUG("Reading socket..");
    ssize_t sz_recv, total_recv = 0;
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start);
    while ((sz_recv = read(socket, read_buf, RECV_BUFFER_SIZE)) > 0)
    {
        size_t sz_written;
        DEBUGS(printf("Read %zd bytes from socket.\n", sz_recv));
        if ((sz_written = fwrite(read_buf, 1, sz_recv, fp)) != sz_recv)
        {
            perror("An I/O error occurred while writing file");
            fprintf(stderr, "Failed to write %zd bytes to file: %zd bytes actually.\n"
                , sz_recv, sz_written);
            free(read_buf);
            fflush(fp);
            return CLIENT_ERR_FAILED_TO_WRITE_FILE;
        }
        if ((total_recv += sz_recv) == file_size)
            break; // all data has been received
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end);
    fflush(fp);
    free(read_buf);

    // socket read failure
    if (sz_recv < 0)
    {
        perror("An error occurred while receiving file");
        return CLIENT_ERR_SOCKET_ERROR;
    }

    if (!sz_recv)
        DEBUGS(printf("Encountered EOF.\n"));
    DEBUGS(printf("total_recv=%zd, file_size=%" PRIu64 ".\n", total_recv, file_size));
    uint64_t delta_us = (ts_end.tv_sec - ts_start.tv_sec) * 1000000 + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000;
    // 0.95367431640625 == (1000 / 1024) * (1000 / 1024)
    printf("Time elapsed: %.2fs. Average speed: %.2fMB/s.\n", delta_us / 1.0E6, file_size * 0.95367431640625 / delta_us);
    ASSERT2(total_recv == file_size, "File size mismatch");
    return CLIENT_ERR_SUCCESS;
}

/**
 * @brief Send a HandShake message to the remote peer.
 * 
 * @param s the socket to use.
 * @return int 0 if succeed, non-zero if failed.
 */
int send_handshake(int s)
{
    // read_buf is identical to NFH_HELLO, just reuse it
    if (write(s, NFH_HELLO, LEN_NFH_HELLO) < 0)
    {
        // failed to write
        perror("Failed to send greeting message");
        return -1;
    }
    return 0;
}

/**
 * @brief Receive and expect a correct HandShake message from the remote peer.
 * 
 * @param s the socket to use.
 * @return int 0 if succeed, non-zero if failed.
 */
int expect_handshake(int s)
{
    char read_buf[16];
    int read_sz;
    if ((read_sz = read_exactly(s, read_buf, LEN_NFH_HELLO)) <= 0)
    {
        // failed to read
        int errsv = errno;
        fprintf(stderr, "Failed to read from socket: read() "
            "returned %d, [errno %d] %s.\n", read_sz, errsv, strerror(errsv));
        return -1;
    }

    // read successfully

    // compare length
    if (read_sz != LEN_NFH_HELLO)
    {
        read_buf[read_sz] = '\0';
        fprintf(stderr, "Bad handshake from peer: unexpected EOF while reading "
            "greeting message (NFH_HELLO): expected `" NFH_HELLO "`, got %s.\n", read_buf);
        
        // bad client
        return -1;
    }

    // compare content
    read_buf[read_sz] = '\0';
    if (strcmp(read_buf, NFH_HELLO))
    {
        // unequal
        // bad client
        fprintf(stderr, "Bad handshake message from peer: got `%s` instead of `" NFH_HELLO "`.\n", read_buf);
        return -1;
    }

    return 0;
}

int send_bye_message(int s)
{
    int sz_write;
    if ((sz_write = write(s, NFH_BYE, LEN_NFH_BYE)) != LEN_NFH_BYE)
    {
        if (sz_write < 0)
        {
            perror("Failed to send BYE message");
        }
        else
        {
            fprintf(stderr, "Failed to send BYE message: "
                "%d bytes expected, but wrote %d bytes.\n", LEN_NFH_BYE, sz_write);
        }
        return -1;
    }
    return 0;
}

int receive_bye_message(int s)
{
    int sz_recv;
    char read_buf[LEN_NFH_BYE + 1];
    read_buf[LEN_NFH_BYE] = '\0';
    if ((sz_recv = read_exactly(s, read_buf, LEN_NFH_BYE)) != LEN_NFH_BYE)
    {
        if (sz_recv < 0)
        {
            perror("Failed to read BYE message");
        }
        else
        {
            fprintf(stderr, "Failed to read BYE message: "
                "%d bytes expected, but got %d bytes.\n", LEN_NFH_BYE, sz_recv);
        }
        return -1;
    }
    if (strcmp(read_buf, NFH_BYE))
    {
        fprintf(stderr, "Invalid BYE message from client: %s.\n", read_buf);
        return -1;
    }
    return 0;
}

static int __vf_is_accepted_state(fsm_context *ctx)
{
    return ctx->state == FSM_STOP;
}

static int __vf_fsm(fsm_context *ctx)
{
    // return 0 if success
    // return non-zero if failed
    int failed = 0;
    while (!ctx->vf_is_accepted_state(ctx))
    {
        int r;
        switch(ctx->state)
        {
            case FSM_INIT:
                // initialize socket and connection
                __DEBUG("FSM_INIT");
                r = ctx->vf_init(ctx);
                break;
            case FSM_HS:
                // Handshake
                // puts("Connecting...")
                __DEBUG("FSM_HS");
                r = ctx->vf_handshake(ctx);
                break;
            case FSM_MS:
                // ModeSwitch
                __DEBUG("FSM_MS");
                r = ctx->vf_modeswitch(ctx);
                break;
            case FSM_DE:
                // DataExchange
                __DEBUG("FSM_DE");
                r = ctx->vf_dataexchange_handler(ctx);
                break;
            case FSM_Q:
                // Quit
                __DEBUG("FSM_Q");
                r = ctx->vf_quit_handler(ctx);
                break;
            case FSM_DIE:
                // Die
                __DEBUG("FSM_DIE");
                r = ctx->vf_connection_die(ctx);
                break;
        }
        failed |= r;
    }
    return failed;
}

// static int __vf_is_error_state(fsm_context *ctx)
// {
//     return ctx->state == FSM_ERR;
// }