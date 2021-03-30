#ifndef __PACKET_H
#define __PACKET_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
// #include <linux/if_ether.h> // struct ethhdr
// #include <linux/ip.h>       // struct iphdr
// #include <linux/tcp.h>      // struct tcphdr
// #include <linux/udp.h>      // struct udphdr
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/ethernet.h>

#include "layer3.h"

void print_decoded_packet(FILE *fp, const uint8_t *packet, size_t size);

#endif