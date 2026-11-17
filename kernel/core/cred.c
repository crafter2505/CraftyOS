#include "cred.h"
#include "thread.h"
#include "vfs.h"

void cred_init_kernel(struct cred *c) {
    c->ruid = 0;
    c->euid = 0;
    c->suid = 0;
    c->rgid = 0;
    c->egid = 0;
    c->sgid = 0;
    c->umask = 0022;
    c->_pad = 0;
}

void cred_copy(struct cred *dst, const struct cred *src) {
    dst->ruid = src->ruid;
    dst->euid = src->euid;
    dst->suid = src->suid;
    dst->rgid = src->rgid;
    dst->egid = src->egid;
    dst->sgid = src->sgid;
    dst->umask = src->umask;
    dst->_pad = 0;
}

struct cred *current_cred(void) {
    struct thread *t = thread_current();
    if (t) return &t->cred;
    static struct cred kernel_cred;
    static int inited = 0;
    if (!inited) {
        cred_init_kernel(&kernel_cred);
        inited = 1;
    }
    return &kernel_cred;
}


int cred_check(const struct cred *c, struct inode *i, uint32_t want) {
    if (!c || !i) return -1;
    if (c->euid == 0) {
        if ((want & CRED_X) && (i->mode & 0111) == 0) return -1;
        return 0;
    }

    uint32_t perm;
    if (c->euid == i->uid)        perm = (i->mode >> 6) & 7;
    else if (c->egid == i->gid)   perm = (i->mode >> 3) & 7;
    else                          perm = i->mode & 7;

    if ((want & CRED_R) && !(perm & 4)) return -1;
    if ((want & CRED_W) && !(perm & 2)) return -1;
    if ((want & CRED_X) && !(perm & 1)) return -1;
    return 0;
}