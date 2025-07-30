#include "utils.h"

static uint64_t crc_table[256];
static int crc_table_computed = 0;

static void make_crc_table(void) {
    uint64_t c;
    int n, k;

    for(n = 0; n < 256; n++) {
        c = (uint64_t) n;
        for(k = 0; k < 8; k++) {
            if(c & 1) {
                c = 0xEDB88320L ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

static uint64_t update_crc(uint64_t crc, uint8_t *buf, int len) {
    uint64_t c = crc;
    int n;
    if(!crc_table_computed) {
        make_crc_table();
    }
    for(n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

uint64_t crc(uint8_t *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}
