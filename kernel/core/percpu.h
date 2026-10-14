#ifndef PERCPU_H
#define PERCPU_H
#include "types.h"

struct percpu {
    uint64_t kernel_stack;
    uint64_t user_stack;
    uint64_t user_ss;
    uint64_t scratch;
};

struct percpu *percpu_get(void);
void percpu_init(void);

#endif