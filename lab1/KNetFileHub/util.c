#include "util.h"

/**
 * @brief Check if a string ends in specific length.
 * 
 * @param buf the string buffer.
 * @param max_length the max length desired. Ending '\0' is not included.
 * @return int 1 if valid, 0 if invalid
 */
int is_string_buf_valid(char *buf, unsigned max_length)
{
    for (int i = 0; i < max_length + 1; ++i)
    {
        if (buf[i] == '\0')
            return 1;
    }
    return 0;
}

/**
 * @brief Check if the file name is valid.
 * 
 * @param s a valid C string.
 * @return int 1 if valid, 0 if invalid.
 */
int is_valid_file_name(char *s)
{
    int c;
    while ((c = *s++) != '\0')
    {
        if (c == '\\' || c == '/' || c == '*' 
            || c == '?' || c == '<' || c == '>' || c == '|')
            return 0;
    }
    return 1;
}

/**
 * @brief Read exactly n bytes from a file. This function has the same signature with read().
 * 
 * @param fd the file discriptor to read.
 * @param buf the buffer to save read data.
 * @param n bytes to read. Exactly n bytes will be read.
 * @return n if success, -1 if failed.
 */
ssize_t read_exactly(const int fd, const void *__buf, const size_t n)
{
    if (n < 0 || !fd || !__buf)
        return -1;
    char *buf = (char*)__buf;
    size_t bytes_remaining = n;
    ssize_t sz_read;
    while (bytes_remaining)
    {
        if ((sz_read = read(fd, buf, bytes_remaining)) < 0)
        {
            // failed
            perror("read_exactly failed");
            return -1;
        }
        else if (sz_read > 0)
        {
            // success
            // move the buf pointer, update bytes_remaining
            bytes_remaining -= sz_read;
            buf += sz_read;
            DEBUGS(printf("[DEBUG] Read %zd bytes. Remaining %zd bytes.\n", sz_read, bytes_remaining));
        }
        else
        {
            // unexpected EOF
            int errsv = errno;
            __DEBUG("Unexpected EOF encountered while reading");
            errno = errsv;
            return -1;
        }
    }
    return n;
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