#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"

// from nob.h (https://github.com/tsoding/nob.h/blob/main/nob.h)
#define shift_args(xs, xs_sz) (assert((xs_sz) > 0), (xs_sz)--, *(xs)++)

typedef struct {
    const char* input_path;
    bool decompress;
    const char* output_path;
} Options;

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
