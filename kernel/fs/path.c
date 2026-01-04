/**
 * =============================================================================
 * Chanux OS - Path Utilities
 * =============================================================================
 * Path parsing and manipulation functions.
 * =============================================================================
 */

#include "fs/vfs.h"
#include "string.h"

/**
 * Check if a path is absolute (starts with /).
 */
int path_is_absolute(const char* path) {
    return path && path[0] == '/';
}

/**
 * Get the basename (filename) from a path.
 *
 * Returns pointer to the filename within the path string.
 */
const char* path_basename(const char* path) {
    if (!path || !*path) {
        return ".";
    }

    /* Find last slash */
    const char* last_slash = NULL;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    if (!last_slash) {
        return path;  /* No slash, entire path is filename */
    }

    if (last_slash[1] == '\0') {
        /* Path ends with slash - find previous component */
        const char* end = last_slash;
        while (end > path && *end == '/') {
            end--;
        }
        if (end == path && *end == '/') {
            return "/";  /* Root directory */
        }
        const char* start = end;
        while (start > path && *(start - 1) != '/') {
            start--;
        }
        return start;
    }

    return last_slash + 1;
}

/**
 * Get the directory name from a path.
 *
 * Writes the directory portion to buf.
 * Returns 0 on success, -1 on error.
 */
int path_dirname(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) {
        return -1;
    }

    if (!*path) {
        if (size < 2) return -1;
        buf[0] = '.';
        buf[1] = '\0';
        return 0;
    }

    /* Find last slash */
    const char* last_slash = NULL;
    for (const char* p = path; *p; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    if (!last_slash) {
        /* No slash - directory is current directory */
        if (size < 2) return -1;
        buf[0] = '.';
        buf[1] = '\0';
        return 0;
    }

    if (last_slash == path) {
        /* Only leading slash - directory is root */
        if (size < 2) return -1;
        buf[0] = '/';
        buf[1] = '\0';
        return 0;
    }

    /* Copy path up to (but not including) last slash */
    size_t len = last_slash - path;
    if (len >= size) {
        return -1;  /* Buffer too small */
    }

    memcpy(buf, path, len);
    buf[len] = '\0';

    return 0;
}

/**
 * Normalize a path.
 *
 * - Resolves . and ..
 * - Removes duplicate slashes
 * - Makes relative paths absolute using cwd
 *
 * Returns 0 on success, -1 on error.
 */
int path_normalize(const char* path, const char* cwd, char* buf, size_t size) {
    if (!path || !buf || size == 0) {
        return -1;
    }

    /* Build working path */
    char work[VFS_MAX_PATH];
    size_t work_len = 0;

    /* Start with absolute path or cwd */
    if (path[0] == '/') {
        work[0] = '/';
        work_len = 1;
    } else {
        /* Use cwd as base */
        if (!cwd || cwd[0] != '/') {
            work[0] = '/';
            work_len = 1;
        } else {
            size_t cwd_len = strlen(cwd);
            if (cwd_len >= VFS_MAX_PATH) {
                return -1;
            }
            memcpy(work, cwd, cwd_len);
            work_len = cwd_len;
            /* Ensure trailing slash */
            if (work_len > 0 && work[work_len - 1] != '/') {
                if (work_len >= VFS_MAX_PATH - 1) {
                    return -1;
                }
                work[work_len++] = '/';
            }
        }
    }

    /* Process path components */
    const char* p = path;
    if (*p == '/') p++;  /* Skip leading slash */

    while (*p) {
        /* Skip multiple slashes */
        while (*p == '/') p++;
        if (!*p) break;

        /* Find end of component */
        const char* end = p;
        while (*end && *end != '/') end++;

        size_t comp_len = end - p;

        if (comp_len == 1 && p[0] == '.') {
            /* Current directory - skip */
        } else if (comp_len == 2 && p[0] == '.' && p[1] == '.') {
            /* Parent directory - go up */
            if (work_len > 1) {
                /* Remove trailing slash */
                if (work[work_len - 1] == '/') {
                    work_len--;
                }
                /* Remove last component */
                while (work_len > 1 && work[work_len - 1] != '/') {
                    work_len--;
                }
            }
        } else {
            /* Regular component - append */
            if (work_len > 0 && work[work_len - 1] != '/') {
                if (work_len >= VFS_MAX_PATH - 1) {
                    return -1;
                }
                work[work_len++] = '/';
            }
            if (work_len + comp_len >= VFS_MAX_PATH) {
                return -1;
            }
            memcpy(work + work_len, p, comp_len);
            work_len += comp_len;
        }

        p = end;
    }

    /* Ensure at least root */
    if (work_len == 0) {
        work[0] = '/';
        work_len = 1;
    }

    /* Remove trailing slash (except for root) */
    while (work_len > 1 && work[work_len - 1] == '/') {
        work_len--;
    }

    /* Copy to output buffer */
    if (work_len >= size) {
        return -1;
    }
    memcpy(buf, work, work_len);
    buf[work_len] = '\0';

    return 0;
}

/**
 * Join a directory and name into a path.
 *
 * Returns 0 on success, -1 on error.
 */
int path_join(const char* dir, const char* name, char* buf, size_t size) {
    if (!dir || !name || !buf || size == 0) {
        return -1;
    }

    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);

    /* Check if we need a separator */
    bool need_sep = (dir_len > 0 && dir[dir_len - 1] != '/' && name[0] != '/');

    size_t total_len = dir_len + (need_sep ? 1 : 0) + name_len;
    if (total_len >= size) {
        return -1;  /* Buffer too small */
    }

    memcpy(buf, dir, dir_len);
    if (need_sep) {
        buf[dir_len] = '/';
        memcpy(buf + dir_len + 1, name, name_len);
    } else {
        memcpy(buf + dir_len, name, name_len);
    }
    buf[total_len] = '\0';

    return 0;
}
