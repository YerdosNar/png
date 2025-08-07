#include "../include/utils.h"

//Source => https://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html

//Table of CRCs of all 8-bit message
uint32_t crc_table[256];

//Flag: has the table been computed? Initially false
int32_t crc_table_computed = 0;

//Make the table for a fast CRC.
void make_crc_table() {
    uint32_t c;
    int32_t n, k;
    for(n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for(k = 0; k < 8; k++) {
            if(c & 1) {
                c = 0xedb88320L ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        } 
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC (see
 * the crc()) routine below)). */ 
uint32_t update_crc(uint32_t crc, uint8_t *buf, int len) {
    uint32_t c = crc;
    int32_t n;

    if(!crc_table_computed) {
        make_crc_table();
    }
    for(n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

// Return the CRC of the bytes buf[0..len-1].
uint32_t crc(uint8_t *buf, int32_t len) {
    return update_crc(0xFFffFFffL, buf, len) ^ 0xFFffFFffL;
}

void reverse(void *buffer, size_t buffer_size) {
    uint8_t *temp = buffer;
    for(size_t i = 0; i < buffer_size/2; i++) {
        uint8_t t = temp[i];
        temp[i] = temp[buffer_size-i-1];
        temp[buffer_size-i-1] = t;
    }
}
