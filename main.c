#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint64_t u64;

#define BUF_CAPACITY 8096

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
// from nob.h (https://github.com/tsoding/nob.h/blob/main/nob.h)
#define shift_args(xs, xs_sz) (assert((xs_sz) > 0), (xs_sz)--, *(xs)++)

typedef struct Node Node;

struct Node {
    size_t weight;
    unsigned char byte;
    Node* left;
    Node* right;
};

typedef struct {
    size_t n_bits;
    size_t data;
} BitNum;

typedef struct {
    const char* input_path;
    bool decompress;
    const char* output_path;
} Options;

typedef u64 FreqTable[256];
typedef BitNum ByteLookupTable[256];

#define GLOBAL_NODE_POOL_CAPACITY 8096
static Node global_node_pool[GLOBAL_NODE_POOL_CAPACITY];
static size_t global_node_pool_count = 0;

void encode_file(const char* input_path, const char* output_path);
void decode_file(const char* input_path, const char* output_path);
void encode(FILE* in_f, FILE* out_f, ByteLookupTable lookup_table);
void decode(FILE* in_f, FILE* out_f, Node* root);
Node* build_huffman_tree(FreqTable freqs);
Node* generate_huffman_tree(Node** nodes, size_t nodes_count);
void store_header_information(FILE* f, FreqTable freqs);
void read_header_information(FILE* f, FreqTable freqs);
void count_file_byte_freq(FILE* f, FreqTable freqs);
void count_byte_freq(const u8* data, size_t length, FreqTable freqs);
void populate_lookup_table(BitNum* byte_lookup_table, Node* root, BitNum bitnum);
void sort_nodes(Node** nodes, size_t length);
void swap_nodes(Node** nodes, size_t i, size_t j);
void freqs_to_nodes(FreqTable freqs, Node** nodes, size_t* nodes_count);
void print_nodes(Node** nodes, size_t length);
void print_tree(Node* root, int indent);
void print_lookup_table(ByteLookupTable* lookup_table);
void print_pad(int indent);
Node* store_node_in_pool(Node node);
void append_bit(BitNum* bitnum, char bit);
bool is_leaf(Node* node);
void print_bits(size_t num, size_t len);
void pexit(const char* msg);
void print_usage(FILE* f, const char* program_name);
void parse_args(Options* arg, int argc, char** argv);
const char* make_output_path(char* output_buf, size_t len, const char* input_path, char* suffix);

int main(int argc, char** argv) {
    Options opt = {0};
    parse_args(&opt, argc, argv);

    char output_buf[256] = {0};
    const char* output_path = opt.output_path
        ? opt.output_path
        : make_output_path(
            output_buf,
            sizeof(output_buf),
            opt.input_path,
            opt.decompress ? "dec" : "enc");

    if (opt.decompress) decode_file(opt.input_path, output_path);
    else encode_file(opt.input_path, output_path);

    return 0;
}

void encode_file(const char* input_path, const char* output_path) {
    FILE* in_f = fopen(input_path, "r");
    if (in_f == NULL) pexit("fopen failed");

    FILE* out_f = fopen(output_path, "w");
    if (out_f == NULL) pexit("fopen failed");

    FreqTable freqs = {0};
    count_file_byte_freq(in_f, freqs);
    Node* root = build_huffman_tree(freqs);

    store_header_information(out_f, freqs);

    ByteLookupTable lookup_table = {0};
    BitNum bitnum = {0};
    populate_lookup_table(lookup_table, root, bitnum);
    print_lookup_table(&lookup_table);

    rewind(in_f);
    encode(in_f, out_f, lookup_table);

    if (fclose(out_f) == EOF) pexit("fclose failed");
    if (fclose(in_f) == EOF) pexit("fclose failed");

    printf("Encoded `%s` into `%s`\n", input_path, output_path);
}

