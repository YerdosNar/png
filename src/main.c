#include "png_io.h"

int main(int argc, char **argv) {
    uint8_t signature[PNG_SIG_SIZE];
    FILE *r_file = fopen(argv[1], "rb");
    read_bytes(r_file, signature, PNG_SIG_SIZE);
    for(int i = 0; i < PNG_SIG_SIZE; i++) {
        printf("%d\n", signature[i]);
    }
    uint32_t size = read_chunk_size(r_file);
    uint8_t type[4];
    read_chunk_type(r_file, type);
    uint32_t crc = read_chunk_crc(r_file);

    printf("CHUNK SIZE: %d\n", size);
    printf("CHUNK TYPE: %.*s\n", (int)(sizeof(type)), type);
    printf("CHUNK CRC : %d\n", crc);

    FILE *w_file = fopen(argv[2], "wb");
    write_bytes(w_file, signature, PNG_SIG_SIZE);
    
    return 0;
}
