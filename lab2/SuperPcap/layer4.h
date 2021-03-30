#ifndef __LAYER4_H
#define __LAYER4_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/ethernet.h>

typedef void (layer4_handler)(FILE *, const void*, size_t);

void l4_tcp(FILE *fp, const void *buf, size_t n);
void l4_udp(FILE *fp, const void *buf, size_t n);
void l4_icmp(FILE *fp, const void *buf, size_t n);
void l4_default(FILE *fp, const void *buf, size_t n);

#endif