void decode_file(const char* input_path, const char* output_path) {
    FILE* in_f = fopen(input_path, "r");
    if (in_f == NULL) pexit("fopen failed");

    FILE* out_f = fopen(output_path, "w");
    if (out_f == NULL) pexit("fopen failed");

    FreqTable freqs = {0};
    read_header_information(in_f, freqs);

    Node* root = build_huffman_tree(freqs);
    decode(in_f, out_f, root);

    if (fclose(in_f) == EOF) pexit("fclose failed");
    if (fclose(out_f) == EOF) pexit("fclose failed");

    printf("Decoded `%s` into `%s`\n", input_path, output_path);
}

void encode(FILE* in_f, FILE* out_f, ByteLookupTable lookup_table) {
    bool done = false;
    char read_buf[BUF_CAPACITY];
    u8 write_buf[BUF_CAPACITY * 4];
    size_t cur_byte = 0;
    int cur_bit = 7;
    while (!done) {
        size_t n = fread(read_buf, 1, sizeof(read_buf), in_f);
        if (n < sizeof(read_buf)) {
            if (feof(in_f)) done = true;
            if (ferror(in_f)) pexit("fread failed");
        }
        for (size_t i = 0; i < n; ++i) {
            BitNum bitnum = lookup_table[(u8)read_buf[i]];
            for (int j = bitnum.n_bits - 1; j >= 0; --j) {
                int mask = 1 << j;
                if ((bitnum.data & mask) > 0) {
                    write_buf[cur_byte] += (1 << cur_bit);
                }
                cur_bit--;
                if (cur_bit < 0) {
                    assert(cur_byte < ARRAY_LEN(write_buf));
                    cur_byte += 1;
                    cur_bit = 7;
                }
            }
        }
        fwrite(write_buf, 1, cur_byte+1, out_f);
    }
}

void decode(FILE* in_f, FILE* out_f, Node* root) {
    u8 buf[BUF_CAPACITY] = {0};
    size_t n = fread(buf, 1, sizeof(buf), in_f);
    if (n < BUF_CAPACITY) {
        if (ferror(in_f)) pexit("fread failed");
        assert(feof(in_f));
    }

    size_t cur_byte = 0;
    int cur_bit = 7;
    for (size_t i = 0; i < root->weight; ++i) {
        Node* cur_node = root;
        while (true) {
            size_t mask = 1 << cur_bit;
            if ((buf[cur_byte] & mask) > 0) {
                assert(cur_node->right != NULL);
                cur_node = cur_node->right;
            } else {
                cur_node = cur_node->left;
            }
            cur_bit -= 1;
            if (cur_bit < 0) {
                cur_byte += 1;
                cur_bit = 7;
            }
            if (is_leaf(cur_node)) {
                fputc(cur_node->byte, out_f);
                break;
            }
        }
    }
}

Node* build_huffman_tree(FreqTable freqs) {
    Node* nodes[256] = {0};
    size_t nodes_count = 0;
    freqs_to_nodes(freqs, nodes, &nodes_count);

    return generate_huffman_tree(nodes, nodes_count);
}

Node* generate_huffman_tree(Node** nodes, size_t nodes_count) {
    sort_nodes(nodes, nodes_count);

    while (nodes_count > 1) {
        assert(nodes_count >= 2);

        Node root = {
            .weight = nodes[0]->weight + nodes[1]->weight,
            .byte = 0,
            .left = nodes[0],
            .right = nodes[1],
        };
        nodes[0] = store_node_in_pool(root);

        swap_nodes(nodes, 1, nodes_count-1);
        nodes_count -= 1;
        sort_nodes(nodes, nodes_count);
    }

    return nodes[0];
}

