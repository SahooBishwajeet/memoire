#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char *key;
    char *value;
} Entry;

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
            "  %s [options] set <key> <value>  # set (create or overwrite)\n"
            "Options:\n"
            "  -f, --file <path>       Use data file (default ./data.txt)\n"
            "  -y, --yes               Assume yes for confirmations\n"
            "  -h, --help              Show this help\n",
            prog, prog, prog);
}

static int confirmPrompt(const char *prompt) {
    char buf[32];
    fprintf(stderr, "%s [y/N]: ", prompt);
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    return (buf[0] == 'y' || buf[0] == 'Y');
}

static void freeEntries(Entry *arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; ++i) {
        free(arr[i].key);
        free(arr[i].value);
    }
    free(arr);
}

static int loadEntries(const char *path, Entry **out, size_t *numReads, int verbose) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (errno == ENOENT) {
            *out = NULL;
            *numReads = 0;
            return 0;
        }
        fprintf(stderr, "Error opening '%s' : '%s'\n", path, strerror(errno));
        return -1;
    }

    Entry *arr = NULL;
    size_t arrCap = 0, entriesLoaded = 0;

    char *line = NULL;
    size_t capacity = 0;
    ssize_t nread;

    while ((nread = getline(&line, &capacity, fp)) != -1) {
        if (nread <= 0) continue;

        // remove trailing \n, \r & windows-style \r\n
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[--nread] = '\0';
        }

        // get delimiter ":"
        char *delim = strchr(line, ':');
        if (!delim) {
            if (verbose) {
                fprintf(stderr, "Skipping line : %s [No Separator ':']\n", line);
            }
            continue;
        }
        *delim = '\0';

        // now line contains key; delim + 1 contains value
        char *key = trim(line);
        char *value = trim(delim + 1);

        if (key == NULL || key[0] == '\0') {
            if (verbose) {
                fprintf(stderr, "Skipping line : %s [Empty key]\n", line);
            }
            continue;
        }

        if (entriesLoaded == arrCap) {
            size_t nc = arrCap ? arrCap * 2 : 16;
            Entry *tmp = realloc(arr, nc * sizeof(Entry));
            if (!tmp) {
                fprintf(stderr, "Memory allocation failed\n");
                free(line);
                freeEntries(arr, entriesLoaded);
                fclose(fp);
                return -1;
            }
            arr = tmp;
            arrCap = nc;
        }

        arr[entriesLoaded].key = strdup(key);
        arr[entriesLoaded].value = value ? strdup(value) : strdup("");
        if (!arr[entriesLoaded].key || !arr[entriesLoaded].value) {
            fprintf(stderr, "Memory allocation failed\n");
            free(line);
            freeEntries(arr, entriesLoaded + 1);
            fclose(fp);
            return -1;
        }
        entriesLoaded++;
    }

    free(line);
    fclose(fp);

    *out = arr;
    *numReads = entriesLoaded;
    return 0;
}

static int saveEntriesAtomic(const char *path, Entry *arr, size_t n) {
    size_t tlen = strlen(path) + 12;
    char *tmp = malloc(tlen + 1);
    if (!tmp) {
        fprintf(stderr, "Memory error\n");
        return -1;
    }
    snprintf(tmp, tlen + 1, "%s.tmpXXXXXX", path);

    int fd = mkstemp(tmp);
    if (fd == -1) {
        fprintf(stderr, "mkstemp failed for '%s': %s\n", tmp, strerror(errno));
        free(tmp);
        return -1;
    }

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        fprintf(stderr, "fdopen failed: %s\n", strerror(errno));
        close(fd);
        unlink(tmp);
        free(tmp);
        return -1;
    }

    for (size_t i = 0; i < n; ++i) {
        if (fprintf(fp, "%s:%s\n", arr[i].key ? arr[i].key : "", arr[i].value ? arr[i].value : "") <
            0) {
            fprintf(stderr, "Write error: %s\n", strerror(errno));
            fclose(fp);
            unlink(tmp);
            free(tmp);
            return -1;
        }
    }

    if (fflush(fp) != 0) {
        fprintf(stderr, "fflush failed: %s\n", strerror(errno));
        fclose(fp);
        unlink(tmp);
        free(tmp);
        return -1;
    }
    int outfd = fileno(fp);
    if (outfd >= 0) {
        if (fsync(outfd) != 0) {
            fprintf(stderr, "Warning: fsync failed: %s\n", strerror(errno));
        }
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "fclose failed: %s\n", strerror(errno));
        unlink(tmp);
        free(tmp);
        return -1;
    }

    if (rename(tmp, path) != 0) {
        fprintf(stderr, "rename %s -> %s failed: %s\n", tmp, path, strerror(errno));
        unlink(tmp);
        free(tmp);
        return -1;
    }

    free(tmp);
    return 0;
}

