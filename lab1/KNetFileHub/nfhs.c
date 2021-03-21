/*************************************
 *  NFH Server Class Implementation  *
 *************************************/

#include "nfhs.h"
#include "util.h"

// private methods
static int __vf_server_init(fsm_context *ctx);
static int __vf_server_connection_die(fsm_context *ctx);
static int __vf_server_handshake(fsm_context *ctx);
static int __vf_server_modeswitch(fsm_context *ctx);
static int __vf_server_dataexchange_upload(fsm_context *ctx);
static int __vf_server_dataexchange_download(fsm_context *ctx);
static int __vf_server_quit_from_upload_handler(fsm_context *ctx);
static int __vf_server_quit_from_download_handler(fsm_context *ctx);

fsm_context *server_new(char *host, u_int16_t port)
{
    fsm_context *p = new_fsm_context(host, port);
    if (!p)
    {
        return p;
    }
    // init concrete class member
    /// init vfunc
    p->vf_init = &__vf_server_init;
    p->vf_handshake = &__vf_server_handshake;
    p->vf_modeswitch = &__vf_server_modeswitch;
    p->vf_connection_die = &__vf_server_connection_die;
    // p->vf_dataexchange_handler = 0; // dynamically bound in ModeSwitch phase

    // initialize socket related things
// initialize socket and listen to it
    // server accepts new connections in Init phase

    // create socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1)
    {
        int errsv = errno;
        fprintf(stderr, "Failed to create socket: [errno %d] %s\n", errsv, strerror(errsv));
FAILED:
        // p->state = FSM_DIE;
        del_fsm_context(p);
        return 0;
    }

    // parse host string
    struct in_addr addr;
    if (!inet_aton(p->host, &addr))
    {
        fprintf(stderr, "Invalid inet4 address: %s\n", p->host);
        goto FAILED;
    }

    // bind
    // struct sockaddr_in listen_endpoint;
    // listen_endpoint.sin_addr = addr;
    // listen_endpoint.sin_family = AF_INET;
    // listen_endpoint.sin_port = htons(p->port);

    struct sockaddr_in listen_endpoint;
    // the host is ignored, for convenience
    listen_endpoint.sin_addr.s_addr = INADDR_ANY;
    // if ((listen_endpoint.sin_addr.s_addr = inet_addr(p->host)) == INADDR_NONE)
    // {
    //     fprintf(stderr, "Invalid inet4 address: %s\n", p->host);
    //     goto FAILED;
    // }
    listen_endpoint.sin_family = AF_INET;
    listen_endpoint.sin_port = htons(p->port);

    if (bind(s, (struct sockaddr*)&listen_endpoint, sizeof(listen_endpoint)))
    {
        int errsv = errno;
        fprintf(stderr, "Failed to bind to endpoint %s:%d [errno %d]: %s\n",
            p->host, p->port, errsv, strerror(errsv));
        goto FAILED;
    }

    // listen to [host]:[port]
    if (listen(s, SERVER_LISTEN_BACKLOG))
    {
        int errsv = errno;
        fprintf(stderr, "Failed to listen to socket %s:%d [error %d]: %s\n",
            p->host, p->port, errsv, strerror(errsv));
        goto FAILED;
    }

    p->socket = s;

    return p;
}


void server_delete(fsm_context *ctx)
{
    if (!ctx)
        return;
    // call super destructor
    del_fsm_context(ctx);
}


static int __vf_server_init(fsm_context *ctx)
{
    // mostly copied to `__vf_client_init`

    // accept new connections
    printf("\nWaiting for clients...\n");
    int s;
    ASSERT2(ctx->socket >= 0, "Invalid greeting socket");
    if ((s = accept(ctx->socket, NULL, NULL)) < 0)
    {
        // failed to accept
        // ctx->client_socket = -1; // in case we forgot to reset the socket
        perror("Failed to accept new connection");
// HS_ERR_SUCCESS_ACCEPT_FAILED_READ:
        ctx->state = FSM_DIE;
        return -1;
    }
    
    // connected successfully

    // new client
    // update client socket
    ctx->client_socket = s;

    // go into Handshake state
    ctx->state = FSM_HS;
    return 0;
}

