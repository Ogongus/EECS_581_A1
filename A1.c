/*
 * Reads lines of text and extracts a single valid IPv4 address, optionally
 * followed by :port, embedded anywhere in the line.
 *
 * Approach:
 *   1. Split the line into maximal runs of candidate characters
 *      (digits, '.', ':'). Everything else is garbage and separates runs.
 *   2. Each run must match the grammar IN FULL:
 *          octet '.' octet '.' octet '.' octet [ ':' port ]
 *      No truncation, no searching for a valid piece inside a longer run.
 *      This automatically rejects stray '.'/':' adjacent to an address,
 *      a second colon, extra octets, etc.
 *   3. The first run that validates is the extracted address.
 *
 * No string-to-number, address-parsing, or regex library functions are used.
 * 
 * 100% of this code was written using Claude Opus 5.5
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* A character that can be part of a candidate token. */
static int isTokenChar(char c)
{
    return isdigit((unsigned char)c) || c == '.' || c == ':';
}

/*
 * Parse one number starting at tok[*pos] (tok has length len).
 * Consumes ALL consecutive digits, then checks:
 *   - at least 1 digit, at most maxDigits
 *   - no leading zero unless the value is exactly "0"
 *   - value <= maxValue
 * Digit accumulation is done by hand.
 * Returns 1 on success (value in *out, *pos advanced), 0 on failure.
 */
static int parseNumber(const char *tok, size_t len, size_t *pos,
                       int maxDigits, long maxValue, long *out)
{
    size_t start = *pos;
    size_t i = start;
    long value = 0;
    int digits = 0;

    while (i < len && isdigit((unsigned char)tok[i])) {
        digits++;
        if (digits > maxDigits) {
            return 0;                 /* too many digits */
        }
        value = value * 10 + (tok[i] - '0');
        i++;
    }

    if (digits == 0) {
        return 0;                     /* empty field */
    }
    if (digits > 1 && tok[start] == '0') {
        return 0;                     /* disallowed leading zero */
    }
    if (value > maxValue) {
        return 0;                     /* out of range */
    }

    *out = value;
    *pos = i;
    return 1;
}

/*
 * Validate one complete candidate token of length len.
 * Returns 1 if the entire token is a valid address[:port].
 */
static int validateToken(const char *tok, size_t len,
                         unsigned long *outAddress, int *outPort)
{
    size_t pos = 0;
    unsigned long address = 0;
    long value;
    int octet;

    for (octet = 0; octet < 4; octet++) {
        if (octet > 0) {
            if (pos >= len || tok[pos] != '.') {
                return 0;             /* missing separator / wrong char */
            }
            pos++;
        }
        if (!parseNumber(tok, len, &pos, 3, 255, &value)) {
            return 0;
        }
        address = (address << 8) | (unsigned long)value;
    }

    if (pos == len) {                 /* address only, no port */
        *outAddress = address;
        *outPort = -1;
        return 1;
    }

    /* Only a colon may follow the fourth octet. */
    if (tok[pos] != ':') {
        return 0;                     /* e.g. a fifth octet or trailing '.' */
    }
    pos++;

    if (!parseNumber(tok, len, &pos, 5, 65535, &value)) {
        return 0;                     /* invalid port rejects everything */
    }
    if (pos != len) {
        return 0;                     /* anything after the port (e.g. ':' '.') */
    }

    *outAddress = address;
    *outPort = (int)value;
    return 1;
}

int extractIPv4(const char *str, unsigned long *outAddress, int *outPort)
{
    size_t i = 0;

    *outAddress = 0;
    *outPort = -1;

    if (str == NULL) {
        return 0;
    }

    while (str[i] != '\0') {
        size_t start, len;

        /* Skip garbage. */
        if (!isTokenChar(str[i])) {
            i++;
            continue;
        }

        /* Collect a maximal run of candidate characters. */
        start = i;
        while (str[i] != '\0' && isTokenChar(str[i])) {
            i++;
        }
        len = i - start;

        {
            unsigned long addr;
            int port;
            if (validateToken(str + start, len, &addr, &port)) {
                *outAddress = addr;
                *outPort = port;
                return 1;             /* exactly one address per line */
            }
        }
    }

    return 0;
}

/*
 * Read an entire line of arbitrary length from stdin.
 * Returns a malloc'd string without the trailing newline, or NULL on EOF.
 */
static char *readLine(void)
{
    size_t cap = 128, len = 0;
    char *buf = malloc(cap);
    int c;

    if (buf == NULL) {
        return NULL;
    }

    while ((c = getchar()) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            char *tmp;
            cap *= 2;
            tmp = realloc(buf, cap);
            if (tmp == NULL) {
                free(buf);
                return NULL;
            }
            buf = tmp;
        }
        buf[len++] = (char)c;
    }

    if (c == EOF && len == 0) {
        free(buf);
        return NULL;
    }

    /* Tolerate Windows line endings. */
    if (len > 0 && buf[len - 1] == '\r') {
        len--;
    }
    buf[len] = '\0';
    return buf;
}

int main(void)
{
    for (;;) {
        char *line;
        unsigned long address;
        int port;

        printf("Enter a line of text (END to quit): ");
        fflush(stdout);

        line = readLine();
        if (line == NULL) {           /* EOF behaves like END */
            break;
        }
        if (strcmp(line, "END") == 0) {
            free(line);
            break;
        }

        if (extractIPv4(line, &address, &port)) {
            printf("Extracted IPv4 address: %lu.%lu.%lu.%lu (decimal value: %lu, port: ",
                   (address >> 24) & 0xFFUL,
                   (address >> 16) & 0xFFUL,
                   (address >> 8) & 0xFFUL,
                   address & 0xFFUL,
                   address);
            if (port >= 0) {
                printf("%d)\n", port);
            } else {
                printf("none)\n");
            }
        } else {
            printf("No valid IPv4 address found.\n");
        }

        free(line);
    }

    printf("Program terminated.\n");
    return 0;
}