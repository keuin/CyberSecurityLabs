#ifndef __NFH_H
#define __NFH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

/* configurations */
#define SERVER_DEDFAULT_PORT 3789
#define SEND_BUFFER_SIZE 4194304U /* 4KB */
#define RECV_BUFFER_SIZE 4194304U /* 4KB */
#define SERVER_LISTEN_BACKLOG 3

/* protocol specific constants */
#define MAX_FILENAME_LENGTH 255
#define NFH_HELLO "NFH.HELLO"
#define NFHC_MODE_UPLOAD "MODESW.UPLOAD"
#define NFHC_MODE_DOWNLOAD "MODESW.DOWNLD"
#define NFHS_ALLOW_UPLOAD "SA.ALLOWUPLD"
#define NFHS_ALLOW_DOWNLOAD "SA.ALLOWDNLD"
#define NFHS_OFFER_FILES "SA.FILES"
#define NFH_BYE "NFH.BYE"

#define LEN_NFH_HELLO 9
#define LEN_NFHC_MODE_SWITCH 13
#define LEN_NFHS_ALLOW 12
#define LEN_NFHS_OFFER 8
#define LEN_NFH_BYE 7

/* error codes */
#define CLIENT_ERR_SUCCESS 0
#define CLIENT_ERR_FILE_NAME_TOO_LONG -1
#define CLIENT_ERR_FAILED_TO_READ_FILE -2
#define CLIENT_ERR_FAILED_TO_WRITE_FILE -3
#define CLIENT_ERR_SOCKET_ERROR -4
#define CLIENT_ERR_MALLOC_FAILURE -5
#define CLIENT_ERR_SEND_SIZE_MISMATCH -6

/* FSM consts */
// #define FSM_ERR   9 /* [AC] error */
#define FSM_INIT 0                                                         /* connection initialization */
#define FSM_HS 1                                                           /* Handshake */
#define FSM_MS 2                                                           /* ModeSwitch */
#define FSM_DE 3 /* DataExchange, vfunc will be dynamically bound in MS */ /* only available when mode is UPLOAD */
#define FSM_SO 4                                                           /* ServerOffer, only available in DOWNLOAD mode */
#define FSM_CT 5                                                           /* WTF? */
#define FSM_Q 6                                                            /* Quit */
#define FSM_DIE 10                                                         /* clean up, recover from a broken connection or invalid peer */
#define FSM_STOP 11                                                        /* no vfunc, when reached, stop the main loop. Only joined from DIE */

/* DataExchange modes */
#define DE_MODE_UPLOAD 1
#define DE_MODE_DOWNLOAD 2

/* FSM helpers */
// #define __FSM_FAIL(ctx) ((ctx)->state = FSM_ERR)

typedef struct fsm_context fsm_context;
// typedef int vfunc_init(fsm_context *);
typedef int vfunc_fsm(fsm_context *);
typedef int vfunc_init(fsm_context *);
typedef int vfunc_handshake(fsm_context *);
typedef int vfunc_modeswitch(fsm_context *);
typedef int vfunc_dataexchange_handler(fsm_context *); // polymorphic, upload/download
typedef int vfunc_quit_handler(fsm_context *);
typedef int vfunc_is_accepted_state(fsm_context *);
typedef int vfunc_is_error_state(fsm_context *);
typedef int vfunc_connection_die(fsm_context *); // FSM_DIE

struct fsm_context
{
    // members
    int state;      // fsm state
    int socket;     // tcp socket. If the context is server, this is the greeting socket
    char *host;     // server address: listen to or connect to
    u_int16_t port; // server port: listen to or connect to

    // server members
    int client_socket; // the real socket to the client, once connected
    // int de_mode; // refactor to polymorphic vfunc

    // methods
    // these functions will set FSM to the accepted invalid state if failed
    // return non-zero value as well
    vfunc_fsm *vf_fsm;
    vfunc_init *vf_init;
    vfunc_handshake *vf_handshake;
    vfunc_modeswitch *vf_modeswitch;
    vfunc_dataexchange_handler *vf_dataexchange_handler; // dynamically bound when mode is decided, in vf_modeswitch
    vfunc_quit_handler *vf_quit_handler; // dynamically bound when mode is decided, in vf_modeswitch
    vfunc_is_accepted_state *vf_is_accepted_state;
    vfunc_is_error_state *vf_is_error_state;
    vfunc_connection_die *vf_connection_die;
};

struct sa_c2s_file_preamble
{
    u_int64_t length;
    char name[MAX_FILENAME_LENGTH + 1];
};

struct so_s2c_file_entry
{
    u_int64_t id;
    u_int64_t size;
    u_int64_t ts_modified;
    char name[MAX_FILENAME_LENGTH + 1];
};

/*

NFH Protocol Specification:

Definition:
    NFH is a plain, simple C/S file transfer protocol.
    It allows the client to send one file to the serv-
    er, and allows the client to copy one file from
    the server's file list.
Session Phases:
    Phase 1: Handshake (Client <=> Server): [HS]
        The server listens on a specified port (default to 3789).
        The client then connects to the server. Server send nothing
        until the client sends ASCII `NFH.HELLO` (defined as NFH_HELLO).
        The server should close connection if the client sends an invalid
        packet. Server then reply with NFH_HELLO as well.
        Now, the client and the server know each other is a valid NFH client/server.
    Phase 2: ModeSwitch (Client => Server): [MS]
        Client should send a ModeSwitch message after finishing the handshake.
        If the client want to upload file, send `MODESW.UPLOAD`
        (defined as NFHC_MODE_UPLOAD) or @AD (Client <=> Server):
            Phase 3.1: SelectionOffer: [SO]
                Server sends an unsigned int64 which indicates file count.
                Then, the server sends a `struct so_s2c_file_entry` array of all offered files.
            Phase 3.2: ContentTransfer: [CT]
                After deciding which file to download, the client sends id of the file to get.
                The server then send the whole file to the client.
    Phase 4: Quit (Client <=> Server): [Q]
        After all data has been received correctly, the receiver should send a `NFH.BYE`
        message to indicate an end. The other side should reply with another `NFH.BYTE`
        and close the connection.

*/

fsm_context *new_fsm_context(char *host, uint16_t port);
void del_fsm_context(fsm_context *ctx);
int client_send_file_preamble(int socket, FILE *fp, char *file_name);
int send_file(int socket, FILE *fp);
int receive_file(int socket, FILE *fp, u_int64_t file_size);
int send_handshake(int s);
int expect_handshake(int s);
int send_bye_message(int s);
int receive_bye_message(int s);

#endif