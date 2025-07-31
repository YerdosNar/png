#include <stdbool.h>
#include <string.h>
#include "../include/png_io.h"
#include "../include/image.h"

void usage(char *exec_name) {
    printf("Usage: %s <input.png> -o <output.png> [options]\n", exec_name);
    printf("\nOptions: \n");
    printf("  -o, --output <file>    Output filename (default=out.png)\n");
    printf("  -i, --info <file>      Show information about PNG file\n");
    printf("  -g, --gray             Convert to grayscale\n");
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

int main(int argc, char **argv) {
    if(argc < 2) {
        usage(argv[0]);
        return 1;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    bool force_grayscale = false;
    bool conflict_color = false;
    bool conflict_kernel = false;
    kernel_type kernel = KERNEL_NONE;
    uint16_t steps = 0;

    for(int i = 1; i < argc; i++) {
        if((!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) && i == 1) {
            usage(argv[0]);
            return 0;
        }
        else if(!strcmp(argv[i], "-i") || !strcmp(argv[i], "--info")) {
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
        }
        else if(!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if(i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "ERROR: -o requires an argument after it\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "-g") || !strcmp(argv[i], "--gray")) {
            if(!conflict_color) {
                force_grayscale = true;
                conflict_color = true;
            } else {
                fprintf(stderr, "ERROR: RGB and GRAYSCALE both cannot be set\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--rgb")) {
            if(!conflict_color) {
                force_grayscale = false;
                conflict_color = true;
            } else {
                fprintf(stderr, "ERROR: RGB and GRAYSCALE both cannot be set\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sobel-x")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_X;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sobel-y")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_Y;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sobel")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SOBEL_COMBINED;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--gaussian")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_GAUSSIAN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--blur")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_BLUR;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--laplacian")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_LAPLACIAN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--sharpen")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_SHARPEN;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(!strcmp(argv[i], "--none")) {
            if(!conflict_kernel) {
                conflict_kernel = true;
                kernel = KERNEL_NONE;
            } else {
                fprintf(stderr, "ERROR: Two or more kernel chosen\n");
                return 1;
            }
        }
        else if(strstr(argv[i], ".png") != NULL && input_file == NULL) {
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
        printf("Dfault: out.png");
        output_file = "out.png";
    }

    if(steps == 0 && kernel != KERNEL_NONE) steps = 1;

    // warn user
    if((kernel == KERNEL_SOBEL_COMBINED || kernel == KERNEL_SOBEL_X || kernel == KERNEL_SOBEL_Y || kernel == KERNEL_LAPLACIAN) && !force_grayscale) {
        printf("Node: Edge detection typically works better on grayscale image.\n");
        printf("Consider adding --gray flag.\n\n");
    }

    FILE *input_f = fopen(input_file, "rb");
    if(!input_f) {
        fprintf(stderr, "ERROR: Could not open input file %s\n", input_file);
        return 1;
    }

    uint8_t signature[PNG_SIG_SIZE];
    read_bytes(input_f, signature, PNG_SIG_SIZE);
    if(memcmp(signature, png_signature, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s is not a PNG file\n", input_file);
        printf("SIGNATURE: ");
        print_bytes(signature, PNG_SIG_SIZE);
        fclose(input_f);
        return 1;
    }

    return 0;
}
