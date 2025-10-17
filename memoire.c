#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char* key;
    char* value;
} Entry;

static char* getDataPath(void) {
    const char* xdgConfig = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");

    if (!xdgConfig && !home) {
        return strdup("./data.txt");
    }

    char* configDir;
    if (xdgConfig) {
        size_t len = strlen(xdgConfig) + strlen("/memoire") + 1;
        configDir = malloc(len);
        if (!configDir) return NULL;
        snprintf(configDir, len, "%s/memoire", xdgConfig);
    } else {
        size_t len = strlen(home) + strlen("/.config/memoire") + 1;
        configDir = malloc(len);
        if (!configDir) return NULL;
        snprintf(configDir, len, "%s/.config/memoire", home);
    }

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(configDir, &st) == -1) {
        if (mkdir(configDir, 0755) == -1) {
            fprintf(stderr, "Warning: Could not create config directory: %s\n", strerror(errno));
        }
    }

    size_t pathLen = strlen(configDir) + strlen("/data.txt") + 1;
    char* dataPath = malloc(pathLen);
    if (!dataPath) {
        free(configDir);
        return NULL;
    }
    snprintf(dataPath, pathLen, "%s/data.txt", configDir);

    free(configDir);
    return dataPath;
}

static char* trim(char* s) {
    if (!s) return NULL;

    // Skip leading whitespace
    while (*s && isspace((unsigned char)*s)) s++;

    if (*s == '\0') return s;  // string was all whitespace

    // Move backward over trailing whitespace
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';
    return s;
}

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [options]            # list all entries\n"
            "  %s [options] get <key>  # get a key (fuzzy search allowed)\n"
            "  %s [options] set <key> <value>  # set (create or overwrite)\n"
            "  %s [options] update <key> <value>  # update existing key only\n"
            "  %s [options] delete <key>  # delete a key\n"
            "Options:\n"
            "  -f, --file <path>       Use data file (default ./data.txt)\n"
            "  -y, --yes               Assume yes for confirmations\n"
            "  -h, --help              Show this help\n",
            prog, prog, prog, prog, prog);
}

static int confirmPrompt(const char* prompt) {
    char buf[32];
    fprintf(stderr, "%s [y/N]: ", prompt);
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    return (buf[0] == 'y' || buf[0] == 'Y');
}

static void freeEntries(Entry* arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; ++i) {
        free(arr[i].key);
        free(arr[i].value);
    }
    free(arr);
}

