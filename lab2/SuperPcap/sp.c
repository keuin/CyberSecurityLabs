#include "util.h"
#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pcap/pcap.h>
#include <sys/types.h>

#define OPTIONAL(x, y) ((x) ? (x) : (y))
#define MAX_ADDRESS_PRINT_BUF 1023
#define TRIM1(s) ((*s==' ') ? (s+1):(s))

#define ERR_PCAP_CANNOT_ENUM_IFACES     -10
#define ERR_PCAP_CANNOT_OPEN_IFACE      -20
#define ERR_PCAP_CANNOT_ACTIVIATE_IFACE -30

#define eprintf(...) fprintf (stderr, __VA_ARGS__)

static void captured_packet_handler(u_char *user_parameter, const struct pcap_pkthdr *packet_header, const u_char *packet);

char errbuf[PCAP_ERRBUF_SIZE] = {'\0'};
pcap_t *inst = NULL;
int link_layer_type;
FILE *of = NULL;

static void signal_handler(int s)
{
    if (s == SIGINT && inst)
    {
        puts("Stopping...");
        pcap_breakloop(inst);
        pcap_close(inst);
        puts("SuperPcap is stopped.");
        fflush(of);
        exit(0);
    }
}

int main(void)
{
    of = stdout;
    signal(SIGINT, signal_handler);
    setbuf(stdout, NULL);
    puts("SuperPcap: A packet capturing tool based on libpcap.");
    printf("pcap version: %s\n\n", pcap_lib_version());

    // print interfaces
    puts("Available interfaces:");
    pcap_if_t *devlist;
    if (pcap_findalldevs(&devlist, errbuf))
    {
        eprintf("Cannot list pcap interfaces: %s\n", errbuf);
        return -10;
    }
    pcap_if_t *iter = devlist;
    int max_dev_id = -1; // max index in devlist
    int i = 0;
    char address_print_buf[MAX_ADDRESS_PRINT_BUF + 1];
    int bp = 0; // buf pointer (< MAX_ADDRESS_PRINT_BUF)
    while (iter)
    {
        ++max_dev_id;
        // print addresses into buffer
        bp = 0;
        pcap_addr_t *address_list = iter->addresses;
        while (address_list)
        {
            char *addr = get_ip_str(address_list->addr);
            size_t addr_len = strlen(addr);
            if (bp + addr_len >= MAX_ADDRESS_PRINT_BUF)
                break; // buffer is full, stop
            memcpy(&address_print_buf[bp], addr, addr_len);
            bp += addr_len;
            address_print_buf[bp++] = ' '; // split
            address_list = address_list->next;
        }
        bp && (address_print_buf[bp - 1] = '\0'); // end

        // print interface information
        if (*address_print_buf)
            printf("[%2d] %20s: (%s) %s\n", i++, iter->name,
                TRIM1(address_print_buf), OPTIONAL(iter->description, ""));
        else
            printf("[%2d] %20s:%s %s\n", i++, iter->name,
                TRIM1(address_print_buf), OPTIONAL(iter->description, ""));
        iter = iter->next;
    }

    // input selection
    puts("");
    int selection = -1;
    do
    {
        printf("Select an interface:");
    } while (scanf("%d", &selection)!= 1 || selection < 0 || selection > max_dev_id); // select interface to use
    
    // extract the interface that we need
    pcap_if_t *iface = devlist; // the interface to use
    for (int i=0; i!=selection; ++i)
        iface = iface->next;

    // start capturing
    if (!(inst = pcap_create(iface->name, errbuf)))
    {
        eprintf("Cannot open device `%s`: %s\n", iface->name, errbuf);
        pcap_freealldevs(devlist);
        return ERR_PCAP_CANNOT_OPEN_IFACE;
    }
    
    // TODO: set filter condition to `inst` here
    // currently we do not implement that

    // set timeout
    pcap_set_timeout(inst, 200);
    pcap_set_buffer_size(inst, 4096);

    // activate
    int pcap_errno;
    if ((pcap_errno = pcap_activate(inst)))
    {
        eprintf("Cannot activate interface %s", iface->name);
FAIL_BEFORE_ACTIVATE:
        pcap_perror(inst, "");
        pcap_freealldevs(devlist);
        pcap_close(inst);
        return ERR_PCAP_CANNOT_ACTIVIATE_IFACE;
    }

    // set link-layer type
    link_layer_type = pcap_datalink(inst);

    if (link_layer_type != DLT_EN10MB)
    {
        eprintf("Unsupported link-layer protocol: %d.\n", link_layer_type);
        goto FAIL_BEFORE_ACTIVATE;
    }

    // ask file name or just print to stdout
    char c = 0;
    do
    {
        if (c != '\n' && c != '\r')
            printf("Where would you like to print capture logs? (f:File, s:STDOUT) ");
    } while (scanf("%c", &c) != 1 || (c != 'f' && c != 'F' && c != 's' && c != 'S'));
    if (c == 'f' || c == 'F')
    {
        // ask for file name
        char file_name[64];
        do
        {
            if (!of)
                perror("Cannot open file");
            printf("Save to:");
        } while(scanf("%63s", file_name) != 1 || !(of = fopen(file_name, "w")));
        printf("Scan result will be saved in file `%s`.\n", file_name);
    }
    else
    {
        printf("Scan result will be printed to STDOUT.\n");
        of = stdout;
    }

    // here we capture packets
    pcap_loop(inst, -1, captured_packet_handler, NULL);

    // finish capturing
    pcap_close(inst);
    puts("SuperPcap is stopped.");
    fflush(of);
    return 0;
}

static void captured_packet_handler(u_char *__, const struct pcap_pkthdr *packet_header, const u_char *packet)
{
    // handle all captured packets
    // printf("Packet %" PRIu32 " bytes.\n", packet_header->len);

    // print capture time
    fprintf(of, "======== FRAME START ========\n");
    fprintf(of, "[Frame] time=%ld.%06ld", packet_header->ts.tv_sec, packet_header->ts.tv_usec);

    char tmbuf[64];
    strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", localtime(&packet_header->ts.tv_sec));
    fprintf(of, " [%s.%06ld], ", tmbuf, packet_header->ts.tv_usec);

    // print packet size
    if (packet_header->len != packet_header->caplen)
    {
        fprintf(of, "%" PRIu32 " bytes captured (%" PRIu32 " bytes in total) ",
            packet_header->caplen, packet_header->len);
    }
    else
    {
        fprintf(of, "%" PRIu32 " bytes ", packet_header->len);
    }

    // print packet data
    fprintf(of, "\n\n");
    // fprintf(of, "Decoded information:\n");
    print_decoded_packet(of, (const u_int8_t*)packet, packet_header->caplen);

    fprintf(of, "\n");
    fprintf(of, "Link-layer frame data:\n");
    binary_write(of, (void *const)packet, packet_header->caplen);
    fputs("========  FRAME END  ========\n\n", of);
}