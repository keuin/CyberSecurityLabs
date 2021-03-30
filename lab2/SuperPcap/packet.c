#include "packet.h"
#include "util.h"

#define MAX_PACKET_STRING_SIZE  1023
#define ETHERNET(x) ((struct ether_header*)(x))

// char __packet_string_buffer[MAX_PACKET_STRING_SIZE + 1];

static inline char *get_net_layer_type(int ether_type)
{
    switch(ntohs(ether_type))
    {
        case ETHERTYPE_IP:
            return "IP";
        case ETHERTYPE_ARP:
            return "ARP";
        case ETHERTYPE_REVARP:
            return "Reverse ARP";
        case ETHERTYPE_IPV6:
            return "IPv6";
        case ETHERTYPE_LOOPBACK:
            return "Loopback";
        default:
            return "<Unknown>";
    }
}

void print_decoded_packet(FILE *fp, const uint8_t *p, size_t size)
{
    // Ethernet frame header
    fprintf(fp, "+ Layer-2 (Ethernet):\n");
    fprintf(fp, "Src: %02X:%02X:%02X:%02X:%02X:%02X\n", p[0], p[1], p[2], p[3], p[4], p[5]);
    fprintf(fp, "Dst: %02X:%02X:%02X:%02X:%02X:%02X\n", p[6], p[7], p[8], p[9], p[10], p[11]);
    const uint16_t layer3_type = ETHERNET(p)->ether_type;
    const char *layer3_type_str = get_net_layer_type(layer3_type);
    fprintf(fp, "Layer-3 protocol: 0x%04X (%s)\n\n", ntohs(layer3_type), layer3_type_str);

    fprintf(fp, "+ Layer 3 (%s):\n", layer3_type_str);
    layer3_handler *handler;
    switch(ntohs(layer3_type))
    {
        case ETHERTYPE_IP:
            handler = &l3_ip;
            break;
        case ETHERTYPE_IPV6:
            handler = &l3_ipv6;
            break;
        default:
            handler = &l3_default;
            break;
    }
    handler(fp, p+14, size-14); // 14: ethernet header size

    // char *net_layer_type = get_net_layer_type(ETHERNET(p)->ether_type);
    // DEBUGS(printf("Network access layer protocol: %" PRIu8 "\n", ETHERNET(p)->ether_type));
    // return net_layer_type;
}
