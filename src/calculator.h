#pragma once

#include <pebble.h>

void calculator_clear(void);
void calculator_append_digit(char digit);
void calculator_append_operator(char op);
void calculator_equals(void);
const char *calculator_expression(void);
const char *calculator_result(void);
