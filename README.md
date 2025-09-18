# PNG Image Processor

A lightweight C program for reading, processing, and filtering PNG images with support for various convolution kernels and color space conversions.

## Features

- **PNG Format Support**: Read and write PNG files with full chunk parsing
- **Multiple Filters**: Apply various image processing filters including:
  - Sobel edge detection (X, Y, and combined)
  - Gaussian blur
  - Box blur
  - Laplacian edge detection
  - Image sharpening
  - Image upscaling
- **Color Space Conversion**: Convert between RGB and grayscale
- **Preserves Alpha Channel**: Maintains transparency information when processing RGBA images

- **Find hidden chunks**: Finds not supported chunks other than IHDR, IDAT, IEND
- **Write custom chunks**: Write your own custom hidden chunks

## Requirements

- C compiler (gcc/clang)
- zlib library for PNG compression/decompression
- Standard C libraries (math, stdio, stdlib)

## Building

```bash
git clone https://github.com/YerdosNar/PNG.git
cd PNG
make
```

## Usage

```bash
./png <input.png> -o <output.png> [options]
```

### Options

- `-i, --info` - Show information about the PNG file
- `-o, --output <file>` - Output filename (default: out.png)
- `-g, --grayscale` - Convert to grayscale
- `-c, --color` - Keep RGB format (default)
- `-x, --sobel-x` - Apply Sobel X edge detection
- `-y, --sobel-y` - Apply Sobel Y edge detection
- `-s, --sobel` - Apply combined Sobel edge detection
- `--gaussian [steps]` - Apply Gaussian blur
- `-b, --blur [steps]` - Apply box blur
- `-l, --laplacian` - Apply Laplacian edge detection
- `-sh,--sharpen` - Apply sharpening filter
- `-u,--upscale [scale_factor]` - Apply sharpening filter (Bilinear)
- `--none` - No filter (default)
- `-h, --help` - Show help message

### Examples

Edge detection with grayscale conversion:
```bash
./png photo.png -o edges.png --sobel --grayscale
```

Apply Gaussian blur:
```bash
./png image.png -o blurred.png --gaussian
```

Sharpen an image:
```bash
./png portrait.png -o sharp.png --sharpen
```

## Supported Image Types

- ✅ Grayscale (8-bit)
- ✅ RGB (24-bit)
- ✅ RGBA (32-bit with alpha)
- ✅ Grayscale + Alpha
- ✅ Indexed/Palette images
- ❌ Interlaced PNGs
- ❌ 16-bit depth images

## Technical Details

The program implements:
- Full PNG chunk parsing (IHDR, IDAT, IEND)
- All 5 PNG filter types (None, Sub, Up, Average, Paeth)
- 3x3 convolution kernels for image filtering
- Per-channel processing for color images
- Proper PNG CRC calculation and validation

## Known Issues

- Currently only supports 8-bit depth images
- Interlaced PNG files are not supported
- Border pixels use simple replication for convolution


## Author

YerdosNar 2025.09.18
