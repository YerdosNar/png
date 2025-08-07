#include "../include/png_io.h"

void read_bytes(FILE *file, void *buffer, size_t buffer_size) {
    size_t n = fread(buffer, buffer_size, 1, file);
    if(1 != n) {
        fprintf(stderr, "ERROR: Could not read bytes: %zu\n", buffer_size);
        exit(1);
    }
}

void read_ihdr(FILE *file, ihdr_t *ihdr) {
    read_bytes(file, &ihdr->width, sizeof(ihdr->width));
    read_bytes(file, &ihdr->height, sizeof(ihdr->height));
    read_bytes(file, &ihdr->bit_depth, sizeof(ihdr->bit_depth));
    read_bytes(file, &ihdr->color_type, sizeof(ihdr->color_type));
    read_bytes(file, &ihdr->compression, sizeof(ihdr->compression));
    read_bytes(file, &ihdr->filter, sizeof(ihdr->filter));
    read_bytes(file, &ihdr->interlace, sizeof(ihdr->interlace));
    read_bytes(file, &ihdr->crc, sizeof(ihdr->crc));
}

void write_bytes(FILE *file, void *buffer, size_t buffer_size) {
    size_t n = fwrite(buffer, buffer_size, 1, file);
    if(1 != n) {
        fprintf(stderr, "ERROR: Could not write bytes: %zu\n", buffer_size);
        exit(1);
    }
}

void write_ihdr(FILE *file, ihdr_t ihdr) {
    write_bytes(file, &ihdr.size, sizeof(ihdr.size));
    write_bytes(file, &ihdr.type, sizeof(ihdr.type));
    write_bytes(file, &ihdr.width, sizeof(ihdr.width));
    write_bytes(file, &ihdr.height, sizeof(ihdr.height));
    write_bytes(file, &ihdr.bit_depth, sizeof(ihdr.bit_depth));
    write_bytes(file, &ihdr.color_type, sizeof(ihdr.color_type));
    write_bytes(file, &ihdr.compression, sizeof(ihdr.compression));
    write_bytes(file, &ihdr.filter, sizeof(ihdr.filter));
    write_bytes(file, &ihdr.interlace, sizeof(ihdr.interlace));
    write_bytes(file, &ihdr.crc, sizeof(ihdr.crc));
}

void write_chunk(FILE *file, chunk_t ch, uint32_t size) {
    write_bytes(file, &ch.size, sizeof(ch.size));
    write_bytes(file, &ch.type, sizeof(ch.type));
    if(size > 0 && ch.data != NULL) {
        write_bytes(file, ch.data, size);
    }
    uint8_t *crc_data = malloc(4+size);
    memcpy(crc_data, ch.type, 4);
    if(size > 0 && ch.data != NULL) {
        memcpy(crc_data+4, ch.data, size);
    }
    uint32_t crc_val = crc(crc_data, 4+size);
    reverse(&crc_val, sizeof(crc_val));
    write_bytes(file, &crc_val, sizeof(crc_val));
    free(crc_data);
}
