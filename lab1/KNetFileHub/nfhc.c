/*************************************
 *  NFH Client Class Implementation  *
 *************************************/

#include "nfhc.h"
#include "util.h"

// private methods
static int __vf_client_dataexchange_upload(fsm_context *ctx);
static int __vf_client_dataexchange_download(fsm_context *ctx);
static int __vf_client_quit_from_download_handler(fsm_context *ctx);
static int __vf_client_quit_from_upload_handler(fsm_context *ctx);

// int main(int argc, char** argv)
// {
//     if (argc == 1 || argc > 2)
//     {
// PRINT_HELP_MENU:
//         puts("nfhc: NetFileHub Client (KNFHC: Keuin's NetFileHub Client implementation).\n");
//         putf("Usage: nfhc [--upload|--download|--help] [<file_name>]\n");
//         return 0;
//     }

//     // argc == 2
//     int download = !strcmp(argv[1], "--download") ^ !strcmp(argv[2], "--download");
//     int upload = !strcmp(argv[1], "--upload") ^ !strcmp(argv[2], "--upload");

//     if (!(upload ^ download))
//         goto PRINT_HELP_MENU;
    
//     char *file_name;

//     if (!strcmp(argv[1], "--download") || !strcmp(argv[1], "--upload"))
//         file_name = argv[2];
//     else
//         file_name = argv[1];

    
// }

static int __vf_client_init(fsm_context *ctx)
{
    // mostly copied from `__vf_server_init` and `server_new`
    // establish connection to the server
    puts("Connecting to server...");
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        int errsv = errno;
        fprintf(stderr, "Failed to create socket: [errno %d] %s\n", errsv, strerror(errsv));
FAILED:
        ctx->state = FSM_DIE;
        return -1;
    }

    // parse host string
    struct sockaddr_in addr;
    if ((addr.sin_addr.s_addr = inet_addr(ctx->host)) == INADDR_NONE)
    {
        fprintf(stderr, "Invalid inet4 address: %s\n", ctx->host);
        goto FAILED;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->port);

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("Failed to connect to server");
        goto FAILED;
    }

    // connected successfully
    // make handshake in HandShake phase
    ASSERT2(s >= 0, "Bad socket");
    ctx->socket = s;
    ctx->state = FSM_HS;
    return 0;
}

static int __vf_client_connection_die(fsm_context *ctx)
{
    // Q -> [AC] DIE -> [quit by main loop]
    fprintf(stderr, "Disconnected from server.\n");
    close(ctx->socket);
    ctx->socket = -1;
    ctx->state = FSM_STOP;
    return 0;
}

static int __vf_client_handshake(fsm_context *ctx)
{
    // send handshake to server
    // then receive handshake from server

    // go into Handshake state
    ctx->state = FSM_HS;
    const int s = ctx->socket;

    ASSERT2(s >= 0, "Invalid socket when handshaking");

    // send handshake
    if (send_handshake(s))
    {
        // failed to send handshake
        goto HS_ERR_SUCCESS_ACCEPT_FAILED_READ;
    }

    // wait for greeting message
    // if the response is invalid, 
    // disconnect this client and reset
    if (expect_handshake(s))
    {
        // failed to receive a valid handshake
        // bad client or broken connection
HS_ERR_SUCCESS_ACCEPT_FAILED_READ:
        ctx->state = FSM_DIE;
        return -1;
    }

    // now switch to ModeSwitch phase
    // wait for client selecting mode
    ctx->state = FSM_MS;
    return 0;
}