static int __vf_server_connection_die(fsm_context *ctx)
{
    // when the server disconnected from the client or received illegal messages
    // go into this state, and clean up

    // check client socket
    int s;
    if ((s = ctx->client_socket) != -1)
    {
        ctx->client_socket = -1;
        fprintf(stderr, "Disconnected from client.\n");
        close(s);
    }

    // check if the greeting socket is alive
    if (ctx->socket < 0)
    {
        // something historical...
        // maybe buggy
        fprintf(stderr, "Cannot create greeting socket. Quit.\n");
        ctx->state = FSM_STOP;
        return 0;
    }

    // goto Init phase to accept another client
    ctx->state = FSM_INIT;
    return 0;
}


static int __vf_server_handshake(fsm_context *ctx)
{
    // wait for greeting message
    // if the response is invalid, 
    // disconnect this client and reset
    const int s = ctx->client_socket;
    if (expect_handshake(s))
    {
        // failed to receive a valid handshake
        // bad client or broken connection
HS_ERR_SUCCESS_ACCEPT_FAILED_READ:
        ctx->state = FSM_DIE;
        return -1;
    }
    puts("Received handshake from client.");

    // good handshake!
    // respond with a greeting message
    if (send_handshake(s))
    {
        // failed to send handshake
        goto HS_ERR_SUCCESS_ACCEPT_FAILED_READ;
    }

    // now switch to ModeSwitch phase
    // wait for client selecting mode
    puts("Connection established.");
    ctx->state = FSM_MS;
    return 0;
}


static int __vf_server_modeswitch(fsm_context *ctx)
{
    // server read mode switch instruction
    // if invalid, close the client socket and reset to handshake phase
    int s = ctx->client_socket;
    char read_buf[16];
    int read_sz;
    if ((read_sz = read_exactly(s, read_buf, LEN_NFHC_MODE_SWITCH)) > 0)
    {
        // success
        read_buf[read_sz] = '\0';

        // compare length
        if (read_sz != LEN_NFHC_MODE_SWITCH)
        {
            fprintf(stderr, "Invalid MODE_SWITCH instruction: %s instead of "
                "`" NFHC_MODE_DOWNLOAD "` or `" NFHC_MODE_UPLOAD "`.\n", read_buf);
            goto MS_FAILED;
        }

        // do action
        vfunc_dataexchange_handler *de_handler; // dataexchange handler
        char *allow_message;
        if (!strcmp(read_buf, NFHC_MODE_UPLOAD))
        {
            // upload
            puts("Client wants to upload.");
            de_handler = &__vf_server_dataexchange_upload;
            allow_message = NFHS_ALLOW_UPLOAD;
            ctx->vf_dataexchange_handler = &__vf_server_dataexchange_upload;
            ctx->vf_quit_handler = &__vf_server_quit_from_upload_handler;
        }
        else if (!strcmp(read_buf, NFHC_MODE_DOWNLOAD))
        {
            // download
            puts("Client wants to download.");
            de_handler = &__vf_server_dataexchange_download;
            allow_message = NFHS_ALLOW_DOWNLOAD;
            ctx->vf_dataexchange_handler = &__vf_server_dataexchange_download;
            ctx->vf_quit_handler = &__vf_server_quit_from_download_handler;
        }
        else
        {
            // invalid instruction
            fprintf(stderr, "Bad client: Invalid MODE_SWITCH instruction: %s.\n", read_buf);
            goto MS_FAILED;
        }

        // send ALLOW message
        puts("Sending ALLOW message...");
        int sz_write;
        if ((sz_write = write(s, allow_message, LEN_NFHS_ALLOW)) != LEN_NFHS_ALLOW)
        {
            if (sz_write == -1)
            {
                perror("Failed to write to socket");
            }
            else
            {
                fprintf(stderr, "Failed to write %d bytes to socket, actually %d bytes.\n", LEN_NFHS_ALLOW, sz_write);
            }
            goto MS_FAILED;
        }

        // update state
        ctx->vf_dataexchange_handler = de_handler;
        ctx->state = FSM_DE;

        char *mode_name = ((void*)allow_message == (void*)NFHS_ALLOW_UPLOAD) ? "UPLOAD" : "DOWNLOAD";
        printf("Switched to %s mode.\n", mode_name);
        return 0;
    }
    else
    {
        perror("Failed to read ModeSwitch instruction");
MS_FAILED: // ModeSwitch failed
        ctx->state = FSM_DIE;
        return -1;
    }
}


