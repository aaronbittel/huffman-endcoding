#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"

void encode_file(const char* input_path, const char* output_path) {
    FILE* in_f = fopen(input_path, "rb");
    if (in_f == NULL) pexit("fopen failed");

    FILE* out_f = fopen(output_path, "wb");
    if (out_f == NULL) pexit("fopen failed");

    FreqTable freqs = {0};
    count_file_byte_freq(in_f, freqs);
    Node* root = build_huffman_tree(freqs);

    write_header_information(out_f, freqs);

    ByteLookupTable lookup_table = {0};
    BitNum bitnum = {0};
    populate_lookup_table(lookup_table, root, bitnum);

    rewind(in_f);
    encode(in_f, out_f, lookup_table);

    if (fclose(out_f) == EOF) pexit("fclose failed");
    if (fclose(in_f) == EOF) pexit("fclose failed");

    printf("Encoded `%s` into `%s`\n", input_path, output_path);
}

void decode_file(const char* input_path, const char* output_path) {
    FILE* in_f = fopen(input_path, "rb");
    if (in_f == NULL) pexit("fopen failed");

    FILE* out_f = fopen(output_path, "wb");
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
    char read_buf[10] = {0};
    u8 write_buf[10] = {0};
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
                    write_buf[cur_byte] |= (1 << cur_bit);
                }
                cur_bit--;
                if (cur_bit < 0) {
                    cur_byte += 1;
                    cur_bit = 7;
                    if (cur_byte == sizeof(write_buf)) {
                        fwrite(write_buf, 1, cur_byte, out_f);
                        cur_byte = 0;
                        // reset write_buf because this loop only sets the one bits
                        memset(write_buf, 0, sizeof(write_buf));
                    }
                }
            }
        }
    }
    size_t bytes_to_write = cur_byte + (cur_bit != 7 ? 1 : 0);
    if (bytes_to_write > 0)
        fwrite(write_buf, 1, bytes_to_write, out_f);
}

void decode(FILE* in_f, FILE* out_f, Node* root) {
    u8 read_buf[10] = {0};
    char write_buf[10] = {0};
    size_t write_buf_count = 0;

    bool done = false;
    u64 symbols_decoded = 0;
    size_t total_symbols_in_file = root->weight;
    while (!done) {
        size_t n = fread(read_buf, 1, sizeof(read_buf), in_f);
        if (n < BUF_CAPACITY) {
            if (ferror(in_f)) pexit("fread failed");
        }

        size_t read_byte = 0;
        int read_bit = 7;
        Node* cur_node = root;
        while (read_byte < n) {
            size_t mask = 1 << read_bit;
            if ((read_buf[read_byte] & mask) > 0) {
                assert(cur_node->right != NULL);
                cur_node = cur_node->right;
            } else {
                cur_node = cur_node->left;
            }
            read_bit -= 1;
            if (read_bit < 0) {
                read_byte += 1;
                read_bit = 7;
            }
            if (is_leaf(cur_node)) {
                write_buf[write_buf_count++] = cur_node->byte;
                if (write_buf_count == ARRAY_LEN(write_buf)) {
                    fwrite(write_buf, 1, ARRAY_LEN(write_buf), out_f);
                    write_buf_count = 0;
                }
                cur_node = root;
                symbols_decoded += 1;
                if (symbols_decoded == total_symbols_in_file) {
                    done = true;
                    break;
                }
            }
        }
        if (n == 0 && feof(in_f)) done = true;
    }
    fwrite(write_buf, 1, write_buf_count, out_f);
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

void write_header_information(FILE* f, FreqTable freqs) {
    size_t n = fwrite("HUF", 3, 1, f);
    if (n < 3) {
        if (ferror(f)) pexit("fwrite failed");
        assert(!feof(f));
    }

    u16 count = 0;
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

    u16 char_count;
    assert(fread(&char_count, 1, sizeof(char_count), f) == sizeof(char_count));

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
