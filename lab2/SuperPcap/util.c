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

/**
 * @brief Replace non-visible character with '.'
 * 
 * @param s string
 * @param n size
 */
static void safe_print(FILE* const fp, const char *s, size_t n)
{
    #define ORELSE(c) ((c < 32U || c > 127U)?('.'):((char)c))
    for (size_t i=0; i!=n; ++i)
    {
        fputc(ORELSE(s[i]), fp);
    }
    #undef ORELSE
}

/**
 * @brief Write n bytes from buffer to file. Data will be printed in hex and ascii representation.
 * 
 * @param fp file
 * @param buffer buffer
 * @param n length
 */
void binary_write(FILE* const fp, void* const buffer, size_t n)
{
    #define block_size 4
    #define line_size 16
    #define MAX(a,b) ((a>b)?(a):(b))
    #define GET_PRINT_LENGTH(cnt) ((cnt) / block_size * 1 + (cnt) * 3)
    char str[GET_PRINT_LENGTH(line_size) + 1] = {'\0'};
    unsigned char *b = buffer;
    for(; n>=line_size; n-=line_size)
    {
        fprintf(fp, "%02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  |  ",
            b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
            b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
        safe_print(fp, (char*)b, line_size);
        fputc('\n', fp);
        b += line_size;
    }
    if (n)
    {
        const unsigned char *line_header = b;
        const size_t block_count = n / block_size;
        memcpy(str, b, n);
        str[n] = '\0';
        for (int i=0; i != block_count; ++i)
        {
            fprintf(fp, "%02X %02X %02X %02X  ", b[0], b[1], b[2], b[3]);
            b += block_size;
        }
        
        for (int i=0; i!=n % 4; ++i)
        {
            fprintf(fp, "%02X ", *b++);
        }
        // padding, 3 spaces per byte
        
        const size_t k = GET_PRINT_LENGTH(line_size) - GET_PRINT_LENGTH(n);
        memset(str, ' ', k);
        str[k] = '\0';
        fprintf(fp, str);

        fprintf(fp, "|  ");
        safe_print(fp, (char*)line_header, n);
        fputc('\n', fp);
    }
    #undef line_size
    #undef block_size
    #undef GET_PRINT_LENGTH
    #undef MAX
}

// char test[256];
// int main()
// {
//     // TEST binary_write
//     for (int i=0; i!=256; ++i)
//     {
//         test[i] = (char)i;
//     }
//     binary_write(stdout, test, 256);
//     return 0;
// }