void store_header_information(FILE* f, FreqTable freqs) {
    size_t n = fwrite("HUF", 3, 1, f);
    if (n < 3) {
        if (ferror(f)) pexit("fwrite failed");
        assert(!feof(f));
    }

    u8 count = 0;
    for (size_t i = 0; i < 256; ++i) {
        if (freqs[i] == 0) continue;
        count += 1;
    }

    fwrite(&count, sizeof(count), 1, f);

    // TODO: calculate the maxlength (u8, u16, u32, u64) of freq and encoded that length
    // into the header

    for (size_t i = 0; i < 256; ++i) {
        if (freqs[i] == 0) continue;
        fwrite(&(u8){(u8)i}, sizeof(u8), 1, f);
        fwrite(&(u64){(u64)freqs[i]}, sizeof(u64), 1, f);
    }
}

void read_header_information(FILE* f, FreqTable freqs) {
    char magic[3] = {0};
    assert(fread(magic, 1, sizeof(magic), f) == 3);
    if (memcmp(magic, "HUF", 3) != 0) {
        fprintf(stderr, "missing MAGIC value\n");
        exit(EXIT_FAILURE);
    }

    // assert(fread(filesize, 1, sizeof(u64), f) == sizeof(u64));

    u8 char_count;
    assert(fread(&char_count, 1, sizeof(u8), f) == sizeof(u8));

    u8 byte;
    u64 count;
    for (size_t i = 0; i < (size_t)char_count; ++i) {
        assert(fread(&byte, 1, sizeof(u8), f) == sizeof(u8));
        assert(fread(&count, 1, sizeof(u64), f) == sizeof(u64));
        freqs[byte] = count;
    }
}

void count_file_byte_freq(FILE* f, FreqTable freqs) {
    char buf[BUF_CAPACITY];
    bool done = false;
    while (!done) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n < sizeof(buf)) {
            if (feof(f)) done = true;
            if (ferror(f)) pexit("fread failed");
        }
        count_byte_freq((const u8*) buf, n, freqs);
    }
}

void count_byte_freq(const u8* data, size_t length, FreqTable freqs) {
    for (size_t i = 0; i < length; ++i) {
        freqs[data[i]] += 1;
    }
}

void populate_lookup_table(BitNum* byte_lookup_table, Node* root, BitNum bitnum) {
    if (is_leaf(root)) {
        byte_lookup_table[root->byte] = bitnum;
        return;
    }

    BitNum left_bitnum = bitnum;
    append_bit(&left_bitnum, 0);
    populate_lookup_table(byte_lookup_table, root->left, left_bitnum);

    BitNum right_bitnum = bitnum;
    append_bit(&right_bitnum, 1);
    populate_lookup_table(byte_lookup_table, root->right, right_bitnum);
}

void sort_nodes(Node** nodes, size_t length) {
    for (size_t i = 0; i < length - 1; ++i) {
        for (size_t j = 0; j < length - i - 1; ++j) {
            if (nodes[j]->weight > nodes[j+1]->weight) {
                swap_nodes(nodes, j, j+1);
            }
        }
    }
}

void swap_nodes(Node** nodes, size_t i, size_t j) {
    Node* tmp = nodes[i];
    nodes[i] = nodes[j];
    nodes[j] = tmp;
}

void freqs_to_nodes(FreqTable freqs, Node** nodes, size_t* nodes_count) {
    *nodes_count = 0;
    for (size_t i = 0; i < 256; ++i) {
        if (freqs[i] == 0) continue;
        Node node = { .weight = freqs[i], .byte = i };
        nodes[(*nodes_count)++] = store_node_in_pool(node);
    }
}

void print_nodes(Node** nodes, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        printf("Weight(%c): %zu\n", nodes[i]->byte, nodes[i]->weight);
    }
}

void print_tree(Node* root, int indent) {
    print_pad(indent);
    printf("%zu", root->weight);
    if (isalnum(root->byte)) printf(" (%c)", root->byte);
    printf("\n");

    if (root->left) print_tree(root->left, indent+2);
    if (root->right) print_tree(root->right, indent+2);

}

