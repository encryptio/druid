#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "test.h"

static char *TEST_current_suite = NULL;
static uint_fast32_t TEST_failed = 0;
static uint_fast32_t TEST_succeeded = 0;
static bool TEST_allsucceeded = true;
static bool TEST_verbose = false;

void test_initialize(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if ( argc == 2 && strcmp(argv[1], "verbose") == 0 ) {
        TEST_verbose = true;
    }
}

void suite(char *name) {
    if ( TEST_current_suite ) {
        if ( TEST_failed || TEST_verbose ) {
            fprintf(stderr, "\r%s: %ds %df\n%s", TEST_current_suite, TEST_succeeded, TEST_failed, name);
        }
    } else {
        if ( TEST_verbose )
            fprintf(stderr, "%s", name);
    }
    if ( TEST_current_suite )
        free(TEST_current_suite);
    TEST_current_suite = strdup(name);
    TEST_failed = 0;
    TEST_succeeded = 0;
}

void test(bool success) {
    if ( success ) {
        TEST_succeeded++;
    } else {
        TEST_allsucceeded = false;
        TEST_failed++;
    }

    if ( (TEST_failed + TEST_succeeded % 100 == 0) && TEST_verbose )
        fprintf(stderr, "\r%s: %ds %df", TEST_current_suite, TEST_succeeded, TEST_failed);
}

void test_exit() {
    if ( TEST_failed || TEST_verbose ) {
        fprintf(stderr, "\r%s: %ds %df\n", TEST_current_suite, TEST_succeeded, TEST_failed);
    }
    exit(TEST_allsucceeded ? EXIT_SUCCESS : EXIT_FAILURE);
}