static int __vf_client_modeswitch(fsm_context *ctx)
{
    // read mode from stdin
    // send ModeSwitch command to server
    const int s = ctx->socket;
    const int mode_upload = 2, mode_download = 1;
    int mode = 0;
    do
    {
        printf("Select mode ([%d] DOWNLOAD, [%d] UPLOAD): ", mode_download, mode_upload);
    } while (scanf("%d", &mode) != 1 || (mode != mode_upload && mode != mode_download));
    
    char *modesw_cmd;
    if (mode == mode_download)
        modesw_cmd = NFHC_MODE_DOWNLOAD;
    else if (mode == mode_upload)
        modesw_cmd = NFHC_MODE_UPLOAD;
    else
        ASSERT2(0, "should not go here");
    
    int write_sz;
    if ((write_sz = write(s, modesw_cmd, LEN_NFHC_MODE_SWITCH) != LEN_NFHC_MODE_SWITCH))
    {
        if (write_sz < 0)
        {
            perror("Failed to send MODESW command");
VF_C_MS_FAILED:
            ctx->state = FSM_DIE;
            return -1;
        }
        else
        {
            fprintf(stderr, "Cannot write %d bytes to socket:"
                " %d bytes actually.\n", LEN_NFHC_MODE_SWITCH, write_sz);
            goto VF_C_MS_FAILED;
        }
    }

    // switched successfully
    // wait for response
    char read_buf[LEN_NFHS_ALLOW + 1];
    int read_sz;
    if ((read_sz = read_exactly(s, read_buf, LEN_NFHS_ALLOW)) != LEN_NFHS_ALLOW)
    {
        if (write_sz < 0)
        {
            perror("Failed to read SA.ALLOW command");
            goto VF_C_MS_FAILED;
        }
        else
        {
            fprintf(stderr, "Cannot read %d bytes NFHS_ALLOW from socket:"
                " %d bytes actually.\n", LEN_NFHC_MODE_SWITCH, read_sz);
            goto VF_C_MS_FAILED;
        }
    }

    // read successfully
    // check message semantic
    // bind vfunc
    if (mode == mode_upload && !strcmp(read_buf, NFHS_ALLOW_UPLOAD))
    {
        // success
        ctx->vf_dataexchange_handler = __vf_client_dataexchange_upload;
        ctx->vf_quit_handler = &__vf_client_quit_from_upload_handler;
        ctx->state = FSM_DE;
        puts("Switch mode to UPLOAD.");
        return 0;
    }
    if (mode == mode_download && !strcmp(read_buf, NFHS_ALLOW_DOWNLOAD))
    {
        // success
        ctx->vf_dataexchange_handler = __vf_client_dataexchange_download;
        ctx->vf_quit_handler = &__vf_client_quit_from_download_handler;
        ctx->state = FSM_DE;
        puts("Switch mode to DOWNLOAD.");
        return 0;
    }

    fprintf(stderr, "Bad response from server: \"%s\", failed to switch mode.\n", read_buf);
    return -1;
}

static int __vf_client_dataexchange_upload(fsm_context *ctx)
{
    const int s = ctx->socket;
    // select a file, then send it
    char file_path[64];
    while(1)
    {
        do
        {
        printf("Input file to send:");
        } while (scanf("%63s", file_path) != 1);
        FILE *f = fopen(file_path, "rb");
        if (f)
        {
            // file exists
            fclose(f);
            break;
        }
        puts("File does not exist! Try again.");
    }

    FILE *fp = fopen(file_path, "rb");

    if (!fp)
    {
        perror("Failed to open file");
C_DE_U_FAIL:
        if (fp)
            fclose(fp);
        ctx->state = FSM_DIE;
        return -1;
    }
    // get file size
    struct stat a;
    if (fstat(fileno(fp), &a))
    {
        perror("Error occurred in fstat");
        goto C_DE_U_FAIL;
    }

    // send fp to s
    
    
    struct sa_c2s_file_preamble preamble;
    memset(&preamble, 0, sizeof(struct sa_c2s_file_preamble));
    preamble.length = a.st_size;
    char file_path_2[64];
    strcpy(file_path_2, file_path); // function `basename` may modify the buffer, so make a copy
    strcpy(preamble.name, basename(file_path_2));
    
    puts("Sending preamble...");
    int write_sz;
    if ((write_sz = write(s, &preamble, sizeof(struct sa_c2s_file_preamble))) != sizeof(struct sa_c2s_file_preamble))
    {
        if (write_sz == -1)
        {
            perror("Failed to send preamble");
            goto C_DE_U_FAIL;
        }
        else
        {
            fprintf(stderr, "Failed to write socket.\n");
            goto C_DE_U_FAIL;
        }
    }

    puts("Sending file content...");

    if (send_file(s, fp))
    {
        goto C_DE_U_FAIL;
    }
    puts("Done.");

    ctx->state = FSM_Q;
    return 0;
}

