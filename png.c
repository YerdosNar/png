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

typedef enum {
    KERNEL_SOBEL_X = 0,
    KERNEL_SOBEL_Y = 1,
    KERNEL_SOBEL_COMBINED = 2,
    KERNEL_GAUSSIAN = 3,
    KERNEL_BLUR = 4,
    KERNEL_LAPLACIAN = 5,
    KERNEL_SHARPEN = 6,
    KERNEL_NONE = 7
} kernel_type;

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

void read_chunk_type(FILE *file, uint8_t chunk_type[]) {
    read_bytes(file, chunk_type, 4);
}

uint32_t read_chunk_crc(FILE *file) {
    uint32_t crc;
    read_bytes(file, &crc, 4);
    return ntohl(crc);
}

// CRC calculation for PNG
// copy pasted from:
// https://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html
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

    // We need to calculate CRC
    uint8_t *crc_buf = malloc(4 + length);
    memcpy(crc_buf, type, 4);
    if(length > 0) {
        memcpy(crc_buf + 4, data, length);
    }
    uint32_t crc_val = htonl(crc(crc_buf, 4+length));
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
        matrix[i] = malloc(width * sizeof(uint8_t));
        if(!matrix[i]) {
            fprintf(stderr, "ERROR: Could not allocate memory for pixel row\n");
            exit(1);
        }
    }

    return matrix;
}

// now free the matrix
void free_pixel_matrix(uint8_t **matrix, uint32_t height) {
    for(uint32_t i = 0; i < height; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// PNG filter after reading IHDR
enum {
    FILTER_NONE  = 0,
    FILTER_SUB   = 1,
    FILTER_UP    = 2,
    FILTER_AVG   = 3, 
    FILTER_PAETH = 4
};

uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = abs(p-a);
    int pb = abs(p-b);
    int pc = abs(p-c);

    if(pa <= pb && pa <= pc) {
        return a;
    } else if(pb <= pc) {
        return b;
    } else {
        return c;
    }
}

// PNG filters (reverse filtering)
void unfilter_scanline(uint8_t *current, uint8_t *previous,
                          uint32_t length, uint32_t bpp,
                          uint8_t filter_type) {
    switch(filter_type) {
        case FILTER_NONE:
            break;

        case FILTER_SUB:
            for(uint32_t i = bpp; i < length; i++) {
                current[i] += current[i-bpp];
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
            for(uint32_t i= 0; i < length; i++) {
                uint8_t left = (i >= bpp) ? current[i-bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                current[i] += (left+up) / 2;
            }
            break;

        case FILTER_PAETH:
            for(uint32_t i = 0; i < length; i++) {
                uint8_t left = (i >= bpp) ? current[i-bpp] : 0;
                uint8_t up = previous ? previous[i] : 0;
                uint8_t up_left = (previous && i >= bpp) ? previous[i-bpp] : 0;
                current[i] += paeth_predictor(left, up, up_left);
            }
            break;
    }
}

// IDAT chunks extraction
image_t *process_idat_chunks(ihdr_t *ihdr,
                                uint8_t *idat_data, uint64_t idat_size) {
    //Determine bytes per pixel based on color type
    uint32_t bpp = 1; //bytes per pixel
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

    // decompress IDAT 
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

        // apply unfiltering
        unfilter_scanline(scanline, previous_scanline, scanline_length, bpp, filter_type);

        //copy to pixel matrix
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
                // RBG to Grayscale using std weights
                uint8_t r = image->pixels[y][x * image->channels + 0];
                uint8_t g = image->pixels[y][x * image->channels + 1];
                uint8_t b = image->pixels[y][x * image->channels + 2];
                gray[y][x] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            } else if(image->channels == 2) {
                // grayscale with alpha ->
                // then we just take grayscale value only
                gray[y][x] = image->pixels[y][x * 2];
            }
        }
    }

    return gray;
}