static int __vf_server_dataexchange_upload(fsm_context *ctx)
{
    // polymorphic methods (of vfunc_dataexchange_handler)
    // accept one sa_c2s_file_preamble, then a byte seq with given size
    const int s = ctx->client_socket;

    // read preamble
    struct sa_c2s_file_preamble preamble;
    int sz_read;
    const int sz_desired = sizeof(struct sa_c2s_file_preamble);
    __DEBUG("Reading file preamble");
    if ((sz_read = read_exactly(s, &preamble, sz_desired)) != sz_desired)
    {
        if (sz_read < 0)
        {
            perror("Failed to read file preamble from client");
        }
        else
        {
            // bad size
            fprintf(stderr, "Failed to read file preamble from client: "
                "read %d bytes, but expected %d bytes.\n", sz_read, sz_desired);
        }
SERVER_DE_FAIL:
        ctx->state = FSM_DIE;
        return -1;
    }

    // check string EOF
    if (!is_string_buf_valid(preamble.name, MAX_FILENAME_LENGTH))
    {
        fprintf(stderr, "Invalid file name in preamble: String is not ended with EOF.\n");
        goto SERVER_DE_FAIL;
    }
    if (!is_valid_file_name(preamble.name))
    {
        fprintf(stderr, "File name contains invalid character: %s.\n", preamble.name);
        goto SERVER_DE_FAIL;
    }
    printf("File name: %s, size: %" PRIu64 " bytes.\n", preamble.name, preamble.length);

    // save file from socket
    FILE *fp = fopen(preamble.name, "rb");

    // check if the file already exists
    if (fp)
    {
        fclose(fp);
        fprintf(stderr, "File %s already exists. Cannot receive.\n", preamble.name);
        goto SERVER_DE_FAIL;
    }

    // receive file
    if (!(fp = fopen(preamble.name, "wb")))
    {
        int errsv = errno;
        fprintf(stderr, "Cannot open file %s [errno %d]: %s\n",
            preamble.name, errsv, strerror(errsv));
        goto SERVER_DE_FAIL;
    }

    __DEBUG("Receiving file content");
    if (receive_file(s, fp, preamble.length))
    {
        fclose(fp);
        fprintf(stderr, "Failed to receive file!\n");
        goto SERVER_DE_FAIL;
    }

    // success
    fclose(fp);
    ctx->state = FSM_Q;
    printf("Received file %s successfully!\n", preamble.name);
    return 0;
}

