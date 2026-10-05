#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "ir_lowering.h"
#include "bpf_builder.h"
#include "ccbpf.h"
#include "heap.h"

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static const char *phase;
static unsigned values[2][16], counts[2], migrations[2];
static unsigned run_index;
static unsigned marker_counts[2][4];

static uint32_t print_number(struct ccbpf_program *p, uint32_t a0,
                             uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)p; (void)a1; (void)a2; (void)a3;
    REQUIRE(counts[run_index] < 16);
    values[run_index][counts[run_index]++] = a0;
    printf("%s: %u\n", phase, a0);
    return 0;
}

static uint32_t print_string(struct ccbpf_program *p, uint32_t a0,
                             uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)a1; (void)a2; (void)a3;
    REQUIRE(a0 < (uint32_t)p->string_count);
    const char *s = p->strings[a0];
    const char *markers[] = {"runing....", "migration_start", "migration_end", "ok!!!"};
    for (unsigned i = 0; i < 4; ++i)
        if (strcmp(s, markers[i]) == 0) marker_counts[run_index][i]++;
    printf("%s: %s\n", phase, s);
    return 0;
}

static uint8_t *compile_image(const char *path, size_t *length)
{
    FILE *f = fopen(path, "rb"); REQUIRE(f);
    REQUIRE(fseek(f, 0, SEEK_END) == 0);
    long n = ftell(f); REQUIRE(n >= 0); rewind(f);
    char *source = malloc((size_t)n + 1); REQUIRE(source);
    REQUIRE(fread(source, 1, (size_t)n, f) == (size_t)n);
    fclose(f); source[n] = 0;
    compiler_init(16, 30 * 1024, 1024, 15 * 1024);
    struct lexer lex; lexer_init(&lex);
    lexer_set_input_buffer(source, (size_t)n);
    struct Parser *parser = parser_new(&lex);
    native_decl_register("print", 3, 1);
    native_decl_register("print_str", 4, 1);
    native_decl_register("migrate", 8, 0);
    parser_program(parser); frontend_destroy(&lex);
    struct bpf_builder b; bpf_builder_init(&b, 24 * 1024);
    struct ir_mes im; ir_mes_get(&im);
    ir_lower_program(im.ir_head, im.label_count, &b); ir_free();
    uint8_t *borrowed = ccbpf_pack_memory(bpf_builder_data(&b),
        7 * 1024, (size_t)bpf_builder_count(&b), length);
    REQUIRE(borrowed);
    uint8_t *owned = malloc(*length); REQUIRE(owned);
    memcpy(owned, borrowed, *length);
    bpf_builder_free(&b); free(source);
    return owned;
}

static enum ccbpf_status until_stop(struct ccbpf_ctx *state,
                                    struct ccbpf_program *prog)
{
    for (unsigned batches = 0; batches < 10000; ++batches) {
        enum ccbpf_status s = ccbpf_vm_step(state, prog, NULL, 0, 0, 64);
        if (s != CCBPF_OK) return s;
    }
    fprintf(stderr, "execution budget exceeded\n"); exit(1);
}

int main(int argc, char **argv)
{
    REQUIRE(argc == 2);
    size_t image_len; uint8_t *image = compile_image(argv[1], &image_len);
    ccbpf_system_init();
    native_register(3, 1, print_number);
    native_register(4, 1, print_string);
    native_register(8, 0, native_migrate);
    struct ccbpf_program *baseline_prog = ccbpf_load_from_memory(image, image_len);
    REQUIRE(baseline_prog);
    struct ccbpf_ctx baseline = {0};
    phase = "baseline"; run_index = 0;
    enum ccbpf_status status;
    do {
        status = until_stop(&baseline, baseline_prog);
        if (status == CCBPF_MIGRATE) migrations[0]++;
    } while (status == CCBPF_MIGRATE);
    REQUIRE(status == CCBPF_FINISHED);
    ccbpf_unload(baseline_prog);

    struct ccbpf_program *source_prog = ccbpf_load_from_memory(image, image_len);
    REQUIRE(source_prog);
    struct ccbpf_ctx source = {0};
    phase = "source"; run_index = 1;
    REQUIRE(until_stop(&source, source_prog) == CCBPF_MIGRATE);
    migrations[1]++;
    REQUIRE(counts[1] == 5);
    REQUIRE(source.pc > 0 && source.pc < source_prog->insn_count);
    REQUIRE(source_prog->insns[source.pc - 1].code == (BPF_MISC | BPF_COP));
    REQUIRE(source_prog->insns[source.pc - 1].k == 8);
    uint32_t local_a;
    memcpy(&local_a, source.mem + sizeof(void *), sizeof(local_a));
    REQUIRE(local_a == 5);
    printf("CHECKPOINT pc=%u A=%u X=%u context=%zu image=%zu insn_size=%zu instructions=%zu\n",
        source.pc, source.A, source.X, sizeof(source), image_len,
        sizeof(struct bpf_insn), source_prog->insn_count);
    uint8_t *snapshot = NULL; size_t snapshot_len = 0;
    REQUIRE(ccbpf_ctx_pack(&source, &snapshot, &snapshot_len) == 0);
    REQUIRE(snapshot_len == sizeof(source));
    struct ccbpf_ctx saved = source;
    ccbpf_unload(source_prog);
    memset(&source, 0xa5, sizeof(source));
    struct ccbpf_program *destination_prog = ccbpf_load_from_memory(image, image_len);
    REQUIRE(destination_prog);
    struct ccbpf_ctx destination = {0};
    REQUIRE(ccbpf_ctx_unpack(&destination, snapshot, snapshot_len - 1) == -1);
    REQUIRE(destination.pc == 0 && destination.A == 0);
    REQUIRE(ccbpf_ctx_unpack(&destination, snapshot, snapshot_len) == 0);
    REQUIRE(memcmp(&destination, &saved, sizeof(saved)) == 0);
    heap_free(snapshot); free(image);
    phase = "destination";
    REQUIRE(until_stop(&destination, destination_prog) == CCBPF_FINISHED);
    REQUIRE(memcmp(&destination, &baseline, sizeof(baseline)) == 0);
    REQUIRE(baseline.ret == 0 && counts[0] == 11 && counts[1] == 11);
    REQUIRE(migrations[0] == 1 && migrations[1] == 1);
    for (unsigned i = 0; i < 11; ++i) {
        REQUIRE(values[0][i] == i + 1);
        REQUIRE(values[1][i] == values[0][i]);
    }
    for (unsigned i = 0; i < 4; ++i)
        REQUIRE(marker_counts[0][i] == 1 && marker_counts[1][i] == 1);
    printf("PASS: restored state equals checkpoint; final state equals baseline; output=1..11; ret=0\n");
    ccbpf_unload(destination_prog);
    return 0;
}
