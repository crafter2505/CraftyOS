#ifndef CRED_H
#define CRED_H

#include "types.h"

typedef uint32_t uid_t;
typedef uint32_t gid_t;

#define CRED_R 4
#define CRED_W 2
#define CRED_X 1

struct cred {
    uid_t ruid;
    uid_t euid;
    uid_t suid;
    gid_t rgid;
    gid_t egid;
    gid_t sgid;
    uint32_t umask;
    uint32_t _pad;
};

struct inode;

void cred_init_kernel(struct cred *c);
void cred_copy(struct cred *dst, const struct cred *src);
struct cred *current_cred(void);
int cred_check(const struct cred *c, struct inode *ino, uint32_t want);

#endif