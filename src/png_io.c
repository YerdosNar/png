#include "../include/processor.h"
#include "../include/png_io.h"

const uint8_t png_sig[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

void print_bytes(uint8_t *buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size; i++) {
        printf("%u ", buffer[i]);
    }
    printf("\n");
}

void read_bytes(FILE *file, void *buffer, size_t size) {
    size_t n = fread(buffer, size, 1, file);
    if(1 != n) {
        if(ferror(file)) {
            fprintf(stderr, "ERROR: Could not read %zu bytes from file: %s\n", size, strerror(errno));
            exit(1);
        } else if(feof(file)) {
            fprintf(stderr, "ERROR: Could not read %zu bytes from file: reached EOF %s\n", size, strerror(errno));
            exit(1);
        }
    }
}

void write_bytes(FILE *file, const void *buffer, size_t size) {
    size_t n = fwrite(buffer, size, 1, file);
    if(1 != n) {
        fprintf(stderr, "ERROR: Could not write %zu bytes to file: %s\n", size, strerror(errno));
        exit(1);
    }
}

uint32_t read_chunk_size(FILE *file) {
    uint32_t size;
    read_bytes(file, &size, 4);
    reverse(&size, sizeof(size));
    return size;
}

void write_chunk_size(FILE *file, uint32_t size) {
    write_bytes(file, &size, sizeof(size));
}

void read_chunk_type(FILE *file, uint8_t *type) {
    read_bytes(file, type, 4);
}

void write_chunk_type(FILE *file, const char type[]) {
    write_bytes(file, type, 4);  // Fixed: removed & operator
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    read_bytes(file, &crc, sizeof(crc));
    reverse(&crc, sizeof(crc));
    return crc;
}

void write_chunk_crc(FILE *file, uint32_t crc) {
    write_bytes(file, &crc, sizeof(crc));
}

void write_chunk(FILE *file, const char type[], uint8_t *data, uint32_t length_le) {
    // Convert length to big endian for writing
    uint32_t length_be = length_le;
    reverse(&length_be, sizeof(length_be));
    write_bytes(file, &length_be, sizeof(length_be));

    // Write chunk type
    write_bytes(file, type, 4);  // Fixed: removed & operator

    // Write data if it exists
    if(length_le > 0 && data != NULL) {
        write_bytes(file, data, length_le);
    }

    // Calculate and write CRC
    uint8_t *crc_buf = malloc(4 + length_le);
    if(!crc_buf) {
        fprintf(stderr, "ERROR: Could not allocate memory for CRC calculation\n");
        exit(1);
    }

    memcpy(crc_buf, type, 4);
    if(length_le > 0 && data != NULL) {
        memcpy(crc_buf + 4, data, length_le);
    }

    uint32_t crc_val = crc(crc_buf, 4 + length_le);
    reverse(&crc_val, sizeof(crc_val));
    write_bytes(file, &crc_val, sizeof(crc_val));

    free(crc_buf);
}

void save_png(const char *filename, uint8_t **pixels,
              uint32_t width, uint32_t height,
              uint8_t color_type, uint32_t channels) {
    FILE *file = fopen(filename, "wb");
    if(!file) {
        fprintf(stderr, "ERROR: Could not create output file %s\n", filename);
        exit(1);
    }

    // Write PNG signature
    write_bytes(file, png_sig, PNG_SIG_SIZE);

    // Create IHDR chunk
    uint8_t ihdr_data[13];
    uint32_t width_be = width;
    uint32_t height_be = height;
    reverse(&width_be, sizeof(width_be));
    reverse(&height_be, sizeof(height_be));

    memcpy(ihdr_data, &width_be, sizeof(width_be));
    memcpy(ihdr_data + 4, &height_be, sizeof(height_be));
    ihdr_data[8] = 8;          // bit depth
    ihdr_data[9] = color_type; // color type
    ihdr_data[10] = 0;         // compression method
    ihdr_data[11] = 0;         // filter method
    ihdr_data[12] = 0;         // interlace method

    write_chunk(file, "IHDR", ihdr_data, sizeof(ihdr_data));

    // Prepare image data with filter bytes
    uint32_t bytes_per_pixel = (color_type == 0) ? 1 : channels;
    uint64_t raw_size = height * (1 + width * bytes_per_pixel);
    uint8_t *raw_data = malloc(raw_size);
    if(!raw_data) {
        fprintf(stderr, "ERROR: Could not allocate memory for raw data\n");
        fclose(file);
        exit(1);
    }

    // Copy pixel data with filter bytes
    for(uint32_t y = 0; y < height; y++) {
        uint64_t row_offset = y * (1 + width * bytes_per_pixel);
        raw_data[row_offset] = 0; // filter type: None
        memcpy(raw_data + row_offset + 1, pixels[y], width * bytes_per_pixel);
    }

    // Compress data
    uLongf compressed_size = compressBound(raw_size);
    uint8_t *compressed_data = malloc(compressed_size);
    if(!compressed_data) {
        fprintf(stderr, "ERROR: Could not allocate memory for compressed data\n");
        free(raw_data);
        fclose(file);
        exit(1);
    }

    int result = compress2(compressed_data, &compressed_size, raw_data, raw_size, Z_DEFAULT_COMPRESSION);
    if(result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to compress image data (error: %d)\n", result);
        free(raw_data);
        free(compressed_data);
        fclose(file);
        exit(1);
    }

    // Write IDAT chunk
    write_chunk(file, "IDAT", compressed_data, (uint32_t)compressed_size);

    // Write IEND chunk
    write_chunk(file, "IEND", NULL, 0);

    free(raw_data);
    free(compressed_data);
    fclose(file);

    printf("Successfully saved output image to: %s\n", filename);
}

