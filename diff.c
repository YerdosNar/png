#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <zlib.h>

#define PNG_SIG_SIZE 8

const uint8_t png_sig[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

typedef struct {
    uint32_t width, height;
    uint8_t bit_depth, color_type, compression, filter, interlace;
} ihdr_t;

typedef struct {
    uint8_t **pixels; // 2d matrix for output
    uint32_t width, height, channels; //1 - grayscale, 3 - RGB, 4 - RGBA
} image_t;

// Kernel types enum
typedef enum {
    KERNEL_SOBEL_X = 0,
    KERNEL_SOBEL_Y = 1,
    KERNEL_SOBEL_COMBINED = 2,
    KERNEL_GAUSSIAN = 3,
    KERNEL_BLUR = 4,
    KERNEL_LAPLACIAN = 5,
    KERNEL_SHARPEN = 6,
    KERNEL_NONE = 7
} kernel_type_t;

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
    read_bytes(file, &size, 4);
    return ntohl(size);
}

void read_chunk_type(FILE *file, uint8_t type[4]) {
    read_bytes(file, type, 4);
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    read_bytes(file, &crc, 4);
    return ntohl(crc);
}

// CRC calculation for PNG
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

static uint64_t crc(uint8_t *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

void print_bytes(uint8_t *buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size; i++) {
        printf("%u ", buffer[i]);
    }
    printf("\n");
}

void write_chunk(FILE *file, const char *type, uint8_t *data, uint32_t length) {
    uint32_t len_be = htonl(length);
    write_bytes(file, &len_be, sizeof(len_be));
    write_bytes(file, type, 4);
    if(length > 0) {
        write_bytes(file, data, length);
    }

    // Calculate CRC
    uint8_t *crc_buf = malloc(4 + length);
    memcpy(crc_buf, type, 4);
    if(length > 0) {
        memcpy(crc_buf + 4, data, length);
    }
    uint32_t crc_val = htonl(crc(crc_buf, 4 + length));
    write_bytes(file, &crc_val, sizeof(crc_val));
    free(crc_buf);
}

// Allocate 2d matrix for image processing
uint8_t **allocate_pixel_matrix(uint32_t height, uint32_t width) {
    uint8_t **matrix = malloc(height * sizeof(uint8_t*));
    if(!matrix) {
        fprintf(stderr, "ERROR: Could not allocate memory for pixel matrix\n");
        exit(1);
    }

    for(uint32_t i = 0; i < height; i++) {
        matrix[i] = calloc(width, sizeof(uint8_t));
        if(!matrix[i]) {
            fprintf(stderr, "ERROR: Could not allocate memory for pixel row\n");
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

// PNG filter types
enum {
    FILTER_NONE  = 0,
    FILTER_SUB   = 1,
    FILTER_UP    = 2,
    FILTER_AVG   = 3, 
    FILTER_PAETH = 4
};

uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);

    if(pa <= pb && pa <= pc) {
        return a;
    } else if(pb <= pc) {
        return b;
    } else {
        return c;
    }
}

void unfilter_scanline(uint8_t *current, uint8_t *previous,
                      uint32_t length, uint32_t bpp,
                      uint8_t filter_type) {
    switch(filter_type) {
        case FILTER_NONE:
            break;

        case FILTER_SUB:
            for(uint32_t i = bpp; i < length; i++) {
                current[i] += current[i - bpp];
            }
            break;

        case FILTER_UP:
            if(previous) {
                for(uint32_t i = 0; i < length; i++) {
                    current[i] += previous[i];
                }
            }
            break;

        case FILTER_AVG:
            for(uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bpp) ? current[i - bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                current[i] += (left + up) / 2;
            }
            break;

        case FILTER_PAETH:
            for(uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bpp) ? current[i - bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                uint8_t up_left = (previous && i >= bpp) ? previous[i - bpp] : 0;
                current[i] += paeth_predictor(left, up, up_left);
            }
            break;
    }
}

image_t *process_idat_chunks(ihdr_t *ihdr,
                            uint8_t *idat_data, uint64_t idat_size) {
    uint32_t bpp = 1;
    uint32_t channels = 1;

    switch(ihdr->color_type) {
        case 0: // Grayscale
            channels = 1;
            bpp = 1;
            break;
        case 2: // RGB
            channels = 3;
            bpp = 3;
            break;
        case 3: // Palette
            fprintf(stderr, "ERROR: Palette images not supported\n");
            return NULL;
        case 4: // Grayscale + Alpha
            channels = 2;
            bpp = 2;
            break;
        case 6: // RGBA
            channels = 4;
            bpp = 4;
            break;
    }

    // Decompress IDAT 
    uint64_t expected_size = ihdr->height * (1 + ihdr->width * bpp);
    uint8_t *decompressed = malloc(expected_size);
    if(!decompressed) {
        fprintf(stderr, "ERROR: Could not allocate memory for decompression\n");
        return NULL;
    }

    int result = uncompress(decompressed, &expected_size, idat_data, idat_size);
    if(result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to decompress image data: %d\n", result);
        free(decompressed);
        return NULL;
    }

    image_t *image = malloc(sizeof(image_t));
    image->width = ihdr->width;
    image->height = ihdr->height;
    image->channels = channels;
    image->pixels = allocate_pixel_matrix(ihdr->height, ihdr->width * channels);

    // Process scanlines
    uint32_t scanline_length = ihdr->width * bpp;
    uint8_t *previous_scanline = NULL;

    for(uint32_t y = 0; y < ihdr->height; y++) {
        uint32_t offset = y * (1 + scanline_length);
        uint8_t filter_type = decompressed[offset];
        uint8_t *scanline = &decompressed[offset + 1];

        unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);
        memcpy(image->pixels[y], scanline, scanline_length);
        previous_scanline = scanline;
    }

    free(decompressed);
    return image;
}

