#include "path.h"

/* Compare the component [p, q) against a literal string. */
static int comp_eq(const char *p, const char *q, const char *lit) {
    int i = 0;
    while (p + i < q && lit[i] && p[i] == lit[i]) i++;
    return (p + i == q) && (lit[i] == 0);
}

/* Append a component [p, q) to buf at *len, followed by '/'. */
static int append_comp(char *buf, int *len, int cap,
                       const char *p, const char *q) {
    int n = (int)(q - p);
    if (*len + n + 2 >= cap) return -1;
    for (int i = 0; i < n; i++) buf[(*len)++] = p[i];
    buf[(*len)++] = '/';
    return 0;
}

/* Pop the last component from buf at *len. At root, stay at root. */
static void pop_comp(char *buf, int *len) {
    if (*len <= 1) {
        buf[0] = '/';
        *len = 1;
        return;
    }
    (*len)--;                                   /* step over trailing '/' */
    while (*len > 0 && buf[*len - 1] != '/') (*len)--;
    if (*len == 0) { buf[0] = '/'; *len = 1; }
}

int shell_resolve_path(const char *cwd, const char *target,
                       char *out, int out_size) {
    if (!target || !out || out_size < 2) return -1;

    char buf[512];
    int len = 0;

    /* Rule 1: first character decides the starting point.
     * Absolute path -> start at root, ignore cwd.
     * Relative path -> start at cwd, ensure trailing '/'. */
    if (target[0] == '/') {
        buf[len++] = '/';
        while (*target == '/') target++;
    } else {
        while (*cwd && len < (int)sizeof(buf) - 2) buf[len++] = *cwd++;
        if (len == 0) buf[len++] = '/';
        else if (buf[len - 1] != '/') buf[len++] = '/';
    }

    /* Rule 2 + 3: walk components left to right. */
    while (*target) {
        while (*target == '/') target++;
        if (!*target) break;

        const char *q = target;
        while (*q && *q != '/') q++;

        if (comp_eq(target, q, ".")) {
            /* skip */
        } else if (comp_eq(target, q, "..")) {
            pop_comp(buf, &len);
        } else {
            if (append_comp(buf, &len, (int)sizeof(buf), target, q) != 0)
                return -1;
        }
        target = q;
    }

    /* Rule 4: normalize. Strip trailing '/' unless root. */
    if (len > 1 && buf[len - 1] == '/') len--;
    buf[len] = 0;

    if (len + 1 > out_size) return -1;
    for (int i = 0; i <= len; i++) out[i] = buf[i];
    return 0;
}