static int __vf_client_dataexchange_download(fsm_context *ctx)
{
    const int s = ctx->socket;
    // receive file list
    uint64_t file_count;
    int read_sz;
    if ((read_sz = read_exactly(s, &file_count, sizeof(uint64_t))) != sizeof(uint64_t))
    {
        if (!read_sz)
        {
            perror("Failed to read file list size");
C_DE_D_FAIL:
            ctx->state = FSM_DIE;
            return -1;
        }
        fprintf(stderr, "Failed to read 8 bytes of file list size.\n");
        goto C_DE_D_FAIL;
    }
    printf("Server has %" PRIu64 " file(s).\n", file_count);

    struct so_s2c_file_entry *file_list = calloc(file_count, sizeof(struct so_s2c_file_entry));
    if (!file_list)
    {
        perror("calloc() failed");
        goto C_DE_D_FAIL;
    }
    const size_t file_list_bytes = file_count * sizeof(struct so_s2c_file_entry);
    if ((read_sz = read_exactly(s, file_list, file_list_bytes)) != file_list_bytes)
    {
        if (!read_sz)
        {
            perror("Failed to get file list");
C_DE_D_FAIL_FREE:
            free(file_list);
            goto C_DE_D_FAIL;
        }
        fprintf(stderr, "Failed to read file list.\n");
        goto C_DE_D_FAIL_FREE;
    }
    // decide which to download
    puts("Available files:");
    printf("%4s\t%12s\t%12s\t%24s\n", "ID", "NAME", "SIZE", "TIME MODIFIED");
    for(uint64_t i = 0; i < file_count; ++i)
    {
        struct so_s2c_file_entry *pent = &file_list[i];
        // validate string
        if (!is_string_buf_valid(pent->name, MAX_FILENAME_LENGTH))
        {
            fprintf(stderr, "Corrupt entry #%" PRIu64
                ": File name string is not ended with EOF.\n", i);
            goto C_DE_D_FAIL_FREE;
        }
        char *ts = ctime((time_t*)&file_list[i].ts_modified);
        printf("%4" PRIu64 "\t%12s\t%12" PRIu64 "\t%24s", i, pent->name, pent->size, ts);
    }
    if (file_count == 1)
        printf("File to download [0]:");
    else
        printf("File to download [0-%" PRIu64 "]:", file_count - 1);
    uint64_t file_id = -1;
    while (scanf("%" PRIu64, &file_id) != 1 || (file_id < 0 || file_id >= file_count))
        ;
    // set save file name
    char save_as[64];
    FILE *fp_save; // save to this fp
    while (1)
    {
        printf("Save as:");
        if (scanf("%63s", save_as) == 1)
        {
            fp_save = fopen(save_as, "rb");
            if (fp_save)
            {
                printf("Warning: file %s already exists. Overwrite? (y/N)", save_as);
                char do_overwrite;
                if (scanf("%c", &do_overwrite) == 1 && (do_overwrite == 'Y' || do_overwrite == 'y'))
                {
                    fclose(fp_save);
                    fp_save = fopen(save_as, "wb");
                    break;
                }
            }
            else
            {
                // file does not exist
                fp_save = fopen(save_as, "wb");
                break;
            }
        }
    }

    // send file id
    int write_sz;
    if ((write_sz = write(s, &file_id, sizeof(uint64_t))) != sizeof(uint64_t))
    {
        if (write_sz < 0)
        {
            perror("Failed to send file id");
        }
        else
        {
            fprintf(stderr, "Unexpected EOF at %d (of 8 bytes).\n", write_sz);
        }
        goto C_DE_D_FAIL_FREE;
    }
    // receive file content
    const uint64_t total_size = file_list[file_id].size;
    const char *file_name = file_list[file_id].name;
    printf("Receiving file %s...\n", file_name);
    if (receive_file(s, fp_save, total_size))
    {
        // failed
        fclose(fp_save);
        goto C_DE_D_FAIL_FREE;
    }
    // success
    fclose(fp_save);
    ctx->state = FSM_Q;
    return 0;
}