uint8_t **rgb_to_grayscale(image_t *image) {
    if(image->channels == 1) {
        return image->pixels;
    }

    uint8_t **gray = allocate_pixel_matrix(image->height, image->width);
    for(uint32_t y = 0; y < image->height; y++) {
        for(uint32_t x = 0; x < image->width; x++) {
            if(image->channels >= 3) {
                uint8_t r = image->pixels[y][x * image->channels + 0];
                uint8_t g = image->pixels[y][x * image->channels + 1];
                uint8_t b = image->pixels[y][x * image->channels + 2];
                gray[y][x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            } else if(image->channels == 2) {
                gray[y][x] = image->pixels[y][x * 2];
            }
        }
    }
    return gray;
}

void apply_convolution(uint8_t **input, uint8_t **output,
                      uint32_t height, uint32_t width,
                      kernel_type_t kernel_type, bool preserve_color) {
    
    float kernels[7][3][3] = {
        { // Sobel X
            {-1, 0, 1},
            {-2, 0, 2},
            {-1, 0, 1}
        },
        { // Sobel Y
            {-1, -2, -1},
            { 0,  0,  0},
            { 1,  2,  1}
        },
        { // Sobel Combined (placeholder - handled separately)
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
        },
        { // Gaussian blur
            {1/16.0, 2/16.0, 1/16.0},
            {2/16.0, 4/16.0, 2/16.0},
            {1/16.0, 2/16.0, 1/16.0}
        },
        { // Box blur
            {1/9.0, 1/9.0, 1/9.0},
            {1/9.0, 1/9.0, 1/9.0},
            {1/9.0, 1/9.0, 1/9.0}
        },
        { // Laplacian edge detection
            { 0, -1,  0},
            {-1,  4, -1},
            { 0, -1,  0}
        },
        { // Sharpen
            { 0, -1,  0},
            {-1,  5, -1},
            { 0, -1,  0}
        }
    };

    // Handle special case for Sobel combined
    if(kernel_type == KERNEL_SOBEL_COMBINED) {
        for(uint32_t y = 1; y < height - 1; y++) {
            for(uint32_t x = 1; x < width - 1; x++) {
                int gx = 0, gy = 0;
                
                // Apply both Sobel X and Y
                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        gx += pixel * kernels[KERNEL_SOBEL_X][ky + 1][kx + 1];
                        gy += pixel * kernels[KERNEL_SOBEL_Y][ky + 1][kx + 1];
                    }
                }
                
                int magnitude = (int)sqrt(gx * gx + gy * gy);
                output[y][x] = (magnitude > 255) ? 255 : magnitude;
            }
        }
    } else if(kernel_type < KERNEL_NONE) {
        // Apply single kernel
        for(uint32_t y = 1; y < height - 1; y++) {
            for(uint32_t x = 1; x < width - 1; x++) {
                float sum = 0;
                
                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        sum += pixel * kernels[kernel_type][ky + 1][kx + 1];
                    }
                }
                
                // Clamp to valid range
                if(sum < 0) sum = 0;
                if(sum > 255) sum = 255;
                output[y][x] = (uint8_t)sum;
            }
        }
    } else {
        // No kernel - just copy
        for(uint32_t y = 0; y < height; y++) {
            memcpy(output[y], input[y], width);
        }
        return;
    }
    
    // Handle borders
    for(uint32_t i = 0; i < width; i++) {
        output[0][i] = input[0][i];
        output[height - 1][i] = input[height - 1][i];
    }
    for(uint32_t i = 0; i < height; i++) {
        output[i][0] = input[i][0];
        output[i][width - 1] = input[i][width - 1];
    }
}

