#ifndef __PACKET_H
#define __PACKET_H

#include <stdint.h>
#include <inttypes.h>
#include <linux/if_ether.h> // struct ethhdr
#include <linux/ip.h>       // struct iphdr
#include <linux/tcp.h>      // struct tcphdr
#include <linux/udp.h>      // struct udphdr

char *packet_to_string(uint8_t *packet);

#endif