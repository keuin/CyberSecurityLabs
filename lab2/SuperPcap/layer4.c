#include "layer4.h"

void l4_tcp(FILE *fp, const void *buf, size_t n)
{
    #define BINARY(x) ((x)?1:0)
    #define CWR(x) BINARY(x&128)
    #define ECE(x) BINARY(x&64)
    #define URG(x) BINARY(x&32)
    #define ACK(x) BINARY(x&16)
    #define PSH(x) BINARY(x&8)
    #define RST(x) BINARY(x&4)
    #define SYN(x) BINARY(x&2)
    #define FIN(x) BINARY(x&1)
    const struct tcphdr* tcp = buf;
    fprintf(fp, "Source Port: %u\n", ntohs(tcp->th_sport));
    fprintf(fp, "Dest. Port: %u\n", ntohs(tcp->th_dport));
    fprintf(fp, "Sequence Num.: %u\n", ntohl(tcp->th_seq));

    fprintf(fp, "Flags: ");
    const uint8_t flg = tcp->th_flags;
    if (FIN(flg))
        fprintf(fp, "FIN ");
    if (SYN(flg))
        fprintf(fp, "SYN ");
    if (RST(flg))
        fprintf(fp, "RST ");
    if (PSH(flg))
        fprintf(fp, "PSH ");
    if (ACK(flg))
        fprintf(fp, "ACK ");
    if (URG(flg))
        fprintf(fp, "URG ");
    fprintf(fp, "\n");

    if (ACK(flg))
    {
        fprintf(fp, "ACK Num.: %u\n", tcp->th_ack);
    }
    
    fprintf(fp, "Window Size: %u\n", tcp->th_win);
    fprintf(fp, "Checksum: 0x%4X\n", tcp->th_sum);

    #undef BINARY
    #undef CWR
    #undef ECE
    #undef URG
    #undef ACK
    #undef PSH
    #undef RST
    #undef SYN
    #undef FIN
}

void l4_udp(FILE *fp, const void *buf, size_t n)
{
    const struct udphdr *udp = buf;
    fprintf(fp, "Source Port: %u\n", ntohs(udp->uh_sport));
    fprintf(fp, "Dest. Port: %u\n", ntohs(udp->uh_dport));
    uint16_t length = ntohs(udp->uh_ulen);
    fprintf(fp, "Length: %u bytes (%u bytes of layer-7 payload)\n", length, length - 8U);
    fprintf(fp, "Checksum: 0x%4X\n", udp->uh_sum);
}

void l4_icmp(FILE *fp, const void *buf, size_t n)
{
    fprintf(fp, "<ICMP datagram>\n");
}

void l4_default(FILE *fp, const void *buf, size_t n)
{
    fprintf(fp, "<Unknown layer-4 payload>\n");
}