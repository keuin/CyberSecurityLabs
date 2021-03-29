#include "packet.h"
#include "util.h"

#define MAX_PACKET_STRING_SIZE  1023
#define ETHERNET(x) ((struct ethhdr*)x)

char __packet_string_buffer[MAX_PACKET_STRING_SIZE + 1];

char *packet_to_string(uint8_t *packet)
{
    DEBUGS(printf("Ethernet proto: %" PRIu8 "\n", ETHERNET(packet)->h_proto));
    return "dummy string";
}
