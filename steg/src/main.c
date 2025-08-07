#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../include/png_io.h"
#include "../include/utils.h"

void check_print(uint8_t sig[], char *input_file) {
    if(memcmp(sig, png_signature, 8) != 0) {
        fprintf(stderr, "ERROR: %s is NOT a png file\n", input_file);
        exit(1);
    } else {
        printf("%s is valid png file\n", input_file);
    }
}

int main(int argc, char **argv) {
    char *output_file;
    if(argc < 3) {
        if(argc < 2) {
            fprintf(stderr, "Usage: %s <input.png>", argv[0]);
            return 1;
        }
        output_file = "out.png";
    } else {
        output_file = argv[2];
    }

    char *input_file = argv[1];
    FILE *in = fopen(input_file, "rb");
    if(!in) {
        fprintf(stderr, "ERROR: Could not open %s file\n", input_file);
        return 1;
    }

    uint8_t sig[8];
    read_bytes(in, &sig, sizeof(sig));
    check_print(sig, input_file);
    printf("\n=================\n");

    bool quit = false;
    ihdr_t ihdr;
    chunk_t iend;
    image_t png;
    png.chunks = NULL;
    png.num_chunks = 0;
    while(!quit) {
        uint32_t size;
        uint8_t type[4];
        read_bytes(in, &size, sizeof(size));
        read_bytes(in, &type, sizeof(type));
        uint32_t size_be = size;
        reverse(&size_be, sizeof(size_be));

        printf("TYPE: %.*s, SIZE: %u\n", (int)sizeof(type), type, size_be);
        if(memcmp(type, "IHDR", 4) == 0) {
            ihdr.size = size;
            memcpy(&ihdr.type, type, 4);
            read_ihdr(in, &ihdr);
            uint32_t width_be = ihdr.width;
            uint32_t height_be = ihdr.height;
            reverse(&width_be, sizeof(width_be));
            reverse(&height_be, sizeof(height_be));
            printf("  Width:       %u px\n", width_be);
            printf("  Height:      %u px\n", height_be);
            printf("  Bit depth:   %u\n", ihdr.bit_depth);
            printf("  Color type:  %u->", ihdr.color_type);
            switch(ihdr.color_type) {
                case 0: printf("Grayscale\n"); break;
                case 2: printf("RGB\n"); break;
                case 3: printf("Palette\n"); break;
                case 4: printf("Grayscale+Alpha\n"); break;
                case 6: printf("RGB+Alpha\n"); break;
                default: printf("Invalid value: %u\n", ihdr.color_type);
                         break;
            }
            printf("  Compression: %u\n", ihdr.compression);
            printf("  Filter:      %u\n", ihdr.compression);
            printf("  Interlace:   %u\n", ihdr.interlace);
        }
        else if(memcmp(type, "IDAT", 4) == 0) {
            png.num_chunks++;
            png.chunks = realloc(png.chunks, png.num_chunks * sizeof(chunk_t));
            chunk_t *current_chunk = &png.chunks[png.num_chunks-1];
            //structure:
            //size,
            //type,
            //data,
            //crc
            current_chunk->size = size;
            memcpy(current_chunk->type, type, 4);
            current_chunk->data = malloc(size_be * sizeof(uint8_t));
            read_bytes(in, current_chunk->data, size_be);
            read_bytes(in, &current_chunk->crc, sizeof(current_chunk->crc));
        }
        else if(memcmp(type, "IEND", 4) == 0) {
            printf("Reached EOF aka IEND chunk\n");
            read_bytes(in, &iend.crc, sizeof(iend.crc));
            quit = true;
        }
        else {
            printf("Found an external chunk:\n");
            chunk_t ext_chunk;
            ext_chunk.data = malloc(size_be * sizeof(uint8_t));
            read_bytes(in, ext_chunk.data, size_be);
            printf("The hidden message: %s\n", ext_chunk.data);
            fseek(in, 4, SEEK_CUR);
        }
        printf("\n=================\n");
    }
    fclose(in);

    FILE *out = fopen(output_file, "wb");
    if(!out) {
        fprintf(stderr, "ERROR: Could not open output file: %s\n", output_file);
        return 1;
    }

    write_bytes(out, &sig, 8);
    printf("SIG written\n");

    write_ihdr(out, ihdr);
    printf("IHDR written\n");
    for(uint32_t i = 0; i < png.num_chunks; i++) {
        uint32_t size_be = png.chunks[i].size;
        reverse(&size_be, sizeof(size_be));
        write_chunk(out, png.chunks[i], size_be);
        printf("IDAT written\n");
    }

    chunk_t custom;
    uint32_t cstm_size = strlen(argv[3]);
    custom.size = cstm_size;
    reverse(&custom.size, sizeof(custom.size));
    custom.data = malloc(cstm_size);
    memcpy(custom.data, argv[3], cstm_size);
    memcpy(custom.type, "cSTM", 4);
    write_chunk(out, custom, cstm_size);
    free(custom.data);

    uint32_t endsize = 0;
    reverse(&endsize, sizeof(endsize));
    write_bytes(out, &endsize, sizeof(endsize));
    printf("IEND size written\n");
    write_bytes(out, "IEND", 4);
    printf("IEND type written\n");
    printf("IEND data written\n");
    write_bytes(out, &iend.crc, sizeof(iend.crc));
    printf("IEND crc written\n");

    return 0;
}
