#ifndef __TEST_H__
#define __TEST_H__

#include <stdbool.h>
#include <stdio.h>

extern char *TEST_current_suite;
extern int TEST_failed;
extern int TEST_succeeded;
extern bool TEST_allsucceeded;
extern bool TEST_verbose;

void test_initialize(int argc, char **argv);
void suite(char *name);

#define test(arg) do { if ( arg ) { \
        TEST_succeeded++; \
    } else { \
        TEST_allsucceeded = false; \
        TEST_failed++; \
        printf("\ntest failed (suite %s): %s on %s line %d\n", TEST_current_suite, #arg, __FILE__, __LINE__);\
    } \
    \
    if ( (TEST_failed + TEST_succeeded % 100 == 0) && TEST_verbose ) \
        fprintf(stderr, "\r%s: %ds %df", TEST_current_suite, TEST_succeeded, TEST_failed); \
} while (0)

void test_exit();

#endif

