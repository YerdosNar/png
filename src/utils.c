#include "../include/utils.h"

// CRC calculation for PNG
// copy pasted from:
// https://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html
static uint32_t crc_table[256];
static int crc_table_computed = 0;
static void make_crc_table(void) {
    uint32_t c;
    int n, k;

    for(n = 0; n < 256; n++) {
        c = (uint32_t) n;
        for(k = 0; k < 8; k++) {
            if(c & 1) {
                c = 0xEDB88320 ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}
static uint32_t update_crc(uint32_t crc, uint8_t *buf, int len) {
    uint32_t c = crc;
    int n;
    if(!crc_table_computed) {
        make_crc_table();
    }
    for(n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

uint32_t crc(uint8_t *buffer, int len) {
    return update_crc(0xFFffFFff, buffer, len) ^ 0xFFffFFff;
}
/** CRC FINISH */

uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width) {
    uint8_t **matrix = malloc(height * sizeof(uint8_t*)); // (return value) FLAG => FREE IT
    if(!matrix) {
        fprintf(stderr, "ERROR: Could not allocate memory for pixel matrix\n");
        exit(1);
    }

    for(uint32_t i = 0; i < height; i++) {
        matrix[i] = malloc(width * sizeof(uint8_t)); // FLAG => FREE IT
        if(!matrix[i]) {
            fprintf(stderr, "ERROR: Could not allocate memory for pixel row: %u\n", i);
            exit(1);
        }
    }

    return matrix;
}

void free_pixel_matrix(uint8_t **matrix, uint32_t height) {
    for(uint32_t i = 0; i < height; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

void reverse(void *buffer, size_t buf_size) {
    uint8_t *buff = buffer;
    for(uint32_t i = 0; i < buf_size/2; i++) {
        uint8_t t = buff[i];
        buff[i] = buff[buf_size - i - 1];
        buff[buf_size - i - 1] = t;
    }
}