// convolution with different kernels
// sobel     - edges detection
// laplacian - edge detection
// gaussian  - blurring
// blur      - blur
// sharpen   - sharpening
void apply_convolution(uint8_t **input, uint8_t **output,
                       uint32_t height, uint32_t width,
                       uint8_t kernel_type) {
    float kernels[7][3][3] = {
        { // Sobel x
            {-1, 0, 1},
            {-2, 0, 2},
            {-1, 0, 1}
        },
        { // Sobel y
            {-1, -2, -1},
            {0, 0, 0},
            {1, 2, 1}
        }, // Sobel combined
        {
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
        },
        { // Gaussian blur
            {1/16.0, 2/16.0, 1/16.0},
            {2/16.0, 4/16.0, 2/16.0},
            {1/16.0, 2/16.0, 1/16.0}
     },
        { // Blur
            {1/9.0, 1/9.0, 1/9.0},
            {1/9.0, 1/9.0, 1/9.0},
            {1/9.0, 1/9.0, 1/9.0}
        },
        { // Laplacian edge detection
            {0, -1, 0},
            {-1, 4, -1},
            {0, -1, 0}
        },
        { // Sharpen
            {0, -1, 0},
            {-1, 5, -1},
            {0, -1, 0}
        }
    };

    //apply convolution
    if(kernel_type == KERNEL_SOBEL_COMBINED) {
        for(uint32_t y = 1; y < height-1; y++) {
            for(uint32_t x = 1; x < width-1; x++) {
                int gx = 0, gy = 0;

                //convolute with kernels
                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y+ky][x+kx];
                        gx += pixel * kernels[KERNEL_SOBEL_X][ky+1][kx+1];
                        gy += pixel * kernels[KERNEL_SOBEL_Y][ky+1][kx+1];
                    }
                }

                //calculate magnitue 
                int magnitude = (int)sqrt(gx * gx + gy * gy);
                output[y][x] = (magnitude > 0xff) ? 0xff : magnitude;
            }
        }
    } else if(kernel_type < KERNEL_NONE) {
        for(uint32_t y = 1; y < height-1; y++) {
            for(uint32_t x = 1; x < width-1; x++) {
                float sum = 0.0f;

                for(int ky = -1; ky <= 1; ky++) {
                    for(int kx = -1; kx <= 1; kx++) {
                        uint8_t pixel = input[y + ky][x + kx];
                        sum += pixel * kernels[kernel_type][ky + 1][kx + 1];
                    }
                }

                //clamp to valid to range
                if(sum < 0) {
                    sum = 0;
                }
                if(sum > 255) {
                    sum = 255;
                }
                output[y][x] = (uint8_t)sum;
            }
        }
    } else {
        for(uint32_t y = 0; y < height; y++) {
            memcpy(output[y], input[y], width);
        }
        return;
    }
    
    // handle borders (set to 0)
    for(uint32_t i = 0; i < width; i++) {
        output[0][i] = input[0][i];
        output[height-1][i] = input[height-1][i];
    }
    for(uint32_t i = 0; i < height; i++) {
        output[i][0] = input[i][0];
        output[i][width-1] = input[i][width-1];
    }
}

//save img as PNG
void save_png(const char *filename, uint8_t **pixels,
                        uint32_t width, uint32_t height,
                        uint8_t color_type, uint32_t channels) {
    FILE *file = fopen(filename, "wb");
    if(!file) {
        fprintf(stderr, "ERROR: Could not create output file %s\n", filename);
        exit(1);
    }

    //write PNG signatur
    write_bytes(file, png_sig, PNG_SIG_SIZE);

    // IHDR
    uint8_t ihdr_data[13];
    uint32_t width_be = htonl(width),
             height_be = htonl(height);
    memcpy(ihdr_data, &width_be, sizeof(width_be));
    memcpy(ihdr_data+4, &height_be, sizeof(height_be));
    ihdr_data[8] = 8;
    ihdr_data[9] = color_type; //colortype: 0-grayscale, 2-RGB
    ihdr_data[10] = 0; //compression method
    ihdr_data[11] = 0; //filter method
    ihdr_data[12] = 0; //interlace method
    
    write_chunk(file, "IHDR", ihdr_data, sizeof(ihdr_data));

    // img data with filter bytes
    uint32_t bytes_per_pixel = (color_type == 0) ? 1 : channels;
    uint64_t raw_size = height * (1+width * bytes_per_pixel);
    uint8_t *raw_data = malloc(raw_size);

    for(uint32_t y = 0; y < height; y++) {
        raw_data[y * (1+width * bytes_per_pixel)] = 0; // filter type: None
        memcpy(raw_data + y * (1+width * bytes_per_pixel) + 1, pixels[y], width * bytes_per_pixel);
    }

    // compress before writing
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

    // write IDAT
    write_chunk(file, "IDAT", compressed_data, compressed_size);

    // write IEND 
    write_chunk(file, "IEND", NULL, 0);

    free(raw_data);
    free(compressed_data);
    fclose(file);

    printf("Successfully saved output image to: %s\n", filename);
}

