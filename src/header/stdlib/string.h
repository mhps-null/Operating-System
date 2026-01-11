#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/**
 * C standard memset, check man memset or
 * https://man7.org/linux/man-pages/man3/memset.3.html for more details
 *
 * @param s Pointer to memory area to set
 * @param c Constant byte value for filling memory area
 * @param n Memory area size in byte
 *
 * @return Pointer s
 */
void *memset(void *s, int c, size_t n);

/**
 * C standard memcpy, check man memcpy or
 * https://man7.org/linux/man-pages/man3/memcpy.3.html for more details
 *
 * @param dest Starting location for memory area to set
 * @param src Pointer to source memory
 * @param n Memory area size in byte
 *
 * @return Pointer dest
 */
void *memcpy(void *restrict dest, const void *restrict src, size_t n);

/**
 * C standard memcmp, check man memcmp or
 * https://man7.org/linux/man-pages/man3/memcmp.3.html for more details
 *
 * @param s1 Pointer to first memory area
 * @param s2 Pointer to second memory area
 * @param n Memory area size in byte
 *
 * @return Integer as error code, zero for equality, non-zero for inequality
 */
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * C standard memmove, check man memmove or
 * https://man7.org/linux/man-pages/man3/memmove.3.html for more details
 *
 * @param dest Pointer to destination memory
 * @param src Pointer to source memory
 * @param n Memory area size in byte
 *
 * @return Pointer dest
 */
void *memmove(void *dest, const void *src, size_t n);

#/**
 * C standard strlen, returns length of a null-terminated string
 * 
 * @param s Pointer to null-terminated string
 * @return Number of characters preceding the null terminator
*/
size_t strlen(const char *s);

/**
 * C standard strcmp, lexicographically compare two null-terminated strings
 *
 * @param s1 Pointer to first string
 * @param s2 Pointer to second string
 * @return Integer less than, equal to, or greater than zero if s1 is found,
 * respectively, to be less than, to match, or be greater than s2
 */
int strcmp(const char *s1, const char *s2);

/**
 * C standard strrchr, locate last occurrence of character in string
 *
 * @param s Pointer to null-terminated string
 * @param c Character to locate (converted to char)
 * @return Pointer to the last occurrence of c in s, or NULL if not found
 */
char *strrchr(const char *s, int c);

/**
 * C standard strcat, concatenate two null-terminated strings
 *
 * @param dest Pointer to destination string (must be large enough)
 * @param src Pointer to source string
 * @return Pointer to dest
 */
char *strcat(char *dest, const char *src);

/**
 * C standard sprintf, format and store a string
 *
 * @note This is a simplified version of sprintf, only supports %d, %s, and %c
 * @param buf Pointer to destination buffer
 * @param fmt Pointer to format string
 * @param ... Additional arguments to format
 * @return Number of characters written to buf
 */
int sprintf(char *buf, const char *fmt, ...);

/**
 * Convert a string to an integer
 *
 * @param s Pointer to null-terminated string
 * @return Integer value of the string
 * @note Similiar to `stoi`, but only supports decimal strings (0-9)
 */
int str_to_int(const char *s);

char *strcpy(char *dest, const char *src);

char *strtok(char *str, const char *delim);

char *strstr(const char *haystack, const char *needle);

void uint_to_str(uint32_t num, char *str);
void int_to_str(int32_t num, char *str);

#endif /* _STRING_H */