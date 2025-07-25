#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <zlib.h>
#include <math.h>

#define PNG_SIG_SIZE 8
const uint8_t png_sig[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
} png_ihdr_t;

typedef struct {
    uint8_t **pixels;  // 2D array of pixel values
    uint32_t width;
    uint32_t height;
    uint32_t channels;   // 1 for grayscale, 3 for RGB, 4 for RGBA
} png_image_t;

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

void print_bytes(uint8_t *buffer, size_t buffer_size) {
    for(size_t i = 0; i < buffer_size; i++) {
        printf("%u ", buffer[i]);
    }
    printf("\n");
}

uint32_t read_chunk_size(FILE *file) {
    uint32_t size;
    read_bytes(file, &size, sizeof(size));
    return ntohl(size);
}

void read_chunk_type(FILE *file, uint8_t type[4]) {
    read_bytes(file, type, 4);
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    read_bytes(file, &crc, sizeof(crc));
    return ntohl(crc);
}

// CRC calculation for PNG chunks
static uint64_t crc_table[256];
static int crc_table_computed = 0;

static void make_crc_table(void) {
    uint64_t c;
    int n, k;
    
    for (n = 0; n < 256; n++) {
        c = (uint64_t) n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

static uint64_t update_crc(uint64_t crc, uint8_t *buf, int len) {
    uint64_t c = crc;
    int n;
    
    if (!crc_table_computed)
        make_crc_table();
    for (n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

static uint64_t crc(uint8_t *buf, int len) {
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

// Write a PNG chunk
void write_chunk(FILE *file, const char *type, uint8_t *data, uint32_t length) {
    uint32_t len_be = htonl(length);
    write_bytes(file, &len_be, 4);
    write_bytes(file, type, 4);
    if (length > 0) {
        write_bytes(file, data, length);
    }
    
    // Calculate and write CRC
    uint8_t *crc_buf = malloc(4 + length);
    memcpy(crc_buf, type, 4);
    if (length > 0) {
        memcpy(crc_buf + 4, data, length);
    }
    uint32_t crc_val = htonl(crc(crc_buf, 4 + length));
    write_bytes(file, &crc_val, 4);
    free(crc_buf);
}

// Allocate 2D pixel matrix
uint8_t** allocate_pixel_matrix(uint32_t height, uint32_t width) {
    uint8_t **matrix = malloc(height * sizeof(uint8_t*));
    if (!matrix) {
        fprintf(stderr, "ERROR: Could not allocate memory for pixel matrix\n");
        exit(1);
    }
    
    for (uint32_t i = 0; i < height; i++) {
        matrix[i] = malloc(width * sizeof(uint8_t));
        if (!matrix[i]) {
            fprintf(stderr, "ERROR: Could not allocate memory for pixel row\n");
            exit(1);
        }
    }
    
    return matrix;
}

// Free 2D pixel matrix
void free_pixel_matrix(uint8_t **matrix, uint32_t height) {
    for (uint32_t i = 0; i < height; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// PNG filter types
enum {
    FILTER_NONE = 0,
    FILTER_SUB = 1,
    FILTER_UP = 2,
    FILTER_AVG = 3,
    FILTER_PAETH = 4
};

uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    
    if (pa <= pb && pa <= pc) return a;
    else if (pb <= pc) return b;
    else return c;
}

// Apply PNG filters (reverse the filtering)
void unfilter_scanline(uint8_t *current, uint8_t *previous, 
                       uint32_t length, uint32_t bpp, 
                       uint8_t filter_type) {
    switch(filter_type) {
        case FILTER_NONE:
            break;
            
        case FILTER_SUB:
            for (uint32_t i = bpp; i < length; i++) {
                current[i] += current[i - bpp];
            }
            break;
            
        case FILTER_UP:
            if (previous) {
                for (uint32_t i = 0; i < length; i++) {
                    current[i] += previous[i];
                }
            }
            break;
            
        case FILTER_AVG:
            for (uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bpp) ? current[i - bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                current[i] += (left + up) / 2;
            }
            break;
            
        case FILTER_PAETH:
            for (uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bpp) ? current[i - bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                uint8_t up_left = (previous && i >= bpp) ? previous[i - bpp] : 0;
                current[i] += paeth_predictor(left, up, up_left);
            }
            break;
    }
}

// Process IDAT chunks and extract pixel data
png_image_t* process_idat_chunks(FILE *file, png_ihdr_t *ihdr, 
                                uint8_t *idat_data, uint64_t idat_size) {
    
    // Determine bytes per pixel based on color type
    uint32_t bpp = 1;  // bytes per pixel
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
        case 3: // Palette (not implemented in this example)
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
    
    // Decompress the IDAT data
    uint64_t expected_size = ihdr->height * (1 + ihdr->width * bpp);
    uint8_t *decompressed = malloc(expected_size);
    if (!decompressed) {
        fprintf(stderr, "ERROR: Could not allocate memory for decompression\n");
        return NULL;
    }
    
    int result = uncompress(decompressed, &expected_size, idat_data, idat_size);
    if (result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to decompress image data: %d\n", result);
        free(decompressed);
        return NULL;
    }
    
    // Create image structure
    png_image_t *image = malloc(sizeof(png_image_t));
    image->width = ihdr->width;
    image->height = ihdr->height;
    image->channels = channels;
    image->pixels = allocate_pixel_matrix(ihdr->height, ihdr->width * channels);
    
    // Process scanlines
    uint32_t scanline_length = ihdr->width * bpp;
    uint8_t *previous_scanline = NULL;
    
    for (uint32_t y = 0; y < ihdr->height; y++) {
        uint32_t offset = y * (1 + scanline_length);
        uint8_t filter_type = decompressed[offset];
        uint8_t *scanline = &decompressed[offset + 1];
        
        // Apply unfiltering
        unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);
        
        // Copy to pixel matrix
        memcpy(image->pixels[y], scanline, scanline_length);
        
        previous_scanline = scanline;
    }
    
    free(decompressed);
    return image;
}

// Convert RGB to grayscale if needed
uint8_t** rgb_to_grayscale(png_image_t *image) {
    if (image->channels == 1) {
        return image->pixels;  // Already grayscale
    }
    
    uint8_t **gray = allocate_pixel_matrix(image->height, image->width);
    
    for (uint32_t y = 0; y < image->height; y++) {
        for (uint32_t x = 0; x < image->width; x++) {
            if (image->channels >= 3) {
                // Convert RGB to grayscale using standard weights
                uint8_t r = image->pixels[y][x * image->channels + 0];
                uint8_t g = image->pixels[y][x * image->channels + 1];
                uint8_t b = image->pixels[y][x * image->channels + 2];
                gray[y][x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            } else if (image->channels == 2) {
                // Grayscale with alpha - just take the gray value
                gray[y][x] = image->pixels[y][x * 2];
            }
        }
    }
    
    return gray;
}

// Simple edge detection kernel (Sobel operator example)
void apply_sobel_edge_detection(uint8_t **input, uint8_t **output, 
                               uint32_t height, uint32_t width) {
    
    // Sobel X kernel
    int sobel_x[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    
    // Sobel Y kernel
    int sobel_y[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    // Gaussian Blur 
    int gaussian_blur[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };

    // Blur 
    float blur_kernel[3][3] = {
        {1/9.0, 1/9.0, 1/9.0},
        {1/9.0, 1/9.0, 1/9.0},
        {1/9.0, 1/9.0, 1/9.0}
    };

    // Laplacian edge detect
    int laplacian[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };

    // Sharpen
    int sharpen[3][3] = {
        {0, -1, 0},
        {-1, 5, -1},
        {0, -1, 0}
    };
    
    // Apply convolution
    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int gx = 0, gy = 0;
            
            // Convolve with kernels
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    uint8_t pixel = input[y + ky][x + kx];
                    gx += pixel * sharpen[ky + 1][kx + 1];
                    gy += pixel * sharpen[ky + 1][kx + 1];
                }
            }
            
            // Calculate magnitude
            int magnitude = (int)sqrt(gx * gx + gy * gy);
            output[y][x] = (magnitude > 255) ? 255 : magnitude;
        }
    }
    
    // Handle borders (set to 0)
    for (uint32_t i = 0; i < width; i++) {
        output[0][i] = 0;
        output[height - 1][i] = 0;
    }
    for (uint32_t i = 0; i < height; i++) {
        output[i][0] = 0;
        output[i][width - 1] = 0;
    }
}

// Save grayscale image as PNG
void save_grayscale_png(const char *filename, uint8_t **pixels, 
                       uint32_t width, uint32_t height) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "ERROR: Could not create output file %s\n", filename);
        exit(1);
    }
    
    // Write PNG signature
    write_bytes(file, png_sig, PNG_SIG_SIZE);
    
    // Prepare IHDR chunk
    uint8_t ihdr_data[13];
    uint32_t width_be = htonl(width);
    uint32_t height_be = htonl(height);
    memcpy(ihdr_data, &width_be, 4);
    memcpy(ihdr_data + 4, &height_be, 4);
    ihdr_data[8] = 8;   // bit depth
    ihdr_data[9] = 0;   // color type (grayscale)
    ihdr_data[10] = 0;  // compression method
    ihdr_data[11] = 0;  // filter method
    ihdr_data[12] = 0;  // interlace method
    
    write_chunk(file, "IHDR", ihdr_data, 13);
    
    // Prepare image data with filter bytes
    uint64_t raw_size = height * (1 + width);
    uint8_t *raw_data = malloc(raw_size);
    
    for (uint32_t y = 0; y < height; y++) {
        raw_data[y * (1 + width)] = 0;  // Filter type: None
        memcpy(raw_data + y * (1 + width) + 1, pixels[y], width);
    }
    
    // Compress the data
    uint64_t compressed_size = compressBound(raw_size);
    uint8_t *compressed_data = malloc(compressed_size);
    
    int result = compress2(compressed_data, &compressed_size, raw_data, raw_size, Z_DEFAULT_COMPRESSION);
    if (result != Z_OK) {
        fprintf(stderr, "ERROR: Failed to compress image data\n");
        free(raw_data);
        free(compressed_data);
        fclose(file);
        exit(1);
    }
    
    // Write IDAT chunk
    write_chunk(file, "IDAT", compressed_data, compressed_size);
    
    // Write IEND chunk
    write_chunk(file, "IEND", NULL, 0);
    
    free(raw_data);
    free(compressed_data);
    fclose(file);
    
    printf("Successfully saved edge-detected image to: %s\n", filename);
}

int main(int argc, char **argv) {
    char *exec_file = argv[0];
    if(3 != argc) {
        fprintf(stderr, "USAGE: %s <input_file.png> <output_file.png>\n", exec_file);
        exit(1);
    }
    
    char *input_file_name = argv[1];
    char *output_file_name = argv[2];
    
    FILE *input_file = fopen(input_file_name, "rb");
    if(!input_file) {
        fprintf(stderr, "Could not read the file %s\n", input_file_name);
        exit(1);
    }
    
    // Check PNG signature
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_file, signature, PNG_SIG_SIZE);
    
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", input_file_name);
        exit(1);
    }
    
    printf("Processing: %s\n", input_file_name);
    
    png_ihdr_t ihdr;
    uint8_t *idat_data = NULL;
    uint64_t idat_size = 0;
    uint64_t idat_capacity = 0;
    bool quit = false;
    
    while(!quit) {
        uint32_t chunk_size = read_chunk_size(input_file);
        uint8_t chunk_type[4];
        read_chunk_type(input_file, chunk_type);
        
        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            // Read IHDR chunk
            read_bytes(input_file, &ihdr.width, 4);
            read_bytes(input_file, &ihdr.height, 4);
            read_bytes(input_file, &ihdr.bit_depth, 1);
            read_bytes(input_file, &ihdr.color_type, 1);
            read_bytes(input_file, &ihdr.compression, 1);
            read_bytes(input_file, &ihdr.filter, 1);
            read_bytes(input_file, &ihdr.interlace, 1);
            
            ihdr.width = ntohl(ihdr.width);
            ihdr.height = ntohl(ihdr.height);
            
            printf("Image dimensions: %u x %u\n", ihdr.width, ihdr.height);
            printf("Bit depth: %u, Color type: %u\n", ihdr.bit_depth, ihdr.color_type);
            
        } else if(memcmp(chunk_type, "IDAT", 4) == 0) {
            // Accumulate IDAT chunks
            if(idat_capacity < idat_size + chunk_size) {
                idat_capacity = (idat_size + chunk_size) * 2;
                idat_data = realloc(idat_data, idat_capacity);
                if(!idat_data) {
                    fprintf(stderr, "ERROR: Could not allocate memory for IDAT\n");
                    exit(1);
                }
            }
            read_bytes(input_file, idat_data + idat_size, chunk_size);
            idat_size += chunk_size;
            
        } else if(memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        } else {
            // Skip other chunks
            // if(fseek(input_file, chunk_size, SEEK_CUR) < 0) {
            //     fprintf(stderr, "ERROR: could not skip: %s\n", strerror(errno));
            //     exit(1);
            // }
            printf("There is a hidden chunk!\n");
            uint8_t string[chunk_size];
            read_bytes(input_file, &string, sizeof(string));
            printf("The hidden data is %.*s\n", (int)sizeof(string), string);
        }
        
        // Read and skip CRC
        read_chunk_crc(input_file);
    }
    
    fclose(input_file);
    
    // Process the image data
    if(idat_data && idat_size > 0) {
        printf("\nProcessing image data...\n");
        png_image_t *image = process_idat_chunks(input_file, &ihdr, idat_data, idat_size);
        
        if(image) {
            // Convert to grayscale if needed
            uint8_t **grayscale = rgb_to_grayscale(image);
            
            // Create output matrix for edge detection
            uint8_t **edges = allocate_pixel_matrix(image->height, image->width);
            
            // Apply edge detection
            printf("Applying Sobel edge detection...\n");
            apply_sobel_edge_detection(grayscale, edges, image->height, image->width);
            
            // Save the edge-detected image
            save_grayscale_png(output_file_name, edges, image->width, image->height);
            
            // Clean up
            if(grayscale != image->pixels) {
                free_pixel_matrix(grayscale, image->height);
            }
            free_pixel_matrix(edges, image->height);
            free_pixel_matrix(image->pixels, image->height);
            free(image);
        }
        
        free(idat_data);
    }
    
    return 0;
}