static ssize_t findEntry(Entry *arr, size_t n, const char *key) {
    if (!key) return -1;
    for (size_t i = 0; i < n; ++i) {
        if (arr[i].key && strcmp(arr[i].key, key) == 0) return (ssize_t)i;
    }
    return -1;
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

    // decide whether this is list/get or subcommand
    if (idx >= argc) {
        // list mode: use loadEntries()
        Entry *arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            freeEntries(arr, n);
            return 1;
        }
        for (size_t i = 0; i < n; ++i) {
            printf("%s: %s\n", arr[i].key ? arr[i].key : "", arr[i].value ? arr[i].value : "");
        }
        freeEntries(arr, n);
        return 0;
    }

    // If first positional arg is "set", handle set <key> <value>
    if (strcmp(argv[idx], "set") == 0) {
        if (idx + 2 >= argc) {
            usage(prog);
            return 2;
        }  // need two args
        const char *key = argv[idx + 1];

        size_t valLen = 1;  // for '\0'
        for (int i = idx + 2; i < argc; ++i) valLen += strlen(argv[i]) + 1;
        char *value = malloc(valLen);
        if (!value) {
            fprintf(stderr, "Memory error\n");
            return 1;
        }
        value[0] = '\0';
        for (int i = idx + 2; i < argc; ++i) {
            strcat(value, argv[i]);
            if (i + 1 < argc) strcat(value, " ");
        }

        Entry *arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            freeEntries(arr, n);
            return 1;
        }

        ssize_t pos = findEntry(arr, n, key);
        if (pos >= 0) {
            // exists: confirm overwrite
            if (!assumeYes) {
                char prompt[1024];
                snprintf(prompt, sizeof(prompt),
                         "Key '%s' exists.\nOld value: %s\nNew value: %s\nConfirm overwrite?", key,
                         arr[pos].value ? arr[pos].value : "", value ? value : "");
                if (!confirmPrompt(prompt)) {
                    fprintf(stderr, "Aborted.\n");
                    freeEntries(arr, n);
                    return 1;
                }
            }
            free(arr[pos].value);
            arr[pos].value = strdup(value ? value : "");
            if (!arr[pos].value) {
                fprintf(stderr, "Memory error\n");
                freeEntries(arr, n);
                return 1;
            }
        } else {
            // create new
            Entry *tmp = realloc(arr, (n + 1) * sizeof(Entry));
            if (!tmp) {
                fprintf(stderr, "Memory error\n");
                freeEntries(arr, n);
                return 1;
            }
            arr = tmp;
            arr[n].key = strdup(key);
            arr[n].value = strdup(value ? value : "");
            if (!arr[n].key || !arr[n].value) {
                fprintf(stderr, "Memory error\n");
                freeEntries(arr, n + 1);
                return 1;
            }
            n++;
        }

        if (saveEntriesAtomic(filePath, arr, n) != 0) {
            fprintf(stderr, "Failed to save changes\n");
            freeEntries(arr, n);
            return 1;
        }

        freeEntries(arr, n);
        free(value);
        printf("OK\n");
        return 0;
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

    if (listMode) {
        Entry *arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            freeEntries(arr, n);
            free(search);
            return 1;
        }
        for (size_t i = 0; i < n; ++i) {
            printf("%s: %s\n", arr[i].key ? arr[i].key : "", arr[i].value ? arr[i].value : "");
        }
        freeEntries(arr, n);
    } else {
        Entry *arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 1) != 0) {
            freeEntries(arr, n);
            free(search);
            return 1;
        }
        int found = 0;
        for (size_t i = 0; i < n; ++i) {
            if (arr[i].key && strcmp(arr[i].key, search) == 0) {
                printf("%s: %s\n", arr[i].key, arr[i].value ? arr[i].value : "");
                found = 1;
                break;
            }
        }
        freeEntries(arr, n);
        (void)found;
    }

    free(search);
    (void)assumeYes;

    return 0;
}
