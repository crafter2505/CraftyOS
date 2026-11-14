#include "user.h"
#include "sha256.h"
#include "kmalloc.h"
#include "kprintf.h"
#include "vfs.h"

static struct user *user_list = 0;
static struct user *current_user = 0;

static void copy_str(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static struct user *make_user(uid_t uid, gid_t gid,
                              const char *name, const char *pass,
                              const char *home, const char *shell,
                              int is_admin) {
    struct user *u = (struct user *)kmalloc(sizeof(struct user));
    if (!u) return 0;
    uint8_t *p = (uint8_t *)u;
    for (unsigned long i = 0; i < sizeof(struct user); i++) p[i] = 0;

    u->uid = uid;
    u->gid = gid;
    copy_str(u->name, name, USER_NAME_LEN);
    copy_str(u->home, home, USER_HOME_LEN);
    copy_str(u->shell, shell, USER_SHELL_LEN);

    int n = 0;
    while (pass[n]) n++;
    sha256(pass, (uint64_t)n, u->pass_hash);

    u->is_admin = is_admin;

    u->next = user_list;
    user_list = u;
    return u;
}

void user_init_subsystem(void) {

    make_user(0,    0,    "root", "crafty", "/root",     "/mnt/efi/boot/sh", 1);
    make_user(1000, 1000, "Iham", "Iham", "/home/Iham", "/mnt/efi/boot/sh", 0);

    current_user = 0;

    kprintf("  USER: initialized (%s)\n", user_list ? user_list->name : "none");
}

struct user *user_lookup(const char *name) {
    if (!name) return 0;
    for (struct user *u = user_list; u; u = u->next) {
        int i = 0;
        while (u->name[i] && name[i] && u->name[i] == name[i]) i++;
        if (u->name[i] == 0 && name[i] == 0) return u;
    }
    return 0;
}

struct user *user_lookup_uid(uid_t uid) {
    for (struct user *u = user_list; u; u = u->next) {
        if (u->uid == uid) return u;
    }
    return 0;
}

int user_authenticate(const char *name, const char *password) {
    struct user *u = user_lookup(name);
    if (!u) return -1;

    uint8_t hash[32];
    int plen = 0;
    while (password[plen]) plen++;
    sha256(password, (uint64_t)plen, hash);

    for (int i = 0; i < 32; i++) {
        if (hash[i] != u->pass_hash[i]) return -1;
    }
    return (int)u->uid;
}

struct user *user_current(void) {
    return current_user;
}

void user_set_current(struct user *u) {
    current_user = u;
}

void user_dump(void) {
    int count = 0;
    for (struct user *u = user_list; u; u = u->next) count++;
    kprintf("  USER: %d user(s)\n", (uint64_t)count);

    for (struct user *u = user_list; u; u = u->next) {
        kprintf("    uid=");
        kprintf("%d", (uint64_t)u->uid);
        int n = 0;
        uint32_t tmp = u->uid;
        do { n++; tmp /= 10; } while (tmp > 0);
        for (int i = n; i < 4; i++) kprintf(" ");

        kprintf(" gid=");
        kprintf("%d", (uint64_t)u->gid);
        n = 0;
        tmp = u->gid;
        do { n++; tmp /= 10; } while (tmp > 0);
        for (int i = n; i < 4; i++) kprintf(" ");

        kprintf(" name='%s' home='%s'", u->name, u->home);
        if (u->is_admin) kprintf(" [admin]");
        kprintf("\n");
    }
}