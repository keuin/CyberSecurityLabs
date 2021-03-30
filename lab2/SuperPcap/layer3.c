#include "layer3.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

static const char *ipv4_get_next_protocol(int s)
{
    switch(s)
    {
        case IPPROTO_TCP:
            return "TCP";
        case IPPROTO_UDP:
            return "UDP";
        case IPPROTO_ICMP:
            return "ICMP";
        default:
            return "<Unknown>";
    }
}

void l3_default(FILE *fp, const void *buf, size_t size)
{
    fprintf(fp, "<Unknown layer-3 payload>\n");
}

void l3_ip(FILE *fp, const void *buf, size_t size)
{
    #define BINARY(x) ((x)?1:0)
    #define BYTE1(x) (((uint32_t)x) & 0xFFU)
    #define BYTE2(x) ((((uint32_t)x) & 0xFF00U) >> 8)
    #define BYTE3(x) ((((uint32_t)x) & 0xFF0000U) >> 16)
    #define BYTE4(x) ((((uint32_t)x) & 0xFF000000U) >> 24)
    // fprintf(fp, "IP Datagram:\n");
    fprintf(fp, "Version: 0x%X\n", IPV4(buf)->version); // FIX THIS
    const unsigned int ihl = IPV4(buf)->ihl & 0x0FU;
    const uint32_t ipv4_header_bytes = ihl * 4U;
    fprintf(fp, "IHL (Internet Header Length): %d (%d bytes)\n", ihl, ipv4_header_bytes);
    fprintf(fp, "Payload size: %" PRIu16 " bytes\n", ntohs(IPV4(buf)->tot_len));
    fprintf(fp, "Identification: 0x%04X\n", ntohs(IPV4(buf)->id));
    unsigned int flags = IPV4(buf)->frag_off >> 13U;
    fprintf(fp, "Flags: %d%d%d\n", BINARY(flags&4), BINARY(flags&2), BINARY(flags&1));
    fprintf(fp, "Fragment Offset: %u\n",  ntohs(IPV4(buf)->frag_off & 0x1FFFU));
    fprintf(fp, "TTL: %d\n", IPV4(buf)->ttl);
    const uint8_t l4_proto = IPV4(buf)->protocol;
    const char *l4_proto_str = ipv4_get_next_protocol(l4_proto);
    fprintf(fp, "Layer-4 Protocol: %u (%s)\n", l4_proto, l4_proto_str);
    fprintf(fp, "Checksum: 0x%4X\n", IPV4(buf)->check);
    int s = IPV4(buf)->saddr, d = IPV4(buf)->daddr;
    fprintf(fp, "Source Address: %d.%d.%d.%d\n", BYTE1(s), BYTE2(s), BYTE3(s), BYTE4(s));
    fprintf(fp, "Dest. Address: %d.%d.%d.%d\n", BYTE1(d), BYTE2(d), BYTE3(d), BYTE4(d));

    fprintf(fp, "\n");
    fprintf(fp, "+ Layer 4 (%s):\n", l4_proto_str);
    layer4_handler *handler;
    switch(l4_proto)
    {
        case IPPROTO_TCP:
            handler = &l4_tcp;
            break;
        case IPPROTO_UDP:
            handler = &l4_udp;
            break;
        case IPPROTO_ICMP:
            handler = &l4_icmp;
            break;
        default:
            handler = &l4_default;
            break;
    }
    handler(fp, buf+ipv4_header_bytes, size - ipv4_header_bytes);
    #undef BINARY
    #undef BYTE1
    #undef BYTE2
    #undef BYTE3
    #undef BYTE4
}

void l3_ipv6(FILE *fp, const void *buf, size_t size)
{
    fprintf(fp, "<IPv6 fragment>\n");
}