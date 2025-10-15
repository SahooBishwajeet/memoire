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

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [options]            # list entries (default)\n"
            "  %s [options] <key>      # get a key\n"
            "Options:\n"
            "  -f, --file <path>       Use data file (default ./data.txt)\n"
            "  -y, --yes               Assume yes for confirmations (future use)\n"
            "  -h, --help              Show this help\n",
            prog, prog);
}

int main(int argc, char const *argv[]) {
    const char *prog = argc > 0 ? argv[0] : "memoire";

    const char *filePath = "./data.txt";
    int assumeYes = 0;

    int idx = 1;
    while (idx < argc && argv[idx] && argv[idx][0] == '-') {
        if (strcmp(argv[idx], "-f") == 0 || strcmp(argv[idx], "--file") == 0) {
            if (idx + 1 >= argc) {
                fprintf(stderr, "Missing argument (filename) for %s\n", argv[idx]);
                usage(prog);
                return 2;
            }

            filePath = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-y") == 0 || strcmp(argv[idx], "--yes") == 0) {
            assumeYes = 1;
            idx++;
        } else if (strcmp(argv[idx], "-h") == 0 || strcmp(argv[idx], "--help") == 0) {
            usage(prog);
            return 0;
        } else {
            fprintf(stderr, "Unknown options: %s\n", argv[idx]);
            usage(prog);
            return 2;
        }
    }

    if ((argc - idx) > 1) {
        fprintf(stderr, "Too Many arguments\n");
        usage(prog);
        return 2;
    }
    int listMode = ((argc - idx) == 0);
    char *search = NULL;

    if (!listMode) {
        search = strdup(argv[idx]);
        if (!search) {
            fprintf(stderr, "Memory Error\n");
            return 1;
        }

        char *temp = trim(search);
        if (temp != search) {
            memmove(search, temp, strlen(temp) + 1);
        }
    }

    FILE *fp = fopen(filePath, "r");
    if (!fp) {
        fprintf(stderr, "Error opening '%s' : '%s'\n", filePath, strerror(errno));
        free(search);
        return 1;
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
    (void)assumeYes;

    return 0;
}