void usage(char *exec_name) {
    printf("Usage: %s <input.png> -o <output.png> [options]\n", exec_name);
    printf("\nOptions: \n");
    printf("  -o, --output <file>    Output filename (default=out.png)\n");
    printf("  -i, --info <file>      Show information about PNG file\n");
    printf("  -g, --grayscale        Convert to grayscale\n");
    printf("  -c, --color            Keep RGB format (default)\n");
    printf("  --sobel-x              Apply Sobel X edge detection\n");
    printf("  --sobel-y              Apply Sobel Y edge detection\n");
    printf("  --sobel                Apply combined Sobel edge detection\n");
    printf("  --gaussian [steps]     Apply Gaussian blur (optional: number of iterations, default=1)\n");
    printf("  --blur [steps]         Apply box blur (optional: number of iterations, default=1)\n");
    printf("  --laplacian            Apply Laplacian edge detection\n");
    printf("  --sharpen              Apply sharpening filter\n");
    printf("  --none                 No filter (default)\n");
    printf("  -h, --help             Show this HELP message\n");
    printf("\nExamples:\n");
    printf("  %s input.png -o edges.png --sobel --grayscale\n", exec_name);
    printf("  %s photo.png -o blurred.png --gaussian\n", exec_name);

    printf("\n\n");
    printf("Author: YerdosNar github.com/YerdosNar/png.git\n");
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

            ihdr.width = ntohl(ihdr.width);
            ihdr.height = ntohl(ihdr.height);

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
            printf("  Interlace:   %u\n", ihdr.interlace);
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
            fseek(file, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(file);
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        usage(argv[0]);
        return 1;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    bool force_grayscale = false;
    bool conflict = false; 
    bool conflict_kernel = false;
    kernel_type kernel = KERNEL_NONE;
    uint8_t steps = 0;
    
    for(int i = 1; i < argc; i++) {
        if((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) && i == 1) {
            usage(argv[0]);
            return 0;
        } else if(!strcmp(argv[i], "-i") || !strcmp(argv[i], "--info")) {
            if(argc < 3) {
                fprintf(stderr, "ERROR: Invalid number of arguments for --info flag\n");
                return 1;
            }

            if(!input_file) {
                input_file = argv[2];
                if(strstr(input_file, ".png") == NULL) {
                    fprintf(stderr, "ERROR: Input file not provided for --info\n");
                    return 1;
                }
            }

            FILE *file = fopen(input_file, "rb");
            print_info(file);
            fclose(file);
            return 0;
        } else if(!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if(i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "ERROR: -o requires an argument\n");
                return 1;
            }
        } else if((!strcmp(argv[i], "-g") || !strcmp(argv[i], "--grayscale")) && !conflict) {
            if(!conflict) {
                force_grayscale = true;
                conflict = true;
            } else {
                fprintf(stderr, "ERROR: RGB and Grayscale both cannot be set\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--rgb")) {
            if(!conflict) {
                force_grayscale = false;
                conflict = true;
            } else {
                fprintf(stderr, "ERROR: RGB and Grayscale both cannot be set\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--sobel-x")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_X;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--sobel-y")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_Y;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--sobel")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_COMBINED;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--gaussian")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_GAUSSIAN;
                if(i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9') {
                    steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                }
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--blur")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_BLUR;
                // steps for blur
                if(i + 1 < argc && argv[i+1][0] >= '0' && argv[i+1][0] <= '9') {
                    steps = (uint8_t)(strtol(argv[++i], NULL, 10));
                }
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--laplacian")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_LAPLACIAN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--sharpen")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SHARPEN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(!strcmp(argv[i], "--none")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_NONE;
            } else {
                fprintf(stderr, "ERROR: Two or more kernels chosen\n");
                return 1;
            }
        } else if(strstr(argv[i], ".png") != NULL && input_file == NULL) {
            input_file = argv[i];
        }
    }

    // validate args 
    if(!input_file) {
        fprintf(stderr, "ERROR: No input file specified\n");
        usage(argv[0]);
        return 1;
    }
    if(!output_file) {
        printf("No output filename was set\n");
        printf("Default: out.png\n");
        output_file = "out.png";
    }

    // step for blur default 1
    if(steps == 0 && kernel != KERNEL_NONE) {
        steps = 1;
    }

    // edge detection work better on gray
    if((kernel == KERNEL_SOBEL_X || kernel == KERNEL_SOBEL_Y || 
       kernel == KERNEL_SOBEL_COMBINED || kernel == KERNEL_LAPLACIAN) && !force_grayscale) {
        printf("Note: Edge detection typically works better on grayscale images.\n");
        printf("Consider adding --grayscale flag.\n\n");
    }

    FILE*input_fp = fopen(input_file, "rb");
    if(!input_fp) {
        fprintf(stderr, "ERROR: Could not open input file %s\n", input_file);
        return 1;
    }

    // Is it PNG
    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_fp, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", input_file);
        fclose(input_fp);
        return 1;
    }

    printf("Processing: %s\n", input_file);
    printf("Kernel: ");
    switch(kernel) {
        case KERNEL_SOBEL_X: printf("Sobel X\n"); break;
        case KERNEL_SOBEL_Y: printf("Sobel Y\n"); break;
        case KERNEL_SOBEL_COMBINED: printf("Sobel Combined\n"); break;
        case KERNEL_GAUSSIAN: printf("Gaussian\n"); break;
        case KERNEL_BLUR: printf("Blur"); 
            if(steps > 1) printf(" (%d steps)", steps);
            printf("\n");
            break;
        case KERNEL_LAPLACIAN: printf("Laplacian\n"); break;
        case KERNEL_SHARPEN: printf("Sharpen\n"); break;
        case KERNEL_NONE: printf("None\n"); break;
    }
    printf("Output format: %s\n\n", force_grayscale ? "Grayscale" : "RGB");

    ihdr_t ihdr;
    uint8_t *idat_data = NULL;
    uint64_t idat_size = 0;
    uint64_t idat_capacity = 0;
    bool quit = false;

    while(!quit) {
        uint32_t chunk_size = read_chunk_size(input_fp);
        uint8_t chunk_type[4];
        read_chunk_type(input_fp, chunk_type);

        if(memcmp(chunk_type, "IHDR", 4) == 0) {
            read_bytes(input_fp, &ihdr.width, 4);
            read_bytes(input_fp, &ihdr.height, 4);
            read_bytes(input_fp, &ihdr.bit_depth, 1);
            read_bytes(input_fp, &ihdr.color_type, 1);
            read_bytes(input_fp, &ihdr.compression, 1);
            read_bytes(input_fp, &ihdr.filter, 1);
            read_bytes(input_fp, &ihdr.interlace, 1);

            ihdr.width = htonl(ihdr.width);
            ihdr.height = htonl(ihdr.height);

            printf("Image dimensions: %u x %u\n", ihdr.width, ihdr.height);
            printf("Bit depth: %u, Color type: %u\n", ihdr.bit_depth, ihdr.color_type);
        } else if(memcmp(chunk_type, "IDAT", 4) == 0) {
            if(idat_capacity < idat_size + chunk_size) {
                idat_capacity = (idat_size + chunk_size) * 2;
                idat_data = realloc(idat_data, idat_capacity);
                if (!idat_data) {
                    fprintf(stderr, "ERROR: Could not reallocate memory for IDAT\n");
                    exit(1);
                }
            }
            read_bytes(input_fp, idat_data + idat_size, chunk_size);
            idat_size += chunk_size;
        } else if(memcmp(chunk_type, "IEND", 4) == 0) {
            quit = true;
        } else {
            fseek(input_fp, chunk_size, SEEK_CUR);
        }

        read_chunk_crc(input_fp);
    }
    fclose(input_fp);

    //now we need to create a new image
    if(idat_data && idat_size > 0) {
        printf("\nProcessing image data...\n");
        image_t *image = process_idat_chunks(&ihdr, idat_data, idat_size);

        if(image) {
            if(force_grayscale) {
                // grayscale
                uint8_t **grayscale = rgb_to_grayscale(image);
                uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
                uint8_t **temp = NULL;

                if(steps > 1) {
                    temp = allocate_pixel_matrix(image->height, image->width);
                }


                // convolution apply
                printf("Applying filter...\n");
                if(steps > 1) printf(" (%d steps)", steps);
                printf("...\n");

                uint8_t **input = grayscale;
                uint8_t **output = processed;

                for(uint8_t i = 0; i < steps; i++) {
                    apply_convolution(input, output, image->height, image->width, kernel);
                    if(i < steps - 1) {
                        uint8_t **swap = input;
                        input = output;
                        output = (swap == grayscale && temp) ? temp : grayscale;
                    }
                }

                // save img
                save_png(output_file, output, image->width, image->height, 0, 1);

                //cleaning
                if(grayscale != image->pixels) {
                    free_pixel_matrix(grayscale, image->height);
                }
                free_pixel_matrix(processed, image->height);
                if(temp) {
                    free_pixel_matrix(temp, image->height);
                }
            }
            else if(kernel != KERNEL_NONE){
                // for RGB
                if(image->channels >= 3) {
                    uint8_t **processed = allocate_pixel_matrix(image->height, image->width * image->channels);

                    printf("Applying filter");
                    if(steps > 1) printf(" (%d steps)", steps);
                    printf("...\n");

                    //kernel to each channel
                    for(uint32_t ch = 0; ch < 3; ch++) {
                        uint8_t **channel = allocate_pixel_matrix(image->height, image->width);
                        uint8_t **proc_channel = allocate_pixel_matrix(image->height, image->width);
                        uint8_t **temp_channel = NULL;

                        if(steps > 1) {
                            temp_channel = allocate_pixel_matrix(image->height, image->width);
                        }

                        // extract channel
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                channel[y][x] = image->pixels[y][x * image->channels + ch];
                            }
                        }

                        //apply kernel
                        uint8_t **input = channel;
                        uint8_t **output = proc_channel;

                        for(uint8_t i = 0; i < steps; i++) {
                            apply_convolution(input, output, image->height, image->width, kernel);

                            //swap
                            if(i < steps - 1) {
                                uint8_t **swap = input;
                                input = output;
                                output = (swap == channel && temp_channel) ? temp_channel : channel;
                            }
                        }

                        //copy 
                        for(uint32_t y = 0; y < image->height; y++) {
                            for(uint32_t x = 0; x < image->width; x++) {
                                processed[y][x * image->channels + ch] = output[y][x];
                                // copy alpha
                                if(image->channels == 4 && ch == 0) {
                                    processed[y][x * image->channels + 3] = image->pixels[y][x * image->channels + 3];
                                }
                            }
                        }
                        free_pixel_matrix(channel, image->height);
                        free_pixel_matrix(proc_channel, image->height);
                        if(temp_channel) {
                            free_pixel_matrix(temp_channel, image->height);
                        }
                    }

                    //save rgb
                    uint8_t color_type = (image->channels == 4) ? 6 : 2;
                    save_png(output_file, processed, image->width, image->height, color_type, image->channels);
                    free_pixel_matrix(processed, image->height);
                }
                else {
                    //if it is gray
                    uint8_t **processed = allocate_pixel_matrix(image->height, image->width);
                    uint8_t **temp = NULL;

                    if(steps > 1) {
                        temp = allocate_pixel_matrix(image->height, image->width);
                    }

                    uint8_t **input = image->pixels;
                    uint8_t **output = processed;

                    for(uint8_t i = 0; i < steps; i++) {
                        apply_convolution(input, output, image->height, image->width, kernel);

                        if(i < steps - 1) {
                            uint8_t **swap = input;
                            input = output;
                            output = (swap == image->pixels && temp) ? temp : image->pixels;
                        }
                    }

                    save_png(output_file, processed, image->width, image->height, 0, 1);
                    free_pixel_matrix(processed, image->height);
                    if(temp) {
                        free_pixel_matrix(temp, image->height);
                    }
                }
            }
            else {
                if(image->channels >= 3) {
                    //save rgb/rgba img
                    uint8_t color_type = (image->channels == 4) ? 6 : 2;
                    save_png(output_file, image->pixels, image->width, image->height, color_type, image->channels);
                }
                else {
                    //save grayscale img
                    save_png(output_file, image->pixels, image->width, image->height, 0, 1);
                }
            }

            free_pixel_matrix(image->pixels, image->height);
            free(image);
        }

        free(idat_data);
    }
    printf("\nDone!\n");
    return 0;
}
