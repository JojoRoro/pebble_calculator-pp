#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/calculator.h"
#include "../src/c/parser.h"

static void check_voice(const char *speech, const char *expression, const char *result) {
  char parsed[CALCULATOR_EXPRESSION_MAX];
  assert(parser_normalize_expression(speech, parsed, sizeof(parsed)));
  assert(strcmp(parsed, expression) == 0);
  assert(calculator_set_expression(parsed));
  assert(calculator_equals());
  assert(strcmp(calculator_result(), result) == 0);
}

int main(void) {
  check_voice("22 divided by 55 plus 8 equals", "22/55+8", "8.4");
  check_voice("twenty two divided by fifty five plus eight", "22/55+8", "8.4");
  check_voice("10 times 5", "10*5", "50");
  check_voice("one hundred minus twenty", "100-20", "80");
  check_voice("fifty point five plus two", "50.5+2", "52.5");
  check_voice("negative five plus two", "-5+2", "-3");
  check_voice("2 plus 3 times 4", "2+3*4", "14");

  calculator_clear();
  calculator_append_digit('1');
  calculator_append_digit('0');
  calculator_append_operator('/');
  calculator_append_digit('0');
  assert(!calculator_equals());
  assert(calculator_has_error());
  assert(strcmp(calculator_result(), "Division by zero") == 0);

  puts("MinusPP logic tests passed");
  return 0;
}