void print_lookup_table(ByteLookupTable* lookup_table) {
    for (size_t i = 0; i < 256; ++i) {
        BitNum bitnum = (*lookup_table)[i];
        if (bitnum.n_bits == 0) continue;
        if (isalnum(i)) printf("%c => ", (char)i);
        else printf("%zu => ", i);
        print_bits(bitnum.data, bitnum.n_bits);
    }
}

void print_pad(int indent) {
    for (int i = 0; i < indent; ++i) {
        printf(" ");
    }
}

/*Store the node in the global node pool and return the index.*/
Node* store_node_in_pool(Node node) {
    assert(global_node_pool_count < GLOBAL_NODE_POOL_CAPACITY);
    global_node_pool[global_node_pool_count++] = node;
    return &global_node_pool[global_node_pool_count - 1];
}

void append_bit(BitNum* bitnum, char bit) {
    assert(bitnum->n_bits < sizeof(bitnum->data)*8);
    bitnum->n_bits += 1;
    bitnum->data <<= 1;
    bitnum->data += bit;
}

bool is_leaf(Node* node) {
    return node->left == NULL && node->right == NULL;
}

void print_bits(size_t num, size_t len) {
    if (len == 0) return;

    for (size_t i = 0; i < len; i++) {
        size_t bit_index = len - 1 - i;
        size_t bit = (num >> bit_index) & 1;
        putchar(bit ? '1' : '0');
    }
    putchar('\n');
}

void pexit(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_usage(FILE* f, const char* program_name) {
    fprintf(f, "Usage:\n");
    fprintf(f, "  %s [OPTIONS] <input_file>\n\n", program_name);

    fprintf(f, "Description:\n");
    fprintf(f, "  Compress or decompress a file using Huffman encoding.\n\n");

    fprintf(f, "Options:\n");
    fprintf(f, "  -d, --decompress        Decompress the input file\n");
    fprintf(f, "  -o, --output <file>     Write output to <file>\n");
    fprintf(f, "                          (default: stdout or inferred name)\n");
    fprintf(f, "\n");
}

void parse_args(Options* opt, int argc, char** argv) {
    char* program_name = shift_args(argv, argc);

    if (argc < 1) {
        print_usage(stderr, program_name);
        fprintf(stderr, "ERROR: no input file provided\n");
        exit(EXIT_FAILURE);
    }

    while (argc > 0) {
        char* arg = shift_args(argv, argc);
        if (strcmp(arg, "--decompress") == 0 || strcmp(arg, "-d") == 0) {
            opt->decompress = true;
        } else if (strcmp(arg, "--output") == 0 || strcmp(arg, "-o") == 0) {
            if (opt->output_path != NULL) {
                print_usage(stderr, program_name);
                fprintf(stderr, "ERROR: `--output` flag already provided: `%s`\n", opt->output_path);
                exit(EXIT_FAILURE);
            }
            if (argc < 1) {
                print_usage(stderr, program_name);
                fprintf(stderr, "ERROR: no output filename provided with `--output`\n");
                exit(EXIT_FAILURE);
            }
            opt->output_path = shift_args(argv, argc);
        } else {
            if (opt->input_path != NULL) {
                print_usage(stderr, program_name);
                fprintf(stderr, "ERROR: input_path `%s` already provided\n", opt->input_path);
                exit(EXIT_FAILURE);
            }
            opt->input_path = arg;
        }
    }

    if (opt->input_path == NULL) {
        print_usage(stderr, program_name);
        fprintf(stderr, "ERROR: no input_path provided\n");
        exit(EXIT_FAILURE);
    }
}

const char* make_output_path(char* output_buf, size_t len, const char* input_path, char* suffix) {
    int n = snprintf(output_buf, len, "%s.%s", input_path, suffix);
    if (n < 0 || n >= (int)len) {
        fprintf(stderr, "ERROR: default output_path would be truncated. Please provide a output_path using `--output` flag.\n");
        exit(EXIT_FAILURE);
    }
    return output_buf;
}
