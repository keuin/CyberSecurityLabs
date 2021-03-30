#ifndef __LAYER3_H
#define __LAYER3_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/ethernet.h>

#include "layer4.h"

void l3_default(FILE *fp, const void *buf, size_t size);
void l3_ip(FILE *fp, const void *buf, size_t size);
void l3_ipv6(FILE *fp, const void *buf, size_t size);

#define IPV4(x) ((struct iphdr*)(x))

typedef void (layer3_handler)(FILE *, const void*, size_t);

#endif