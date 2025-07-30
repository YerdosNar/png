#include "png_io.h"
#include "utils.h"
#include "image.h"

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

int main(int argc, char **argv) {
    
    return 0;
}
