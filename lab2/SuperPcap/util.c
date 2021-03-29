#include "util.h"

// Convert a struct sockaddr address to a string, IPv4 and IPv6:
static char __get_ip_str_buffer[256];
char *get_ip_str(struct sockaddr *sa)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    __get_ip_str_buffer, 255);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    __get_ip_str_buffer, 255);
            break;

        case AF_PACKET:
            return "";

        default:
            return "<Unknown address>";
    }

    return __get_ip_str_buffer;
}

/**
 * @brief Assertion function for C.
 * 
 * @param s statement.
 * @param f file name.
 * @param l line number.
 * @param m custom message. may be NULL.
 */
inline void __assertion(int s, char *f, int l, char *m)
{
    if (!s)
    {
        if (!m)
            fprintf(stderr, "Assertion failed (%s:%d).\n", f, l);
        else
            fprintf(stderr, "Assertion failed (%s:%d): %s.\n", f, l, m);
        exit(-200);
    }
}

/**
 * @brief Print a message with [DEBUG] prefix.
 * 
 * @param s the message to be printed.
 * @param f the source file name.
 * @param func the function name.
 * @param l line number.
 */
inline void __debug(const char *s, const char *f, const char* func, const int l)
{
    printf("[DEBUG] (%s:%d:%s) %s\n", f, l, func, s);
}