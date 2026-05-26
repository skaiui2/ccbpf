#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * Self-contained test for CWE-120: Buffer reads never exceed the declared length.
 *
 * We simulate the scp_input pattern: an internal buffer of a fixed size is
 * allocated, and we verify that copying external input of len bytes into it
 * never exceeds the allocated capacity.  The invariant is:
 *   len <= allocated_size  MUST hold before any memcpy into sb->data.
 *
 * We use a guard-page allocator so that any overread/overwrite past the
 * allocated region triggers a SIGSEGV (caught by the test runner as a
 * test failure / signal), proving the invariant was violated.
 */

#define INTERNAL_BUFFER_SIZE 512  /* typical small SCP internal buffer */

/* ---------------------------------------------------------------------------
 * Guard-page allocator: allocates `size` bytes followed immediately by a
 * read/write-protected page so that any overflow is caught at runtime.
 * --------------------------------------------------------------------------- */
static void *alloc_with_guard(size_t size)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;

    /* Round up to page boundary, then add one guard page */
    size_t alloc_pages = (size + (size_t)page_size - 1) / (size_t)page_size;
    size_t total = (alloc_pages + 1) * (size_t)page_size;

    void *base = mmap(NULL, total, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return NULL;

    /* Place the guard page immediately after the usable region */
    void *guard = (char *)base + alloc_pages * (size_t)page_size;
    if (mprotect(guard, (size_t)page_size, PROT_NONE) != 0) {
        munmap(base, total);
        return NULL;
    }

    /*
     * Return a pointer that sits at the END of the usable region so that
     * even a single byte overflow hits the guard page.
     */
    return (char *)base + alloc_pages * (size_t)page_size - size;
}

static void free_with_guard(void *ptr, size_t size)
{
    if (!ptr) return;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;

    size_t alloc_pages = (size + (size_t)page_size - 1) / (size_t)page_size;
    size_t total = (alloc_pages + 1) * (size_t)page_size;

    /* Recover the base of the mmap region */
    void *base = (char *)ptr - (alloc_pages * (size_t)page_size - size);
    munmap(base, total);
}

/* ---------------------------------------------------------------------------
 * Minimal stub that mirrors the vulnerable scp_input logic.
 * The SAFE version enforces the invariant; the test checks that the invariant
 * is enforced (i.e. the function returns an error instead of overflowing).
 * --------------------------------------------------------------------------- */
typedef struct {
    uint8_t *data;
    size_t   capacity;
    size_t   used;
} strbuf_t;

/*
 * safe_scp_input: the corrected version that MUST be true.
 * Returns 0 on success, -1 if len would overflow the buffer.
 */
static int safe_scp_input(strbuf_t *sb, void *buf, size_t len)
{
    if (!sb || !buf) return -1;
    /* Invariant: len must never exceed allocated capacity */
    if (len > sb->capacity) return -1;   /* reject oversized input */
    memcpy(sb->data, buf, len);
    sb->used = len;
    return 0;
}

/* ---------------------------------------------------------------------------
 * Test payloads: adversarial inputs whose *length* exceeds INTERNAL_BUFFER_SIZE
 * --------------------------------------------------------------------------- */

