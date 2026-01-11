#include <stdint.h>
#include <stddef.h>
#include "header/stdlib/string.h"

void *memset(void *s, int c, size_t n)
{
    uint8_t *buf = (uint8_t *)s;
    for (size_t i = 0; i < n; i++)
        buf[i] = (uint8_t)c;
    return s;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *dstbuf = (uint8_t *)dest;
    const uint8_t *srcbuf = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
        dstbuf[i] = srcbuf[i];
    return dstbuf;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *buf1 = (const uint8_t *)s1;
    const uint8_t *buf2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++)
    {
        if (buf1[i] < buf2[i])
            return -1;
        else if (buf1[i] > buf2[i])
            return 1;
    }

    return 0;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *dstbuf = (uint8_t *)dest;
    const uint8_t *srcbuf = (const uint8_t *)src;
    if (dstbuf < srcbuf)
    {
        for (size_t i = 0; i < n; i++)
            dstbuf[i] = srcbuf[i];
    }
    else
    {
        for (size_t i = n; i != 0; i--)
            dstbuf[i - 1] = srcbuf[i - 1];
    }

    return dest;
}

/**
 * Return the length of the null-terminated string s.
 */
size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
    {
        p++;
    }
    return (size_t)(p - s);
}

/**
 * Compare two null-terminated strings lexicographically.
 * Returns <0 if s1<s2, 0 if equal, >0 if s1>s2.
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

/**
 * Locate the last occurrence of c in the string s.
 * Returns a pointer to the character, or NULL if not found.
 */
char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    char ch = (char)c;
    while (*s)
    {
        if (*s == ch)
            last = s;
        s++;
    }
    if (ch == '\0')
        return (char *)s;
    return (char *)last;
}

/**
 * Concatenate src to the end of dest.
 * dest must have enough space.
 * Returns dest.
 */
char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d)
        d++;
    while ((*d++ = *src++))
        ;
    return dest;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *p = buf;
    for (; *fmt; fmt++)
    {
        if (*fmt != '%')
        {
            *p++ = *fmt;
            continue;
        }

        fmt++;
        int width = 0, pad_zero = 0;

        // Check for padding
        if (*fmt == '0')
        {
            pad_zero = 1;
            fmt++;
            while (*fmt >= '0' && *fmt <= '9')
            {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        if (*fmt == 'd')
        {
            int val = va_arg(args, int);
            char tmp[16];
            int i = 0, neg = 0;
            if (val < 0)
            {
                neg = 1;
                val = -val;
            }
            do
            {
                tmp[i++] = '0' + (val % 10);
                val /= 10;
            } while (val);

            if (neg)
                tmp[i++] = '-';

            // Add padding
            while (i < width)
                tmp[i++] = pad_zero ? '0' : ' ';

            while (i--)
                *p++ = tmp[i];
        }
        else if (*fmt == 's')
        {
            char *s = va_arg(args, char *);
            while (*s)
                *p++ = *s++;
        }
        else if (*fmt == 'c')
        {
            char c = (char)va_arg(args, int);
            *p++ = c;
        }
        else
        {
            *p++ = '%';
            *p++ = *fmt;
        }
    }

    *p = '\0';
    va_end(args);
    return (int)(p - buf);
}

int str_to_int(const char *s)
{
    int res = 0;
    while (*s >= '0' && *s <= '9')
    {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

char *strcpy(char *dest, const char *src)
{
    if (dest == NULL || src == NULL)
    {
        return NULL;
    }

    char *original_dest = dest;

    while ((*dest++ = *src++) != '\0')
        ;

    return original_dest;
}

static char *g_last_token = NULL;

static int is_delimiter(char c, const char *delim)
{
    while (*delim != '\0')
    {
        if (c == *delim)
        {
            return 1;
        }
        delim++;
    }
    return 0;
}

char *strtok(char *str, const char *delim)
{
    char *token_start;

    if (str != NULL)
    {
        g_last_token = str;
    }
    else if (g_last_token == NULL)
    {
        return NULL;
    }

    while (*g_last_token != '\0' && is_delimiter(*g_last_token, delim))
    {
        g_last_token++;
    }

    if (*g_last_token == '\0')
    {
        g_last_token = NULL;
        return NULL;
    }

    token_start = g_last_token;

    while (*g_last_token != '\0' && !is_delimiter(*g_last_token, delim))
    {
        g_last_token++;
    }

    if (*g_last_token == '\0')
    {
        g_last_token = NULL;
    }
    else
    {
        *g_last_token = '\0';

        g_last_token++;
    }

    return token_start;
}

char *strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0')
    {
        return (char *)haystack;
    }

    for (const char *h = haystack; *h != '\0'; h++)
    {
        const char *h_temp = h;
        const char *n = needle;

        while (*h_temp == *n && *h_temp != '\0')
        {
            h_temp++;
            n++;
        }

        if (*n == '\0')
        {
            return (char *)h;
        }
    }

    return NULL;
}

void uint_to_str(uint32_t num, char *str)
{
    if (num == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int i = 0;
    char temp[16];
    while (num > 0)
    {
        temp[i++] = (num % 10) + '0';
        num /= 10;
    }

    int j = 0;
    while (i > 0)
    {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

void int_to_str(int32_t num, char *str)
{
    int i = 0;
    int is_negative = 0;

    // Handle 0 secara khusus
    if (num == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    // Handle kasus angka paling kecil (INT_MIN) secara manual jika perlu,
    // tapi untuk keperluan ini cukup handle negatif standar.
    if (num < 0)
    {
        is_negative = 1;
        num = -num;
    }

    char temp[16];
    while (num > 0)
    {
        temp[i++] = (num % 10) + '0';
        num /= 10;
    }

    int j = 0;
    if (is_negative)
    {
        str[j++] = '-';
    }

    while (i > 0)
    {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}