static int __vf_server_dataexchange_download(fsm_context *ctx)
{
    // polymorphic methods

    // firstly send file list
    // then get response (file selection) from client
    // then send file back to the client
    // fially go to Quit
    int s = ctx->client_socket;

    // list files
    DIR *dir = NULL;
    struct dirent *entry;
    dir = opendir(".");
    const int list_sz = 1024;
    const int list_bytes = sizeof(struct so_s2c_file_entry) * list_sz; // bytes of file_list buffer in memory
    int p = 0;
    struct so_s2c_file_entry *file_list = calloc(list_sz, list_bytes);
    if (!file_list)
    {
        fprintf(stderr, "Failed to malloc.\n");
DE_DOWNLOAD_FAIL:
        ctx->state = FSM_DIE;
        if (dir)
            closedir(dir);
        return -1;
    }
    if (!dir)
    {
        perror("Cannot list files");
        goto DE_DOWNLOAD_FAIL;
    }
    while ((entry = readdir(dir)))
    {
        if (entry->d_type != DT_REG)
            continue; // skip non-regular files
        file_list[p].id = p;
        strcpy(file_list[p].name, entry->d_name);

        FILE *fp = fopen(entry->d_name, "rb");
        if (!fp)
        {
            fprintf(stderr, "Cannot open file `%s`. Skip.\n", entry->d_name);
            continue;
        }
        struct stat a;
        if (fstat(fileno(fp), &a))
        {
            perror("Error occurred in fstat");
            fclose(fp);
            goto DE_DOWNLOAD_FAIL;
        }
        fclose(fp);
        file_list[p].size = a.st_size;
        file_list[p].ts_modified = a.st_mtime; // Unix epoch (UTC)
        if (++p == list_sz)
            break;
    }
    closedir(dir);
    dir = NULL;

    // send file list to the client
    uint64_t buf_file_count;
    buf_file_count = (uint64_t)p; // avoid implicit cast caused size change

    // send list size
    int send_sz;
    if ((send_sz = write(s, &buf_file_count, sizeof(uint64_t))) != sizeof(uint64_t))
    {
        if (send_sz < 0)
        {
            perror("Failed to send file list size");
        }
        else
        {
            fprintf(stderr, "An error occurred while sending file list size: %d bytes written.\n", send_sz);
        }
        goto DE_DOWNLOAD_FAIL;
    }

    // send list body
    const size_t real_list_bytes = sizeof(struct so_s2c_file_entry) * p;
    if ((send_sz = write(s, file_list, real_list_bytes)) != real_list_bytes)
    {
        if (send_sz < 0)
        {
            perror("Failed to send file list size");
        }
        else
        {
            fprintf(stderr, "An error occurred while sending file list: "
                "%d bytes written, total %zd bytes.\n", send_sz, real_list_bytes);
        }
        goto DE_DOWNLOAD_FAIL;
    }

    // get client selection
    // and send file
    uint64_t client_selection;
    int read_sz;
    if ((read_sz = read_exactly(s, &client_selection, 8)) != 8)
    {
        if (send_sz < 0)
        {
            perror("Failed to read file id");
        }
        else
        {
            fprintf(stderr, "An error occurred while reading file id from socket: "
                "%d bytes read, total 8 bytes.\n", read_sz);
        }
        goto DE_DOWNLOAD_FAIL;
    }

    if (client_selection >= p)
    {
        fprintf(stderr, "Client selection is out of bound: %" PRIu64 " >= %d.\n", client_selection, p);
        goto DE_DOWNLOAD_FAIL;
    }

    // good selection
    // send file data
    struct so_s2c_file_entry *file_ent = &file_list[client_selection];
    FILE *fp = fopen(file_ent->name, "rb");
    if (!fp)
    {
        int errsv = errno;
        fprintf(stderr, "Failed to open file %s [errno %d]: %s.\n", file_ent->name, errsv, strerror(errsv));
        goto DE_DOWNLOAD_FAIL;
    }

    if (send_file(s, fp))
    {
        fclose(fp);
        goto DE_DOWNLOAD_FAIL;
    }
    fclose(fp);

    // success
    // goto Quit state, waiting for client's BYE message, then send another BYE.
    ctx->state = FSM_Q;
    return 0;
}

static int __vf_server_quit_from_upload_handler(fsm_context *ctx)
{
    // obey to `vfunc_quit_handler`
    // exchange BYE message, then go to DIE state
    int s = ctx->client_socket;

    // send BYE
    puts("Sending BYE message to client...");
    if (send_bye_message(s))
    {
        ctx->state = FSM_DIE;
        return -1;
    }

    // wait for client's BYE
    puts("Waiting for client's BYE...");
    if (receive_bye_message(s))
    {
        ctx->state = FSM_DIE;
        return -1;
    }

    // good end
    puts("Bye bye.");
    ctx->state = FSM_DIE;
    return 0;
}

static int __vf_server_quit_from_download_handler(fsm_context *ctx)
{
    // obey to `vfunc_quit_handler`
    // exchange BYE message, then go to DIE state
    int s = ctx->client_socket;

    // wait for client's BYE
    puts("Waiting for client's BYE...");
    if (receive_bye_message(s))
    {
        ctx->state = FSM_DIE;
        return -1;
    }

    // send BYE
    puts("Sending BYE message to client...");
    if (send_bye_message(s))
    {
        ctx->state = FSM_DIE;
        return -1;
    }

    // good end
    puts("Bye bye.");
    ctx->state = FSM_DIE;
    return 0;
}
