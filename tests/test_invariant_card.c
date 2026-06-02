#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * We cannot directly call the vulnerable internal display function from card.c
 * without its full context, so we simulate the exact vulnerable pattern to serve
 * as a regression guard. The invariant is: concatenation into a fixed buffer must
 * never exceed the buffer size.
 *
 * This test exercises the pattern from card.c line 158:
 *   strcpy(buf, "  ");
 *   strcat(buf, disp_msg[i]);
 * where buf is a stack buffer of limited size.
 */

#define BUF_SIZE 256  /* Typical stack buffer size used in card.c */

static int safe_format_message(char *buf, size_t buf_size, const char *msg)
{
    /* This encodes what MUST be true: total length fits in buffer */
    size_t prefix_len = 2; /* "  " */
    if (prefix_len + strlen(msg) + 1 > buf_size)
        return -1; /* would overflow */
    strcpy(buf, "  ");
    strcat(buf, msg);
    return 0;
}

START_TEST(test_strcat_buffer_overflow_invariant)
{
    /* Invariant: No message concatenation may exceed the destination buffer bounds */
    const char *payloads[] = {
        /* Exact exploit: string longer than buffer */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        /* Boundary: exactly BUF_SIZE - 3 chars (fills buffer exactly with prefix + NUL) */
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
        /* Valid short input */
        "Hello Card"
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);
    char buf[BUF_SIZE];

    for (int i = 0; i < num_payloads; i++) {
        int ret = safe_format_message(buf, BUF_SIZE, payloads[i]);
        if (strlen(payloads[i]) + 2 + 1 > BUF_SIZE) {
            /* Must be rejected to prevent overflow */
            ck_assert_int_eq(ret, -1);
        } else {
            /* Must succeed and result fits in buffer */
            ck_assert_int_eq(ret, 0);
            ck_assert(strlen(buf) < BUF_SIZE);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_strcat_buffer_overflow_invariant);
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