#ifndef DNS_RELAY_TEST_FRAMEWORK_H
#define DNS_RELAY_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>

static int g_tests_run;
static int g_tests_failed;

#define TEST_ASSERT(cond)                                                          \
    do                                                                           \
    {                                                                            \
        if (!(cond))                                                             \
        {                                                                        \
            fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            g_tests_failed++;                                                    \
        }                                                                        \
    } while (0)

#define RUN_TEST(fn)                                                             \
    do                                                                           \
    {                                                                            \
        g_tests_run++;                                                           \
        printf("  %s ... ", #fn);                                                \
        fflush(stdout);                                                          \
        fn();                                                                    \
        if (g_tests_failed == 0 || g_tests_failed == g_tests_run - 1)            \
        {                                                                        \
                                      \
        }                                                                        \
        printf("done\n");                                                        \
    } while (0)

#endif
