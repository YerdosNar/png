#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PNG_SIG_SIZE 8

const uint8_t png_sig[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

typedef struct {
    uint32_t width, 
             height;
    uint8_t bit_depth, 
            color_type, 
            compression,
            filter,
            interlace;
} ihdr_t;

typedef struct {
    uint8_t **pixels; // 2d matrix for output
    uint32_t width,
             height,
             channels; //1 - grayscale, 3 - RGB, 4 - RGBA
} image_t;

void read_bytes(FILE *file, void *buffer, size_t buffer_size) {
    size_t n = fread(buffer, buffer_size, 1, file);
    if(1 != n) {
        if(ferror(file)) {
            fprintf(stderr, "ERROR: could not read %zu bytes from file: %s\n", buffer_size, strerror(errno));
            exit(1);
        } else if(feof(file)) {
            fprintf(stderr, "ERROR: could not read %zu bytes from file: reached EOF %s\n", buffer_size, strerror(errno));
            exit(1);
        } else {
            assert(0 && "unreachable");
        }
    }
}

void write_bytes(FILE *file, const void *buffer, size_t buffer_size) {
    size_t n = fwrite(buffer, buffer_size, 1, file);
    if(1 != n) {
        fprintf(stderr, "ERROR: could not write %zu bytes to file: %s\n", buffer_size, strerror(errno));
        exit(1);
    }
}

uint32_t read_chunk_size(FILE *file) {
    uint32_t size;
    read_bytes(file, &size, 4); // because chunk size 4 bytes;
    return ntohl(size);
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    read_bytes(file, &crc, 4);
    return ntohl(crc);
}

void print_bytes(uint8_t *buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size; i++) {
        printf("%u ", buffer[i]);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    char *exec_file = argv[0];
    if(2 != argc) {
        fprintf(stderr, "USAGE: %s <input_file.png>\n", exec_file);
        exit(1);
    }

    char *input_file_name = argv[1];
    FILE *input_file = fopen(input_file_name, "rb");
    if(!input_file) {
        fprintf(stderr, "Could not read the file %s\n", input_file_name);
        exit(1);
    }

    //check if it is a PNG file
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_file, signature, PNG_SIG_SIZE);
    printf("Signature: ");
    print_bytes(signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", input_file_name);
        exit(1);
    } else {
        printf("SUCCESS: %s is a PNG file\n", input_file_name);
    }

    bool quit = false;
    while(!quit) {

        //print size
        uint32_t chunk_size;
        read_bytes(input_file, &chunk_size, sizeof(chunk_size));
        chunk_size = ntohl(chunk_size);
        printf("CHUNK SIZE: %u\n", chunk_size);

        //print type 
        uint8_t chunk_type[4];
        read_bytes(input_file, &chunk_type, sizeof(chunk_type));
        printf("CHUNK TYPE: %.*s 0x%08X\n", (int)sizeof(chunk_type), chunk_type, *(uint32_t*)chunk_type);

        if(*(uint32_t*)chunk_type == 0x444e4549) {
            quit = true;
        }

        //skip
        if(fseek(input_file, chunk_size, SEEK_CUR) < 0) {
            fprintf(stderr, "ERROR: could not skip: %s\n", strerror(errno));
            exit(1);
        }
        //print CRC 
        uint32_t chunk_crc;
        read_bytes(input_file, &chunk_crc, sizeof(chunk_crc));
        printf("CHUNK CRC: 0x%08X\n", chunk_crc);
        printf("------------------------------\n");
    }

    fclose(input_file);
    return 0;
}
