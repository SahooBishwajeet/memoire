#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    if (!s) return NULL;

    // Skip leading whitespace
    while (*s && isspace((unsigned char)*s)) s++;

    if (*s == '\0') return s;  // string was all whitespace

    // Move backward over trailing whitespace
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';
    return s;
}

int main(int argc, char const *argv[]) {
    const char *filePath = "./data.txt";
    FILE *fp = fopen(filePath, "r");
    if (!fp) {
        fprintf(stderr, "Error opening '%s' : '%s'", filePath, strerror(errno));
        return 1;
    }

    char *line = NULL;    // buffer to store 1 input line
    size_t capacity = 0;  // current buffer capacity
    ssize_t nread;        // n-chars returned by getline()

    while ((nread = getline(&line, &capacity, fp)) != -1) {
        if (nread <= 0) continue;

        // remove trailing \n, \r & windows-style \r\n
        if (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[nread - 1] = '\0';
            nread--;
            if (nread > 0 && line[nread - 1] == '\r') {
                line[nread - 1] = '\0';
                nread--;
            }
        }

        // get dilimiter ":"
        char *delim = strchr(line, ':');
        if (!delim) {
            fprintf(stderr, "Skipping line : %s [No Separator ':']", line);
            continue;
        }
        *delim = '\0';

        // now line contains key; delim + 1 contains value
        char *key = trim(line);
        char *value = trim(delim + 1);

        printf("Key : '%s' || Value : '%s'\n", key, value);
    }

    free(line);
    fclose(fp);

    return 0;
}
