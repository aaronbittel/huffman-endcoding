#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;

#define BUF_CAPACITY 8096

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct Node Node;

struct Node {
    size_t weight;
    unsigned char byte;
    Node* left;
    Node* right;
};

#define GLOBAL_NODE_POOL_CAPACITY 8096
static Node global_node_pool[GLOBAL_NODE_POOL_CAPACITY];
static size_t global_node_pool_count = 0;

void count_file_byte_freq(FILE* f, size_t freq[256]);
void count_byte_freq(const u8* data, size_t length, size_t freq[256]);
Node* generate_huffman_tree(Node** nodes, size_t nodes_count);
void sort_nodes(Node** nodes, size_t length);
void swap_nodes(Node** nodes, size_t i, size_t j);
void freqs_to_nodes(size_t freq[256], Node** nodes, size_t* nodes_count);
void print_nodes(Node** nodes, size_t length);
void print_tree(Node* root, int indent);
void print_pad(int indent);
Node* store_node_in_pool(Node node);

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];
    FILE* f = fopen(filename, "r");
    if (f == NULL) perror("fopen failed");

    size_t freqs[256] = {0};

    count_file_byte_freq(f, freqs);
    for (size_t i = 0; i < 256; ++i) {
        if (freqs[i] == 0) continue;
        printf("%c => %zu\n", (char)i, freqs[i]);
    }

    Node* nodes[256] = {0};
    size_t nodes_count = 0;
    freqs_to_nodes(freqs, nodes, &nodes_count);

    Node* root = generate_huffman_tree(nodes, nodes_count);
    print_tree(root, 0);

    if (fclose(f) == EOF) perror("close failed");
    return 0;
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

void count_file_byte_freq(FILE* f, size_t freqs[256]) {
    bool done = false;

    while (!done) {
        char buf[BUF_CAPACITY];
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n < sizeof(buf)) {
            if (feof(f) != 0) done = true;
            if (ferror(f) != 0) perror("fread failed");
        }
        count_byte_freq((const u8*) buf, n, freqs);
    }
}

void count_byte_freq(const u8* data, size_t length, size_t freq[256]) {
    for (size_t i = 0; i < length; ++i) {
        freq[data[i]] += 1;
    }
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

void freqs_to_nodes(size_t freqs[256], Node** nodes, size_t* nodes_count) {
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