static int loadEntries(const char* path, Entry** out, size_t* numReads, int verbose) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        if (errno == ENOENT) {
            *out = NULL;
            *numReads = 0;
            return 0;
        }
        fprintf(stderr, "Error opening '%s' : '%s'\n", path, strerror(errno));
        return -1;
    }

    Entry* arr = NULL;
    size_t arrCap = 0, entriesLoaded = 0;

    char* line = NULL;
    size_t capacity = 0;
    ssize_t nread;

    while ((nread = getline(&line, &capacity, fp)) != -1) {
        if (nread <= 0) continue;

        // remove trailing \n, \r & windows-style \r\n
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[--nread] = '\0';
        }

        // get delimiter ":"
        char* delim = strchr(line, ':');
        if (!delim) {
            if (verbose) {
                fprintf(stderr, "Skipping line : %s [No Separator ':']\n", line);
            }
            continue;
        }
        *delim = '\0';

        // now line contains key; delim + 1 contains value
        char* key = trim(line);
        char* value = trim(delim + 1);

        if (key == NULL || key[0] == '\0') {
            if (verbose) {
                fprintf(stderr, "Skipping line : %s [Empty key]\n", line);
            }
            continue;
        }

        if (entriesLoaded == arrCap) {
            size_t nc = arrCap ? arrCap * 2 : 16;
            Entry* tmp = realloc(arr, nc * sizeof(Entry));
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

static int saveEntriesAtomic(const char* path, Entry* arr, size_t n) {
    size_t tlen = strlen(path) + 12;
    char* tmp = malloc(tlen + 1);
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

    FILE* fp = fdopen(fd, "w");
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

static int fuzzyMatch(const char* key, const char* pattern) {
    if (!key || !pattern) return 0;

    const char* k = key;
    const char* p = pattern;

    // Convert to lowercase for case-insensitive matching
    while (*p) {
        char pc = tolower(*p);
        int found = 0;

        // Look for this pattern character in the remaining key
        while (*k) {
            if (tolower(*k) == pc) {
                found = 1;
                k++;
                break;
            }
            k++;
        }

        if (!found) return 0;  // Pattern character not found
        p++;
    }

    return 1;  // All pattern characters found in order
}

static ssize_t findEntry(Entry* arr, size_t n, const char* key) {
    if (!key) return -1;
    for (size_t i = 0; i < n; ++i) {
        if (arr[i].key && strcmp(arr[i].key, key) == 0) return (ssize_t)i;
    }
    return -1;
}

static ssize_t findEntryFuzzy(Entry* arr, size_t n, const char* pattern) {
    if (!pattern) return -1;

    // First try exact match
    ssize_t exact = findEntry(arr, n, pattern);
    if (exact >= 0) return exact;

    // Then try fuzzy match
    for (size_t i = 0; i < n; ++i) {
        if (arr[i].key && fuzzyMatch(arr[i].key, pattern)) {
            return (ssize_t)i;
        }
    }
    return -1;
}

static Entry* removeEntry(Entry* arr, size_t* n, size_t index) {
    if (!arr || index >= *n) return arr;

    free(arr[index].key);
    free(arr[index].value);

    // Shift remaining entries
    for (size_t i = index; i < *n - 1; ++i) {
        arr[i] = arr[i + 1];
    }

    (*n)--;

    if (*n == 0) {
        free(arr);
        return NULL;
    }

    Entry* tmp = realloc(arr, (*n) * sizeof(Entry));
    return tmp ? tmp : arr;  // Return original if realloc fails but keep smaller size
}

int main(int argc, char const* argv[]) {
    const char* prog = argc > 0 ? argv[0] : "memoire";

    char* defaultPath = getDataPath();
    const char* filePath = defaultPath ? defaultPath : "./data.txt";

    int assumeYes = 0;

    int idx = 1;
    while (idx < argc && argv[idx] && argv[idx][0] == '-') {
        if (strcmp(argv[idx], "-f") == 0 || strcmp(argv[idx], "--file") == 0) {
            if (idx + 1 >= argc) {
                fprintf(stderr, "Missing argument (filename) for %s\n", argv[idx]);
                usage(prog);
                free(defaultPath);
                return 2;
            }

            filePath = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-y") == 0 || strcmp(argv[idx], "--yes") == 0) {
            assumeYes = 1;
            idx++;
        } else if (strcmp(argv[idx], "-h") == 0 || strcmp(argv[idx], "--help") == 0) {
            usage(prog);
            free(defaultPath);
            return 0;
        } else {
            fprintf(stderr, "Unknown options: %s\n", argv[idx]);
            usage(prog);
            free(defaultPath);
            return 2;
        }
    }

    // decide whether this is list mode or a subcommand
    if (idx >= argc) {
        // list mode: use loadEntries()
        Entry* arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            freeEntries(arr, n);
            return 1;
        }
        for (size_t i = 0; i < n; ++i) {
            printf("%s:%s\n", arr[i].key ? arr[i].key : "", arr[i].value ? arr[i].value : "");
        }
        freeEntries(arr, n);
        free(defaultPath);
        return 0;
    }

    const char* command = argv[idx];

    // Handle get command
    if (strcmp(command, "get") == 0) {
        if (idx + 1 >= argc) {
            fprintf(stderr, "Missing key for get command\n");
            usage(prog);
            return 2;
        }

        const char* key = argv[idx + 1];
        Entry* arr = NULL;
        size_t n = 0;

        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            freeEntries(arr, n);
            return 1;
        }

        ssize_t pos = findEntryFuzzy(arr, n, key);
        if (pos >= 0) {
            printf("%s:%s\n", arr[pos].key, arr[pos].value ? arr[pos].value : "");
            freeEntries(arr, n);
            return 0;
        } else {
            fprintf(stderr, "Key '%s' not found\n", key);
            freeEntries(arr, n);
            return 1;
        }
    }

    // Handle set command
    if (strcmp(command, "set") == 0) {
        if (idx + 2 >= argc) {
            fprintf(stderr, "Missing key and/or value for set command\n");
            usage(prog);
            return 2;
        }

        const char* key = argv[idx + 1];

        size_t valLen = 1;  // for '\0'
        for (int i = idx + 2; i < argc; ++i) valLen += strlen(argv[i]) + 1;
        char* value = malloc(valLen);
        if (!value) {
            fprintf(stderr, "Memory error\n");
            return 1;
        }
        value[0] = '\0';
        for (int i = idx + 2; i < argc; ++i) {
            strcat(value, argv[i]);
            if (i + 1 < argc) strcat(value, " ");
        }

        Entry* arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            free(value);
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
                    free(value);
                    freeEntries(arr, n);
                    return 1;
                }
            }
            free(arr[pos].value);
            arr[pos].value = strdup(value ? value : "");
            if (!arr[pos].value) {
                fprintf(stderr, "Memory error\n");
                free(value);
                freeEntries(arr, n);
                return 1;
            }
        } else {
            // create new
            Entry* tmp = realloc(arr, (n + 1) * sizeof(Entry));
            if (!tmp) {
                fprintf(stderr, "Memory error\n");
                free(value);
                freeEntries(arr, n);
                return 1;
            }
            arr = tmp;
            arr[n].key = strdup(key);
            arr[n].value = strdup(value ? value : "");
            if (!arr[n].key || !arr[n].value) {
                fprintf(stderr, "Memory error\n");
                free(value);
                freeEntries(arr, n + 1);
                return 1;
            }
            n++;
        }

        if (saveEntriesAtomic(filePath, arr, n) != 0) {
            fprintf(stderr, "Failed to save changes\n");
            free(value);
            freeEntries(arr, n);
            return 1;
        }

        free(value);
        freeEntries(arr, n);
        printf("OK\n");
        return 0;
    }

    // Handle update command
    if (strcmp(command, "update") == 0) {
        if (idx + 2 >= argc) {
            fprintf(stderr, "Missing key and/or value for update command\n");
            usage(prog);
            return 2;
        }

        const char* key = argv[idx + 1];

        size_t valLen = 1;  // for '\0'
        for (int i = idx + 2; i < argc; ++i) valLen += strlen(argv[i]) + 1;
        char* value = malloc(valLen);
        if (!value) {
            fprintf(stderr, "Memory error\n");
            return 1;
        }
        value[0] = '\0';
        for (int i = idx + 2; i < argc; ++i) {
            strcat(value, argv[i]);
            if (i + 1 < argc) strcat(value, " ");
        }

        Entry* arr = NULL;
        size_t n = 0;
        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            free(value);
            freeEntries(arr, n);
            return 1;
        }

        ssize_t pos = findEntry(arr, n, key);
        if (pos >= 0) {
            // exists: confirm update
            if (!assumeYes) {
                char prompt[1024];
                snprintf(prompt, sizeof(prompt),
                         "Update key '%s'?\nOld value: %s\nNew value: %s\nConfirm update?", key,
                         arr[pos].value ? arr[pos].value : "", value ? value : "");
                if (!confirmPrompt(prompt)) {
                    fprintf(stderr, "Aborted.\n");
                    free(value);
                    freeEntries(arr, n);
                    return 1;
                }
            }
            free(arr[pos].value);
            arr[pos].value = strdup(value ? value : "");
            if (!arr[pos].value) {
                fprintf(stderr, "Memory error\n");
                free(value);
                freeEntries(arr, n);
                return 1;
            }

            if (saveEntriesAtomic(filePath, arr, n) != 0) {
                fprintf(stderr, "Failed to save changes\n");
                free(value);
                freeEntries(arr, n);
                return 1;
            }

            free(value);
            freeEntries(arr, n);
            printf("OK\n");
            return 0;
        } else {
            fprintf(stderr, "Key '%s' not found. Use 'set' to create new entries.\n", key);
            free(value);
            freeEntries(arr, n);
            return 1;
        }
    }

    // Handle delete command
    if (strcmp(command, "delete") == 0) {
        if (idx + 1 >= argc) {
            fprintf(stderr, "Missing key for delete command\n");
            usage(prog);
            return 2;
        }

        const char* key = argv[idx + 1];
        Entry* arr = NULL;
        size_t n = 0;

        if (loadEntries(filePath, &arr, &n, 0) != 0) {
            freeEntries(arr, n);
            return 1;
        }

        ssize_t pos = findEntry(arr, n, key);
        if (pos >= 0) {
            if (!assumeYes) {
                char prompt[1024];
                snprintf(prompt, sizeof(prompt), "Delete key '%s'?\nValue: %s\nConfirm delete?",
                         key, arr[pos].value ? arr[pos].value : "");
                if (!confirmPrompt(prompt)) {
                    fprintf(stderr, "Aborted.\n");
                    freeEntries(arr, n);
                    return 1;
                }
            }

            arr = removeEntry(arr, &n, (size_t)pos);

            if (saveEntriesAtomic(filePath, arr, n) != 0) {
                fprintf(stderr, "Failed to save changes\n");
                freeEntries(arr, n);
                return 1;
            }

            freeEntries(arr, n);
            printf("OK\n");
            return 0;
        } else {
            fprintf(stderr, "Key '%s' not found\n", key);
            freeEntries(arr, n);
            return 1;
        }
    }

    // Unknown command
    fprintf(stderr, "Unknown command: %s\n", command);
    usage(prog);
    return 2;
}
