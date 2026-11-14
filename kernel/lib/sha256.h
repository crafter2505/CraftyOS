#ifndef SHA256_H
#define SHA256_H

#include "types.h"

void sha256(const void *data, uint64_t len, uint8_t out[32]);

#endif