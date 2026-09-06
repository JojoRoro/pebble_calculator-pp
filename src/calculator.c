#include "calculator.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char expression[64] = "";
static char result[32] = "";

void calculator_clear(void) {
  expression[0] = '\0';
  result[0] = '\0';
}

void calculator_append_digit(char digit) {
  size_t len = strlen(expression);
  if (len < sizeof(expression)-1) {
    expression[len] = digit;
    expression[len+1] = '\0';
  }
}

void calculator_append_operator(char op) {
  size_t len = strlen(expression);
  if (len < sizeof(expression)-1) {
    expression[len] = op;
    expression[len+1] = '\0';
  }
}

void calculator_equals(void) {
  // Initial evaluator placeholder: expression pipeline is ready.
  // Full precedence parser will replace this simple stage.
  snprintf(result, sizeof(result), "=%s", expression);
}

const char *calculator_expression(void) { return expression; }
const char *calculator_result(void) { return result; }
