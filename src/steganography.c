#include "../include/steganography.h"
#include "../include/png_io.h"

bool isPNG(FILE *file) {
    uint8_t first_8_bytes[8];
    read_bytes(file, &first_8_bytes, 8);
    if (memcmp(first_8_bytes, png_sig, 8) != 0) {
      fprintf(stderr, "ERROR: Not a valid PNG file\n");
      return false;
    }
    return true;
}

bool detect(FILE *file) {
    if(!isPNG(file)) {
        return false;
    }

    bool quit = false;
    while(!quit) {
        uint32_t size;
        uint8_t type[4];
        read_chunk_size(file);
        read_chunk_type(file, type);
        uint8_t main_chunks[4][4] = { "IHDR", "PLTE", "IDAT", "IEND" };
        bool found_main = false;
        for(uint8_t i = 0; i < 4; i++) {
            if(memcmp(type, main_chunks[i], 4) == 0) {
                found_main = true;
            }
        }

        uint8_t additional_chunks[13][4] = { "bKGD", "cHRM", "gAMA", "hIST", "iCCP", "pHYs", "sBIT", "sPLT", "sRGB", "tEXt", "tIME", "tRNS", "zTXt" };
        bool additional_found = false;
        if(!found_main) {
            for(uint8_t i = 0; i < 13; i++) {
                if(memcmp(type, additional_chunks[i], 4) == 0) {
                    additional_found = true;
                }
            }
        }
        if(additional_found)
    }
}

int main() {
    FILE *file = fopen("test.png", "rb");
    detected(file);
    return 0;
}