void save_png(const char *filename, uint8_t **pixels,
              uint32_t width, uint32_t height,
              uint8_t color_type, uint32_t channels) {
    FILE *file = fopen(filename, "wb");
    if(!file) {
        fprintf(stderr, "ERROR: Could not create output file %s\n", filename);
        exit(1);
    }

    write_bytes(file, png_sig, PNG_SIG_SIZE);

    // IHDR
    uint8_t ihdr_data[13];
    uint32_t width_be = htonl(width);
    uint32_t height_be = htonl(height);
    memcpy(ihdr_data, &width_be, sizeof(width_be));
    memcpy(ihdr_data + 4, &height_be, sizeof(height_be));
    ihdr_data[8] = 8;         // bit depth
    ihdr_data[9] = color_type;
    ihdr_data[10] = 0;        // compression method
    ihdr_data[11] = 0;        // filter method
    ihdr_data[12] = 0;        // interlace method
    
    write_chunk(file, "IHDR", ihdr_data, sizeof(ihdr_data));

    // Prepare image data with filter bytes
    uint32_t bytes_per_pixel = (color_type == 0) ? 1 : channels;
    uint64_t raw_size = height * (1 + width * bytes_per_pixel);
    uint8_t *raw_data = malloc(raw_size);

    for(uint32_t y = 0; y < height; y++) {
        raw_data[y * (1 + width * bytes_per_pixel)] = 0; // Filter type: None
        memcpy(raw_data + y * (1 + width * bytes_per_pixel) + 1, 
               pixels[y], width * bytes_per_pixel);
    }

    // Compress
    uint64_t compressed_size = compressBound(raw_size);
    uint8_t *compressed_data = malloc(compressed_size);

    int result = compress2(compressed_data, &compressed_size, raw_data, raw_size, Z_DEFAULT_COMPRESSION);
    if(result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to compress image data\n");
        free(raw_data);
        free(compressed_data);
        fclose(file);
        exit(1);
    }

    write_chunk(file, "IDAT", compressed_data, compressed_size);
    write_chunk(file, "IEND", NULL, 0);

    free(raw_data);
    free(compressed_data);
    fclose(file);

    printf("Successfully saved output image to: %s\n", filename);
}

void print_usage(char *exec_name) {
    printf("Usage: %s <input.png> -o <output.png> [options]\n", exec_name);
    printf("\nOptions:\n");
    printf("  -o, --output <file>    Output filename (default: out.png)\n");
    printf("  -i, --info             Display PNG file information only\n");
    printf("  -g, --grayscale        Convert to grayscale\n");
    printf("  --rgb                  Keep RGB format (default)\n");
    printf("  --sobel-x              Apply Sobel X edge detection\n");
    printf("  --sobel-y              Apply Sobel Y edge detection\n");
    printf("  --sobel                Apply combined Sobel edge detection\n");
    printf("  --gaussian             Apply Gaussian blur\n");
    printf("  --blur [steps]         Apply box blur (optional: number of iterations)\n");
    printf("  --laplacian            Apply Laplacian edge detection\n");
    printf("  --sharpen              Apply sharpening filter\n");
    printf("  --none                 No filter (default)\n");
    printf("  -h, --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s input.png -o edges.png --sobel --grayscale\n", exec_name);
    printf("  %s photo.png -o blurred.png --blur 3\n", exec_name);
    printf("  %s image.png --info\n", exec_name);
}

// Print PNG file information
void print_info(FILE *file) {
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(file, signature, PNG_SIG_SIZE);
    
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: Not a valid PNG file\n");
        return;
    }
    
    printf("PNG File Information:\n");
    printf("===================\n");
    
    bool quit = false;
    while(!quit) {
        uint32_t chunk_size = read_chunk_size(file);
        uint8_t chunk_type[4];
        read_chunk_type(file, chunk_type);
        
        printf("Chunk: %.4s (size: %u bytes)\n", chunk_type, chunk_size);
        
        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            ihdr_t ihdr;
            read_bytes(file, &ihdr.width, 4);
            read_bytes(file, &ihdr.height, 4);
            read_bytes(file, &ihdr.bit_depth, 1);
            read_bytes(file, &ihdr.color_type, 1);
            read_bytes(file, &ihdr.compression, 1);
            read_bytes(file, &ihdr.filter, 1);
            read_bytes(file, &ihdr.interlace, 1);
            
            ihdr.width = ntohl(ihdr.width);
            ihdr.height = ntohl(ihdr.height);
            
            printf("  Dimensions: %u x %u pixels\n", ihdr.width, ihdr.height);
            printf("  Bit depth: %u\n", ihdr.bit_depth);
            printf("  Color type: %u (", ihdr.color_type);
            switch(ihdr.color_type) {
                case 0: printf("Grayscale"); break;
                case 2: printf("RGB"); break;
                case 3: printf("Palette"); break;
                case 4: printf("Grayscale + Alpha"); break;
                case 6: printf("RGBA"); break;
                default: printf("Unknown");
            }
            printf(")\n");
            printf("  Compression: %u\n", ihdr.compression);
            printf("  Filter: %u\n", ihdr.filter);
            printf("  Interlace: %u\n", ihdr.interlace);
        }
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
            // Skip chunk data
            fseek(file, chunk_size, SEEK_CUR);
        }
        
        read_chunk_crc(file);
    }
}
