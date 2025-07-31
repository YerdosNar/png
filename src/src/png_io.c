#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <zlib.h>
#include "../include/utils.h"
#include "../include/png_io.h"

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
    reverse_bytes(&size, sizeof(size));
    return size;
}

void read_chunk_type(FILE *file, uint8_t chunk_type[]) {
    size_t chunk_type_size = 4;
    read_bytes(file, chunk_type, chunk_type_size);
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    size_t crc_size = 4;
    read_bytes(file, &crc, crc_size);
    reverse_bytes(&crc, sizeof(crc));
    return crc;
}

void write_chunk(FILE *file, const char *type, uint8_t *data, uint32_t length) {
    reverse_bytes(&length, sizeof(length));
    write_bytes(file, &length, sizeof(length));
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
    uint32_t crc_value = crc(crc_buffer, length + 4);
    reverse_bytes(&crc_value, sizeof(crc_value));
    write_bytes(file, &crc_value, sizeof(crc_value));
    free(crc_buffer);
}

void save_png(const char *filename, uint8_t **pixels, ihdr_t ihdr, uint32_t channels) {
    FILE *file = fopen(filename, "wb");
    if(!file) {
        fprintf(stderr, "ERROR: Could not create output file: %s\n", filename);
        exit(1);
    }

    uint32_t width = ihdr.width;
    uint32_t height = ihdr.height;
    uint8_t bit_depth = ihdr.bit_depth;
    uint8_t color_type = ihdr.color_type;
    uint8_t compression = ihdr.compression;
    uint8_t filter = ihdr.filter;
    uint8_t interlace = ihdr.interlace;
    

    write_bytes(file, png_signature, PNG_SIG_SIZE);

    // now IHDR
    uint8_t ihdr_data[13]; // IHDR size 13: 4-height, 4-width, 1...
    reverse_bytes(&height, sizeof(height));
    reverse_bytes(&width, sizeof(width));
    memcpy(ihdr_data+4, &height, sizeof(height));
    memcpy(ihdr_data, &width, sizeof(width));
    ihdr_data[8] = bit_depth;
    ihdr_data[9] = color_type;
    ihdr_data[10]= compression;
    ihdr_data[11]= filter;
    ihdr_data[12]= interlace;

    write_chunk(file, "IHDR", ihdr_data, sizeof(ihdr_data));

    // data with filter bytes
    uint32_t bytes_per_pixel = (color_type == 0) ? 1 : channels;
    uint32_t depth = 1 + width * bytes_per_pixel;
    uint64_t raw_size = height * depth;
    uint8_t *raw_data = malloc(raw_size); // FREE IT<- FLAG

    for(uint32_t y = 0; y < height; y++) {
        raw_data[y * depth] = 0; // filter type: none
        memcpy(raw_data + y * depth + 1, pixels[y], width * bytes_per_pixel);
    }

    // compress
    uint64_t compressed_size = compressBound(raw_size);
    uint8_t *compressed_data = malloc(compressed_size); // FREE IT <- FLAG
    int32_t result = compress2(compressed_data, &compressed_size, raw_data, raw_size, Z_DEFAULT_COMPRESSION);
    if(result != Z_OK) {
        fprintf(stderr, "ERROR: compress2 function failed\n");
        free(raw_data);
        free(compressed_data);
        fclose(file);
        exit(1);
    }

    // write IDAT
    write_chunk(file, "IDAT", compressed_data, compressed_size);

    // write IEND
    write_chunk(file, "IEND", NULL, 0);

    free(raw_data);
    free(compressed_data);
    fclose(file);

    printf("Successfully saved output image to: %s\n", filename);
}

void print_bytes(uint8_t *buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size-1; i++) {
        printf("%u ", buffer[i]);
    }
    printf("%u\n", buffer[buffer_size-1]);
}

void print_info(FILE *file) {
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(file, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_signature, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: is not a PNG file\n");
        fclose(file);
        exit(1);
    }
    printf("PNG File Information:\n");
    print_bytes(signature, PNG_SIG_SIZE);
    printf("======== INFO ========\n");
    
    bool quit = false;
    while(!quit) {
        uint32_t chunk_size = read_chunk_size(file);
        uint8_t chunk_type[4];
        read_chunk_type(file, chunk_type);

        printf("Chunk: %.4s (size: %u KB)\n", chunk_type, chunk_size/1024);

        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            ihdr_t ihdr;
            read_bytes(file, &ihdr.width, 4);
            read_bytes(file, &ihdr.height, 4);
            read_bytes(file, &ihdr.bit_depth, 1);
            read_bytes(file, &ihdr.color_type, 1);
            read_bytes(file, &ihdr.compression, 1);
            read_bytes(file, &ihdr.filter, 1);
            read_bytes(file, &ihdr.interlace, 1);

            reverse_bytes(&ihdr.width, sizeof(ihdr.width));
            reverse_bytes(&ihdr.height, sizeof(ihdr.width));

            printf("  Dimensions:  %u x %u pixels\n", ihdr.width, ihdr.height);
            printf("  Bit depth:   %u\n", ihdr.bit_depth);
            printf("  Color type:  %u (", ihdr.color_type);
            switch(ihdr.color_type) {
                case 0: printf("Grayscale"); break;
                case 2: printf("RGB"); break;
                case 3: printf("Palette"); break;
                case 4: printf("Grayscale + Alpha"); break;
                case 6: printf("RGB + Alpha"); break;
                default: printf("Unknown");
            }
            printf(")\n");
            printf("  Compression: %u\n", ihdr.compression);
            printf("  Filter:      %u\n", ihdr.filter);
            printf("  Interlace:   %u\n", ihdr.interlace); }
        else if(memcmp(chunk_type, "tEXt", 4) == 0) {
            uint8_t *text_data = malloc(chunk_size + 1);
            read_bytes(file, text_data, chunk_size);
            text_data[chunk_size] = '\0';

            char *keyword = (char*)text_data;
            char *text = keyword + strlen(keyword) + 1;
            printf("  Text: %s = %s\n", keyword, text);
            free(text_data);
        }
        else if(memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        }
        else {
            fseek(file, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(file);
    }
}
