#ifndef __TEST_H__
#define __TEST_H__

#include <stdbool.h>

void test_initialize();
void suite(char *name);
void test(bool success);
void test_exit();

#endif

