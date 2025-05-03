#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>



static void hash_update(uint8_t *data, size_t len, uint32_t *hash)
{
    static const uint32_t PRIME = 1000000007;

    for (size_t i = 0; i < len; i++) {
        *hash = (*hash * 31 + data[i]) % PRIME;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s\n", argv[1]);
        return 1;
    }

    uint8_t buf[4096];
    size_t len;
    uint32_t hash = 0;
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        hash_update(buf, len, &hash);
    }

    printf("%" PRIu32 "\n", hash);

    return 0;
}
