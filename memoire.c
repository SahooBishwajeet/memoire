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
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [key]\n", argv[0]);
        return 2;
    }
    int listMode = (argc == 1);
    char *search = NULL;

    const char *filePath = "./data.txt";
    FILE *fp = fopen(filePath, "r");
    if (!fp) {
        fprintf(stderr, "Error opening '%s' : '%s'\n", filePath, strerror(errno));
        return 1;
    }

    if (!listMode) {
        search = strdup(argv[1]);
        if (!search) {
            fprintf(stderr, "Memory Error\n");
            fclose(fp);
            return 1;
        }

        char *temp = trim(search);
        if (temp != search) {
            memmove(search, temp, strlen(temp) + 1);
        }
    }
    int found = 0;

    char *line = NULL;    // buffer to store 1 input line
    size_t capacity = 0;  // current buffer capacity
    ssize_t nread;        // n-chars returned by getline()

    while ((nread = getline(&line, &capacity, fp)) != -1) {
        if (nread <= 0) continue;

        // remove trailing \n, \r & windows-style \r\n
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[--nread] = '\0';
        }

        // get dilimiter ":"
        char *delim = strchr(line, ':');
        if (!delim) {
            if (!listMode) {
                fprintf(stderr, "Skipping line : %s [No Separator ':']\n", line);
            }
            continue;
        }
        *delim = '\0';

        // now line contains key; delim + 1 contains value
        char *key = trim(line);
        char *value = trim(delim + 1);

        if (listMode) {
            if (key == NULL || key[0] == '\0') continue;
            printf("%s: %s\n", key, value ? value : "");
        } else {
            if (key && strcmp(key, search) == 0) {
                printf("%s: %s\n", key, value ? value : "");
                found = 1;
                break;
            }
        }
    }

    free(line);
    fclose(fp);

    free(search);
    (void)found;

    return 0;
}
