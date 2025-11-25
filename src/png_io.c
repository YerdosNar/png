#include "../include/processor.h"
#include "../include/png_io.h"

const uint8_t png_sig[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

void print_bytes(uint8_t *buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size-1; i++) {
        printf("%u ", buffer[i]);
    }
    printf("%u\n", buffer[buffer_size-1]);
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

bool read_png_file(const char *filename, png_data_t *png_data) {
    // Initialize png_data structure
    memset(png_data, 0, sizeof(png_data_t));

    FILE *input_fp = fopen(filename, "rb");
    if (!input_fp) {
        fprintf(stderr, "ERROR: Could not open input file %s\n", filename);
        return false;
    }

    // Check PNG signature
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_fp, signature, PNG_SIG_SIZE);
    if (memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", filename);
        fclose(input_fp);
        return false;
    }

    printf("Processing: %s\n", filename);

    uint64_t idat_capacity = 0;
    bool quit = false;

    while (!quit) {
        uint32_t chunk_size = read_chunk_size(input_fp);
        uint8_t chunk_type[4];
        read_chunk_type(input_fp, chunk_type);

        if (memcmp(chunk_type, "IHDR", 4) == 0) {
            read_bytes(input_fp, &png_data->ihdr.width, 4);
            read_bytes(input_fp, &png_data->ihdr.height, 4);
            read_bytes(input_fp, &png_data->ihdr.bit_depth, 1);
            read_bytes(input_fp, &png_data->ihdr.color_type, 1);
            read_bytes(input_fp, &png_data->ihdr.compression, 1);
            read_bytes(input_fp, &png_data->ihdr.filter, 1);
            read_bytes(input_fp, &png_data->ihdr.interlace, 1);

            reverse(&png_data->ihdr.width, sizeof(png_data->ihdr.width));
            reverse(&png_data->ihdr.height, sizeof(png_data->ihdr.height));

            printf("Image dimensions: %u x %u\n", png_data->ihdr.width, png_data->ihdr.height);
            printf("Bit depth: %u, Color type: %u\n", png_data->ihdr.bit_depth, png_data->ihdr.color_type);
        } else if (memcmp(chunk_type, "PLTE", 4) == 0) {
            png_data->palette.entry_count = chunk_size / 3;
            png_data->palette.entries = malloc(chunk_size);
            if (!png_data->palette.entries) {
                fprintf(stderr, "ERROR: Could not allocate memory for PLTE\n");
                fclose(input_fp);
                return false;
            }
            read_bytes(input_fp, png_data->palette.entries, chunk_size);
        } else if (memcmp(chunk_type, "tRNS", 4) == 0) {
            png_data->palette.alpha_count = chunk_size;
            png_data->palette.alphas = malloc(chunk_size);
            if (!png_data->palette.alphas) {
                fprintf(stderr, "ERROR: Could not allocate memory for tRNS\n");
                fclose(input_fp);
                return false;
            }
            read_bytes(input_fp, png_data->palette.alphas, chunk_size);
        } else if (memcmp(chunk_type, "IDAT", 4) == 0) {
            if (idat_capacity < png_data->idat_size + chunk_size) {
                idat_capacity = (png_data->idat_size + chunk_size) * 2;
                uint8_t *new_idat_data = realloc(png_data->idat_data, idat_capacity);
                if (!new_idat_data) {
                    fprintf(stderr, "ERROR: Could not reallocate memory for IDAT\n");
                    free(png_data->idat_data);
                    fclose(input_fp);
                    return false;
                }
                png_data->idat_data = new_idat_data;
            }
            read_bytes(input_fp, png_data->idat_data + png_data->idat_size, chunk_size);
            png_data->idat_size += chunk_size;
        } else if (memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        } else {
            fseek(input_fp, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(input_fp);
    }

    fclose(input_fp);
    return true;
}

void free_png_data(png_data_t *png_data) {
    if (png_data->palette.entries) {
        free(png_data->palette.entries);
        png_data->palette.entries = NULL;
    }
    if (png_data->palette.alphas) {
        free(png_data->palette.alphas);
        png_data->palette.alphas = NULL;
    }
    if (png_data->idat_data) {
        free(png_data->idat_data);
        png_data->idat_data = NULL;
    }
}

void print_info(FILE *file, char *filename) {
    rewind(file);
    fseek(file, 0, SEEK_END);
    uint32_t total_size = ftell(file);
    rewind(file);
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(file, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: is not a PNG file\n");
        fclose(file);
        exit(1);
    }

    printf("PNG File Name : \033[32m%s\033[0m\n", filename);
    printf("PNG Signature : \033[32m");
    print_bytes(signature, PNG_SIG_SIZE);
    printf("\033[0m");
    if(total_size > 1024*1024) {
        printf("PNG Total Size: \033[32m%.2f KB\n", (float)(total_size)/(1024*1024));
    } else if(total_size > 1024) {
        printf("PNG Total Size: \033[32m%.2f KB\n", (float)(total_size)/1024);
    } else {
        printf("PNG Total Size: \033[32m%u B\n", total_size);
    }
    printf("\033[0m                 +======+\n");
    printf(" +================ INFO =================+\n");
    printf("||               +======+                ||\n");
    printf("||                                       ||\n");

    bool quit = false;
    bool no_print = false;
    while(!quit) {
        uint32_t chunk_size = read_chunk_size(file);
        uint8_t chunk_type[4];
        read_chunk_type(file, chunk_type);

        if(chunk_size > 1024*1024) {
            float chunk_size_MB = (float)chunk_size / (1024 * 1024);
            printf("||  Chunk: \033[32m%.*s\033[0m (size: %-3.2f MB)%-9s||\n", (int)sizeof(chunk_type), chunk_type, chunk_size_MB, "");
        } else if(chunk_size > 1024) {
            float chunk_size_KB = (float)chunk_size / 1024;
            printf("||  Chunk: \033[32m%.*s\033[0m (size: %-3.2f KB)%-10s||\n", (int)sizeof(chunk_type), chunk_type, chunk_size_KB, "");
        } else {
            printf("||  Chunk: \033[32m%.*s\033[0m (size: %-3u B)%-12s||\n", (int)sizeof(chunk_type), chunk_type, chunk_size, "");
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

            printf("||                                       ||\n");
            printf("||    %-12s : %-4u x %-4u pixels%-2s||\n", "Dimensions", ihdr.width, ihdr.height, "");
            printf("||    %-12s : %u%-19s||\n", "Bit depth", ihdr.bit_depth, "");
            printf("||    %-12s : %u (", "Color type", ihdr.color_type);
            switch(ihdr.color_type) {
                case 0: printf("%-17s", "\e[100mGrayscale\e[0m)       "); break;
                case 2: printf("%-17s", "\e[1m\033[31mR\033[32mG\033[34mB\033[0m\e[0m)             "); break;
                case 3: printf("%-17s", "Palette)"); break;
                case 4: printf("%-17s", "Grayscale + Alpha)"); break;
                case 6: printf("%-17s", "\e[1m\033[31mR\033[32mG\033[34mB\033[0m\e[0m + Alpha)     "); break;
                default: printf("%-17s", "Unknown)");
            }
            printf("||\n");
            printf("||    %-12s : %u%-19s||\n", "Compression", ihdr.compression, "");
            printf("||    %-12s : %u%-19s||\n", "Filter", ihdr.filter, "");
            printf("||    %-12s : %u%-19s||\n", "Interlace", ihdr.interlace, "");
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
        //             printf("||  Text: %s = %s\n", keyword, text);
        //         }
        //         free(text_data);
        //     } else {
        //         fseek(file, chunk_size, SEEK_CUR);
        //     }
        // }
        else if((memcmp(chunk_type, "PLTE", 4) == 0) ||
                (memcmp(chunk_type, "tRNS", 4) == 0) ||
                (memcmp(chunk_type, "pHYs", 4) == 0) ||
                (memcmp(chunk_type, "IDAT", 4) == 0)) {
            fseek(file, chunk_size, SEEK_CUR);
        }
        else if(memcmp(chunk_type, "IEND", 4) == 0) {
            no_print = true;
            quit = true;
        }
        else {
            char buffer[chunk_size];
            read_bytes(file, &buffer, sizeof(buffer));
            buffer[chunk_size-1] = '\0';
            printf("||                                       ||\n");
            if(strlen(buffer) > 25) {
                printf("||   Text: \033[33m%-27.27s\033[0m...||\n", buffer);
            }
            else {
                printf("||   Text: \033[33m%-30s\033[0m||\n", buffer);
            }
        }
        printf("||                                       ||\n");
        printf(" +=======================================+\n");
        if(!no_print) {
            printf("||                                       ||\n");
        }

        read_chunk_crc(file);
    }
}

void draw_ascii(FILE *file, bool color) {
    if(color) {
        printf("Draw handling in PNG_IO color\n");
    }
    else {
        printf("Draw handling in PNG_IO no color\n");
    }
}
