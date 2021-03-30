#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// #define DEBUGON
#ifdef DEBUGON
#define ASSERT(s) __assertion((s), __FILE__, __LINE__, NULL)
#define ASSERT2(s, m) __assertion((s), __FILE__, __LINE__, (m))
#define __DEBUG(s) __debug(s, __FILE__, __func__, __LINE__)
#define DEBUGS(seq) seq
#else
#define ASSERT(s)
#define ASSERT2(s, m)
#define __DEBUG(s)
#define DEBUGS(seq)
#endif

void __debug(const char *s, const char *f, const char* func, const int l);
void __assertion(int s, char *f, int l, char *m);
char *get_ip_str(struct sockaddr *sa);
void binary_write(FILE* const fp, void* const buffer, size_t n);

#endif