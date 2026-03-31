#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

#define BUF_CAPACITY 8096

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

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
void write_header_information(FILE* f, FreqTable freqs);
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
