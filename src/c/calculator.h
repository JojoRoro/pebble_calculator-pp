#pragma once

#include <stdbool.h>
#include <stddef.h>

#define CALCULATOR_EXPRESSION_MAX 96
#define CALCULATOR_RESULT_MAX 48

void calculator_clear(void);
void calculator_backspace(void);
void calculator_append_digit(char digit);
void calculator_append_decimal(void);
void calculator_append_operator(char op);
bool calculator_set_expression(const char *expression);
bool calculator_equals(void);

const char *calculator_expression(void);
const char *calculator_result(void);
bool calculator_has_result(void);
bool calculator_has_error(void);
