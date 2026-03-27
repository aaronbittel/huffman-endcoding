#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define u8 uint8_t

#define BUF_CAPACITY 8096

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

void count_file_byte_freq(FILE* f, size_t freq[256]);
void count_byte_freq(const u8* data, size_t length, size_t freq[256]);

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];
    FILE* f = fopen(filename, "r");
    if (f == NULL) perror("fopen failed");

    size_t freq[256] = {0};

    count_file_byte_freq(f, freq);

    printf("Count(X) = %zu\n", freq['X']);
    printf("Count(t) = %zu\n", freq['t']);

    if (fclose(f) == EOF) perror("close failed");
}

void count_file_byte_freq(FILE* f, size_t freq[256]) {
    bool done = false;

    while (!done) {
        char buf[BUF_CAPACITY];
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n < sizeof(buf)) {
            if (feof(f) != 0) done = true;
            if (ferror(f) != 0) perror("fread failed");
        }
        count_byte_freq((const u8*) buf, n, freq);
    }
}

void count_byte_freq(const u8* data, size_t length, size_t freq[256]) {
    for (size_t i = 0; i < length; ++i) {
        freq[data[i]] += 1;
    }
}