void print_info(FILE *file) {
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(file, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: is not a PNG file\n");
        fclose(file);
        exit(1);
    }
    printf("PNG File Information:\n");
    print_bytes(signature, PNG_SIG_SIZE);
    printf("+=============== INFO ================+\n");

    bool quit = false;
    while(!quit) {
        uint32_t chunk_size = read_chunk_size(file);
        uint8_t chunk_type[4];
        read_chunk_type(file, chunk_type);

        if(chunk_size > 100*1024) {
            float chunk_size_KB = (float)chunk_size / 1024;
            printf("|Chunk: %.*s (size: %.2f KB)%-8s|\n", (int)sizeof(chunk_type), chunk_type, chunk_size_KB, "");
        } else {
            printf("|Chunk: %.*s (size: %.2u B)%-13s|\n", (int)sizeof(chunk_type), chunk_type, chunk_size, "");
        }

        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            ihdr_t ihdr;
            read_bytes(file, &ihdr.width, 4);
            read_bytes(file, &ihdr.height, 4);
            read_bytes(file, &ihdr.bit_depth, 1);
            read_bytes(file, &ihdr.color_type, 1);
            read_bytes(file, &ihdr.compression, 1);
            read_bytes(file, &ihdr.filter, 1);
            read_bytes(file, &ihdr.interlace, 1);

            reverse(&ihdr.width, sizeof(ihdr.width));
            reverse(&ihdr.height, sizeof(ihdr.height));

            printf("|  %-12s : %u x %u pixels%-4s|\n", "Dimensions", ihdr.width, ihdr.height, "");
            printf("|  %-12s : %u%-19s|\n", "Bit depth", ihdr.bit_depth, "");
            printf("|  %-12s : %u (", "Color type", ihdr.color_type);
            switch(ihdr.color_type) {
                case 0: printf("Grayscale"); break;
                case 2: printf("RGB"); break;
                case 3: printf("Palette"); break;
                case 4: printf("Grayscale + Alpha"); break;
                case 6: printf("RGB + Alpha"); break;
                default: printf("Unknown");
            }
            printf(")%-5s|\n", "");
            printf("|  %-12s : %u%-19s|\n", "Compression", ihdr.compression, "");
            printf("|  %-12s : %u%-19s|\n", "Filter", ihdr.filter, "");
            printf("|  %-12s : %u%-19s|\n", "Interlace", ihdr.interlace, "");
        }
        // else if(memcmp(chunk_type, "tEXt", 4) == 0) {
        //     uint8_t *text_data = malloc(chunk_size + 1);
        //     if(text_data) {
        //         read_bytes(file, text_data, chunk_size);
        //         text_data[chunk_size] = '\0';
        //
        //         char *keyword = (char*)text_data;
        //         char *text = keyword + strlen(keyword) + 1;
        //         if(text < (char*)text_data + chunk_size) {
        //             printf("|  Text: %s = %s\n", keyword, text);
        //         }
        //         free(text_data);
        //     } else {
        //         fseek(file, chunk_size, SEEK_CUR);
        //     }
        // }
        else if(memcmp(chunk_type, "IDAT", 4) == 0) {
            fseek(file, chunk_size, SEEK_CUR);
        }
        else if(memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        }
        else {
            char buffer[chunk_size];
            read_bytes(file, &buffer, sizeof(buffer));
            buffer[chunk_size-1] = '\0';
            if(strlen(buffer) > 25) {
                printf("| Text: %-27.27s...|\n", buffer);
            }
            else {
                printf("| Text: %-30s|\n", buffer);
            }
        }
        printf("+=====================================+\n");

        read_chunk_crc(file);
    }
}
