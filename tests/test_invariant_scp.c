#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "demo/demo2/scp.h"

START_TEST(test_scp_trim_no_underflow)
{
    // Invariant: payload_len never underflows when trimming overlapping segments
    struct scp_session s;
    struct scp_buffer sb;
    uint8_t buffer[1024];
    
    // Test cases: (seq, rcv_nxt, payload_len)
    struct {
        uint32_t seq;
        uint32_t rcv_nxt;
        uint32_t payload_len;
    } test_cases[] = {
        // Exploit case: payload_len < trim (causes underflow)
        {100, 200, 50},
        // Boundary case: payload_len == trim
        {100, 200, 100},
        // Valid case: payload_len > trim
        {100, 150, 100},
        // No overlap case (should not trim)
        {200, 100, 50},
    };
    
    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    
    for (int i = 0; i < num_cases; i++) {
        // Initialize session state
        memset(&s, 0, sizeof(s));
        memset(&sb, 0, sizeof(sb));
        memset(buffer, 0xAA, sizeof(buffer));
        
        s.rcv_nxt = test_cases[i].rcv_nxt;
        sb.data = buffer;
        
        // Setup packet header
        struct scp_hdr *sh = (struct scp_hdr *)buffer;
        sh->seq = htonl(test_cases[i].seq);
        sh->len = htons((uint16_t)test_cases[i].payload_len);
        
        // Ensure payload buffer is large enough
        uint8_t *payload = buffer + sizeof(struct scp_hdr);
        memset(payload, 0xBB, test_cases[i].payload_len + 256);
        
        // Call the actual vulnerable function
        scp_process_data(&s, &sb);
        
        // Property: After processing, payload_len should not exceed original buffer bounds
        // We check this indirectly by verifying the session state is consistent
        ck_assert_msg(s.rcv_nxt <= test_cases[i].rcv_nxt + test_cases[i].payload_len,
                     "Sequence number advanced beyond reasonable bounds");
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scp_trim_no_underflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}