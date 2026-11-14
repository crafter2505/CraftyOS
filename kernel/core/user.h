#ifndef USER_H
#define USER_H

#include "types.h"

typedef uint32_t uid_t;
typedef uint32_t gid_t;

#define USER_NAME_LEN   32
#define USER_HOME_LEN   64
#define USER_SHELL_LEN  64
#define USER_PASS_HASH  32

struct user {
    uid_t   uid;
    gid_t   gid;
    char name[USER_NAME_LEN];
    char home[USER_HOME_LEN];
    char shell[USER_SHELL_LEN];
    uint8_t pass_hash[USER_PASS_HASH];
    int is_admin;
    struct user *next;
};

void user_init_subsystem(void);

struct user *user_lookup(const char *name);

struct user *user_lookup_uid(uid_t uid);

int user_authenticate(const char *name, const char *password);

struct user *user_current(void);

void user_set_current(struct user *u);

void user_dump(void);

#endif