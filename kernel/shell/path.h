#ifndef PATH_H
#define PATH_H

/* Resolve `target` against `cwd`, producing a normalized absolute
 * path in `out`. Handles absolute paths (leading '/'), relative
 * paths, '.', '..', and collapse of '//'.
 *
 * Returns 0 on success, -1 if the result would exceed `out_size`.
 */
int shell_resolve_path(const char *cwd, const char *target,
                       char *out, int out_size);

#endif