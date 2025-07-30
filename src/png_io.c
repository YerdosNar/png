#include "utils.h"

void read_bytes(FILE *file, void *buffer, size_t buffer_size) {
    size_t n = fread(buffer, buffer_size, 1, file);
    if(n != 1) {
        if(ferror(file)) {
            fprintf(stderr, "ERROR: Could not read %zu bytes form file: %s\n", buffer_size, strerror(errno));
            exit(1);
        } else if(feof(file)) {
            fprintf(stderr, "ERROR: Could not read %zu bytes from file: Reached EOF %s\n", buffer_size, strerror(errno));
            exit(1);
        } else {
            assert(0 && "unreachable");
        }
    }
}

void write_bytes(FILE *file, const void *buffer, size_t buffer_size) {
    size_t n = fwrite(buffer, buffer_size, 1, file);
    if(1 != n) {
        fprintf(stderr, "ERROR: Could not write %zu bytes to file: %s\n", buffer_size, strerror(errno));
        exit(1);
    }
}

uint32_t read_chunk_size(FILE *file) {
    uint32_t size;
    read_bytes(file, &size, sizeof(size));
    return ntohl(size);
}

void read_chunk_type(FILE *file, uint8_t chunk_type[]) {
    size_t chunk_type_size = 4;
    read_bytes(file, chunk_type, chunk_type_size);
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    size_t crc_size = 4;
    read_bytes(file, &crc, crc_size);
    return ntohl(crc);
}

void write_chunk(FILE *file, const int8_t *type, uint8_t *data, uint32_t length) {
    uint32_t reverse_length = htonl(length);
    write_bytes(file, &reverse_length, sizeof(reverse_length));
    uint32_t type_size = 4;
    write_bytes(file, type, sizeof(type_size));
    if(length > 0) {
        write_bytes(file, data, length);
    }

    uint8_t *crc_buffer = malloc(length + 4);
    memcpy(crc_buffer, type, 4);
    if(length > 0) {
        memcpy(crc_buffer + 4, data, length);
    }
    uint32_t crc_value = htonl(crc(crc_buffer, length + 4));
    write_bytes(file, &crc_value, sizeof(crc_value));
    free(crc_buffer);
}