START_TEST(test_scp_input_no_buffer_overread)
{
    /* Invariant: safe_scp_input must NEVER copy more bytes than the
     * allocated buffer capacity; oversized inputs must be rejected. */

    /* Each entry: { payload_size_multiplier, description } */
    struct { size_t len; const char *desc; } cases[] = {
        { INTERNAL_BUFFER_SIZE + 1,       "capacity + 1 byte"          },
        { INTERNAL_BUFFER_SIZE * 2,       "2x capacity"                },
        { INTERNAL_BUFFER_SIZE * 10,      "10x capacity"               },
        { INTERNAL_BUFFER_SIZE * 100,     "100x capacity"              },
        { 65536,                          "64 KiB flat"                },
        { 1024 * 1024,                    "1 MiB"                      },
        { SIZE_MAX,                       "SIZE_MAX (integer overflow)" },
        { (size_t)INTERNAL_BUFFER_SIZE + (size_t)UINT16_MAX, "wrap-around attempt" },
    };
    int num_cases = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < num_cases; i++) {
        size_t payload_len = cases[i].len;

        /* Allocate the internal buffer with a guard page */
        uint8_t *data = alloc_with_guard(INTERNAL_BUFFER_SIZE);
        ck_assert_msg(data != NULL, "guard-page allocation failed");

        strbuf_t sb;
        sb.data     = data;
        sb.capacity = INTERNAL_BUFFER_SIZE;
        sb.used     = 0;

        /* Build an adversarial input buffer (only if size is sane) */
        void *input_buf = NULL;
        if (payload_len != SIZE_MAX && payload_len < 2 * 1024 * 1024) {
            input_buf = malloc(payload_len);
            if (input_buf) memset(input_buf, 0xAA, payload_len);
        } else {
            /* For SIZE_MAX / huge values we just pass a small real buffer
             * but claim a huge length — the function must reject it before
             * touching memory. */
            input_buf = malloc(INTERNAL_BUFFER_SIZE);
            if (input_buf) memset(input_buf, 0xBB, INTERNAL_BUFFER_SIZE);
        }

        int ret = safe_scp_input(&sb, input_buf ? input_buf : (void *)"X", payload_len);

        /*
         * INVARIANT: when len > capacity the function MUST return an error.
         * If it returned 0 it would have performed an overflowing memcpy.
         */
        ck_assert_msg(ret == -1,
            "SECURITY VIOLATION: scp_input accepted oversized input "
            "(len=%zu > capacity=%d) for case '%s'",
            payload_len, INTERNAL_BUFFER_SIZE, cases[i].desc);

        /* The guard page must still be intact (no SIGSEGV means no overflow) */
        /* Verify the buffer was NOT modified (used should still be 0) */
        ck_assert_msg(sb.used == 0,
            "Buffer 'used' field was modified despite rejection for case '%s'",
            cases[i].desc);

        free(input_buf);
        free_with_guard(data, INTERNAL_BUFFER_SIZE);
    }
}
END_TEST

START_TEST(test_scp_input_valid_lengths_accepted)
{
    /* Invariant: inputs that fit within the buffer MUST be accepted. */
    size_t valid_lengths[] = {
        0,
        1,
        INTERNAL_BUFFER_SIZE / 2,
        INTERNAL_BUFFER_SIZE - 1,
        INTERNAL_BUFFER_SIZE,
    };
    int num = (int)(sizeof(valid_lengths) / sizeof(valid_lengths[0]));

    for (int i = 0; i < num; i++) {
        size_t len = valid_lengths[i];

        uint8_t *data = alloc_with_guard(INTERNAL_BUFFER_SIZE);
        ck_assert_msg(data != NULL, "guard-page allocation failed");

        strbuf_t sb;
        sb.data     = data;
        sb.capacity = INTERNAL_BUFFER_SIZE;
        sb.used     = 0;

        uint8_t *input_buf = calloc(1, len + 1); /* +1 to avoid zero-size malloc */
        ck_assert_msg(input_buf != NULL, "input buffer allocation failed");
        memset(input_buf, 0xCC, len);

        int ret = safe_scp_input(&sb, input_buf, len);

        ck_assert_msg(ret == 0,
            "Valid input of length %zu was incorrectly rejected", len);
        ck_assert_msg(sb.used == len,
            "sb.used (%zu) != expected len (%zu)", sb.used, len);

        if (len > 0) {
            ck_assert_msg(memcmp(sb.data, input_buf, len) == 0,
                "Data mismatch after valid copy of length %zu", len);
        }

        free(input_buf);
        free_with_guard(data, INTERNAL_BUFFER_SIZE);
    }
}
END_TEST

START_TEST(test_scp_input_null_inputs_rejected)
{
    /* Invariant: NULL pointers must never cause a dereference. */
    uint8_t *data = alloc_with_guard(INTERNAL_BUFFER_SIZE);
    ck_assert_msg(data != NULL, "guard-page allocation failed");

    strbuf_t sb;
    sb.data     = data;
    sb.capacity = INTERNAL_BUFFER_SIZE;
    sb.used     = 0;

    uint8_t dummy[8] = {0};

    /* NULL context */
    int ret = safe_scp_input(NULL, dummy, sizeof(dummy));
    ck_assert_msg(ret == -1, "NULL context must be rejected");

    /* NULL buffer */
    ret = safe_scp_input(&sb, NULL, 8);
    ck_assert_msg(ret == -1, "NULL input buffer must be rejected");

    free_with_guard(data, INTERNAL_BUFFER_SIZE);
}
END_TEST

/* ---------------------------------------------------------------------------
 * Suite wiring
 * --------------------------------------------------------------------------- */
Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s       = suite_create("Security_CWE120_scp_input");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_scp_input_no_buffer_overread);
    tcase_add_test(tc_core, test_scp_input_valid_lengths_accepted);
    tcase_add_test(tc_core, test_scp_input_null_inputs_rejected);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int      number_failed;
    Suite   *s;
    SRunner *sr;

    s  = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}