static int __vf_client_quit_from_upload_handler(fsm_context *ctx)
{
    // obey to `vfunc_quit_handler`
    // exchange BYE message, then go to DIE state
    int s = ctx->socket;

    // wait for server's BYE
    puts("Waiting for server's BYE...");
    if (receive_bye_message(s))
    {
        ctx->state = FSM_DIE;
        return -1;
    }

    // send BYE
    puts("Sending BYE message to server...");
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

static int __vf_client_quit_from_download_handler(fsm_context *ctx)
{
    // obey to `vfunc_quit_handler`
    // exchange BYE message, then go to DIE state
    int s = ctx->socket;

    // send BYE
    puts("Sending BYE message to server...");
    if (send_bye_message(s))
    {
        ctx->state = FSM_DIE;
        return -1;
    }

    // wait for server's BYE
    puts("Waiting for server's BYE...");
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

/**
 * @brief Create a new client instance, but does not create the socket and connect.
 * 
 * @param host the server host.
 * @param port the server port.
 * @return fsm_context* the object created.
 */
fsm_context *client_new(char *host, u_int16_t port)
{
    fsm_context *p = new_fsm_context(host, port);
    if (!p)
    {
        return p;
    }
    // init concrete class member
    /// init vfunc
    p->vf_init = &__vf_client_init;
    p->vf_handshake = &__vf_client_handshake;
    p->vf_modeswitch = &__vf_client_modeswitch;
    p->vf_connection_die = &__vf_client_connection_die;
    p->vf_dataexchange_handler = 0; // dynamically bound in ModeSwitch phase
    p->vf_quit_handler = 0; // dynamically bound in ModeSwitch phase
    return p;
}

void client_delete(fsm_context *ctx)
{
    if (!ctx)
        return;
    // close(ctx->socket);
    // call super destructor
    del_fsm_context(ctx);
}


/**
 * @brief Send a file preamble to the server, which is used in DE phase when uploading a file.
 * 
 * @param socket the socket to the server. Must be valid.
 * @param fp the file discriptor. Must be readable in binary mode.
 * @param file_name the file name.
 * @return int 0 if success, non-zero if failed.
 */
int client_send_file_preamble(int socket, FILE *fp, char *file_name)
{
    // given socket must be valid and the file pointer must be readable
    // file name must be an ASCII string and must not be longer than 255.
    if (strlen(file_name) > MAX_FILENAME_LENGTH)
        return CLIENT_ERR_FILE_NAME_TOO_LONG;

    int feno, errsv;
    clearerr(fp);
    rewind(fp);
    errsv = errno;
    feno = ferror(fp);
    if (errsv || feno)
    {
        fprintf(stderr, "Failed to read file: ferrno[%d], errno[%d]. %s\n",
            feno, errsv, strerror(errsv));
        return CLIENT_ERR_FAILED_TO_READ_FILE;
    }

    // construct preamble
    struct sa_c2s_file_preamble preamble;
    struct stat a;
    fstat(fileno(fp), &a);
    memset(&preamble, 0, sizeof(struct sa_c2s_file_preamble));
    preamble.length = a.st_size;
    strcpy(preamble.name, file_name);

    // send preamble
    ssize_t actual_sz;
    if ((actual_sz = send(socket, &preamble, sizeof(struct sa_c2s_file_preamble), 0))
        != sizeof(struct sa_c2s_file_preamble))
    {
        fprintf(stderr, "Incorrect actual sent size: expected %" PRIu64 " bytes, "
            "actual %zd bytes.\n", preamble.length, actual_sz);
        return CLIENT_ERR_SOCKET_ERROR;
    }

    return 0;
}