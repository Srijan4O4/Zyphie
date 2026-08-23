/*
 * Zyphie CLI entry point.
 *
 * Phase 0 stub: just enough to link, so the build can be verified end to end
 * before any codec code exists. The real filter-mode CLI -- stdin/stdout
 * plumbing, -c/-d, Windows binary-mode setup, distinct exit codes for the
 * worker to branch on -- is Phase 3. The container format it implements is
 * specified in docs/format-spec.md.
 */

#include <stdio.h>
#include <string.h>

#define ZY_VERSION "0.0.0-dev"

static void usage(FILE *out)
{
    fprintf(out,
            "zyphie %s - Huffman compressor\n"
            "\n"
            "usage: zyphie -c | -d [--input FILE] [--output FILE]\n"
            "       with no file arguments, reads stdin and writes stdout\n"
            "\n"
            "The codec is not implemented yet: engine lands in Phase 1,\n"
            "the full CLI in Phase 3.\n",
            ZY_VERSION);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("zyphie %s\n", ZY_VERSION);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }

    usage(stderr);
    return 2;
}
