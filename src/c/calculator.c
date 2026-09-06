#include "calculator.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *text;
  size_t pos;
  bool ok;
  bool division_by_zero;
} EvalParser;

static char s_expression[CALCULATOR_EXPRESSION_MAX] = "";
static char s_result[CALCULATOR_RESULT_MAX] = "";
static bool s_has_result = false;
static bool s_has_error = false;

static double prv_abs(double value) {
  return value < 0.0 ? -value : value;
}

static bool prv_is_operator(char c) {
  return c == '+' || c == '-' || c == '*' || c == '/';
}

static void prv_clear_result_state(void) {
  s_result[0] = '\0';
  s_has_result = false;
  s_has_error = false;
}

static bool prv_append_char(char c) {
  const size_t len = strlen(s_expression);
  if (len + 1 >= sizeof(s_expression)) {
    return false;
  }

  s_expression[len] = c;
  s_expression[len + 1] = '\0';
  return true;
}

static bool prv_result_append_char(char *buffer, size_t buffer_size, size_t *length, char c) {
  if (*length + 1 >= buffer_size) {
    return false;
  }
  buffer[(*length)++] = c;
  buffer[*length] = '\0';
  return true;
}

static bool prv_result_append_unsigned(char *buffer, size_t buffer_size, size_t *length,
                                       unsigned int value) {
  char reversed[12];
  int count = 0;

  do {
    reversed[count++] = (char)('0' + (value % 10));
    value /= 10;
  } while (value && count < (int)sizeof(reversed));

  while (count > 0) {
    if (!prv_result_append_char(buffer, buffer_size, length, reversed[--count])) {
      return false;
    }
  }
  return true;
}

// Pebble's lightweight printf does not support floating-point conversions such
// as %f/%g. Keep calculation in double, but format the result ourselves.
static bool prv_format_result(double value, char *buffer, size_t buffer_size) {
  if (!buffer || buffer_size < 2) {
    return false;
  }

  buffer[0] = '\0';
  size_t length = 0;

  if (prv_abs(value) < 1e-12) {
    value = 0.0;
  }

  if (value < 0.0) {
    if (!prv_result_append_char(buffer, buffer_size, &length, '-')) {
      return false;
    }
    value = -value;
  }

  if (value == 0.0) {
    return prv_result_append_char(buffer, buffer_size, &length, '0');
  }

  int exponent = 0;
  double normalized = value;
  while (normalized >= 10.0 && exponent < 100) {
    normalized /= 10.0;
    exponent++;
  }
  while (normalized < 1.0 && exponent > -100) {
    normalized *= 10.0;
    exponent--;
  }

  if (normalized >= 10.0 || normalized < 1.0) {
    return false;
  }

  // Ten significant decimal digits, matching the old %.10g intent.
  uint64_t scaled = (uint64_t)(normalized * 1000000000.0 + 0.5);
  if (scaled >= 10000000000ULL) {
    scaled /= 10;
    exponent++;
  }

  char digits[11];
  digits[10] = '\0';
  for (int i = 9; i >= 0; --i) {
    digits[i] = (char)('0' + (scaled % 10));
    scaled /= 10;
  }

  int significant_count = 10;
  while (significant_count > 1 && digits[significant_count - 1] == '0') {
    significant_count--;
  }

  const bool scientific = exponent >= 10 || exponent <= -5;
  if (scientific) {
    if (!prv_result_append_char(buffer, buffer_size, &length, digits[0])) {
      return false;
    }
    if (significant_count > 1) {
      if (!prv_result_append_char(buffer, buffer_size, &length, '.')) {
        return false;
      }
      for (int i = 1; i < significant_count; ++i) {
        if (!prv_result_append_char(buffer, buffer_size, &length, digits[i])) {
          return false;
        }
      }
    }

    if (!prv_result_append_char(buffer, buffer_size, &length, 'e')) {
      return false;
    }
    if (exponent < 0) {
      if (!prv_result_append_char(buffer, buffer_size, &length, '-')) {
        return false;
      }
      exponent = -exponent;
    } else if (!prv_result_append_char(buffer, buffer_size, &length, '+')) {
      return false;
    }
    return prv_result_append_unsigned(buffer, buffer_size, &length, (unsigned int)exponent);
  }

  const int decimal_position = exponent + 1;
  if (decimal_position <= 0) {
    if (!prv_result_append_char(buffer, buffer_size, &length, '0') ||
        !prv_result_append_char(buffer, buffer_size, &length, '.')) {
      return false;
    }
    for (int i = 0; i < -decimal_position; ++i) {
      if (!prv_result_append_char(buffer, buffer_size, &length, '0')) {
        return false;
      }
    }
    for (int i = 0; i < significant_count; ++i) {
      if (!prv_result_append_char(buffer, buffer_size, &length, digits[i])) {
        return false;
      }
    }
    return true;
  }

  for (int i = 0; i < significant_count; ++i) {
    if (i == decimal_position &&
        !prv_result_append_char(buffer, buffer_size, &length, '.')) {
      return false;
    }
    if (!prv_result_append_char(buffer, buffer_size, &length, digits[i])) {
      return false;
    }
  }

  for (int i = significant_count; i < decimal_position; ++i) {
    if (!prv_result_append_char(buffer, buffer_size, &length, '0')) {
      return false;
    }
  }

  return true;
}

static double prv_pow10_int(int exponent) {
  double factor = 1.0;
  if (exponent >= 0) {
    while (exponent-- > 0) {
      factor *= 10.0;
    }
  } else {
    while (exponent++ < 0) {
      factor /= 10.0;
    }
  }
  return factor;
}

static double prv_parse_expression(EvalParser *parser);

static double prv_parse_number(EvalParser *parser) {
  const char *s = parser->text;
  size_t pos = parser->pos;
  bool saw_digit = false;
  double value = 0.0;

  while (isdigit((unsigned char)s[pos])) {
    saw_digit = true;
    value = value * 10.0 + (double)(s[pos] - '0');
    pos++;
  }

  if (s[pos] == '.') {
    pos++;
    double place = 0.1;
    while (isdigit((unsigned char)s[pos])) {
      saw_digit = true;
      value += (double)(s[pos] - '0') * place;
      place *= 0.1;
      pos++;
    }
  }

  if (!saw_digit) {
    parser->ok = false;
    return 0.0;
  }

  if (s[pos] == 'e' || s[pos] == 'E') {
    const size_t exponent_start = pos;
    pos++;
    int exponent_sign = 1;
    if (s[pos] == '+' || s[pos] == '-') {
      if (s[pos] == '-') {
        exponent_sign = -1;
      }
      pos++;
    }

    if (!isdigit((unsigned char)s[pos])) {
      parser->ok = false;
      parser->pos = exponent_start;
      return 0.0;
    }

    int exponent = 0;
    while (isdigit((unsigned char)s[pos])) {
      if (exponent < 100) {
        exponent = exponent * 10 + (s[pos] - '0');
      }
      pos++;
    }
    value *= prv_pow10_int(exponent * exponent_sign);
  }

  parser->pos = pos;
  return value;
}

static double prv_parse_factor(EvalParser *parser) {
  const char *s = parser->text;
  int sign = 1;

  while (s[parser->pos] == '+' || s[parser->pos] == '-') {
    if (s[parser->pos] == '-') {
      sign = -sign;
    }
    parser->pos++;
  }

  double value;
  if (s[parser->pos] == '(') {
    parser->pos++;
    value = prv_parse_expression(parser);
    if (!parser->ok || s[parser->pos] != ')') {
      parser->ok = false;
      return 0.0;
    }
    parser->pos++;
  } else {
    value = prv_parse_number(parser);
  }

  return (double)sign * value;
}

static double prv_parse_term(EvalParser *parser) {
  double value = prv_parse_factor(parser);

  while (parser->ok) {
    const char op = parser->text[parser->pos];
    if (op != '*' && op != '/') {
      break;
    }

    parser->pos++;
    const double rhs = prv_parse_factor(parser);
    if (!parser->ok) {
      return 0.0;
    }

    if (op == '*') {
      value *= rhs;
    } else {
      if (prv_abs(rhs) < 1e-12) {
        parser->division_by_zero = true;
        parser->ok = false;
        return 0.0;
      }
      value /= rhs;
    }
  }

  return value;
}

static double prv_parse_expression(EvalParser *parser) {
  double value = prv_parse_term(parser);

  while (parser->ok) {
    const char op = parser->text[parser->pos];
    if (op != '+' && op != '-') {
      break;
    }

    parser->pos++;
    const double rhs = prv_parse_term(parser);
    if (!parser->ok) {
      return 0.0;
    }

    value = (op == '+') ? value + rhs : value - rhs;
  }

  return value;
}

static bool prv_evaluate(const char *expression, double *out_value, bool *out_division_by_zero) {
  EvalParser parser = {
      .text = expression,
      .pos = 0,
      .ok = true,
      .division_by_zero = false,
  };

  if (!expression || !expression[0]) {
    return false;
  }

  const double value = prv_parse_expression(&parser);
  if (!parser.ok || expression[parser.pos] != '\0') {
    if (out_division_by_zero) {
      *out_division_by_zero = parser.division_by_zero;
    }
    return false;
  }

  if (out_value) {
    *out_value = value;
  }
  if (out_division_by_zero) {
    *out_division_by_zero = false;
  }
  return true;
}

void calculator_clear(void) {
  s_expression[0] = '\0';
  prv_clear_result_state();
}

void calculator_backspace(void) {
  if (s_has_result || s_has_error) {
    prv_clear_result_state();
  }

  const size_t len = strlen(s_expression);
  if (len > 0) {
    s_expression[len - 1] = '\0';
  }
}

void calculator_append_digit(char digit) {
  if (digit < '0' || digit > '9') {
    return;
  }

  if (s_has_result || s_has_error) {
    calculator_clear();
  }

  (void)prv_append_char(digit);
}

void calculator_append_decimal(void) {
  if (s_has_result || s_has_error) {
    calculator_clear();
  }

  const size_t len = strlen(s_expression);
  size_t start = len;
  while (start > 0) {
    const char c = s_expression[start - 1];
    if (prv_is_operator(c) || c == '(' || c == ')') {
      break;
    }
    start--;
  }

  for (size_t i = start; i < len; ++i) {
    if (s_expression[i] == '.') {
      return;
    }
  }

  if (len == 0 || prv_is_operator(s_expression[len - 1]) || s_expression[len - 1] == '(') {
    if (!prv_append_char('0')) {
      return;
    }
  }
  (void)prv_append_char('.');
}

void calculator_append_operator(char op) {
  if (!prv_is_operator(op)) {
    return;
  }

  if (s_has_error) {
    return;
  }

  if (s_has_result) {
    strncpy(s_expression, s_result, sizeof(s_expression) - 1);
    s_expression[sizeof(s_expression) - 1] = '\0';
    prv_clear_result_state();
  }

  size_t len = strlen(s_expression);
  if (len == 0) {
    if (op == '-') {
      (void)prv_append_char(op);
    }
    return;
  }

  if (prv_is_operator(s_expression[len - 1])) {
    s_expression[len - 1] = op;
    return;
  }

  (void)prv_append_char(op);
}

bool calculator_set_expression(const char *expression) {
  if (!expression || !expression[0]) {
    return false;
  }

  const size_t len = strlen(expression);
  if (len >= sizeof(s_expression)) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    const char c = expression[i];
    if (!(isdigit((unsigned char)c) || c == '.' || c == '+' || c == '-' || c == '*' ||
          c == '/' || c == '(' || c == ')' || c == 'e' || c == 'E')) {
      return false;
    }
  }

  strcpy(s_expression, expression);
  prv_clear_result_state();
  return true;
}

bool calculator_equals(void) {
  bool division_by_zero = false;
  double value = 0.0;

  if (!prv_evaluate(s_expression, &value, &division_by_zero)) {
    s_has_result = false;
    s_has_error = true;
    snprintf(s_result, sizeof(s_result), "%s", division_by_zero ? "Division by zero" : "Invalid expression");
    return false;
  }

  if (!prv_format_result(value, s_result, sizeof(s_result))) {
    s_has_result = false;
    s_has_error = true;
    snprintf(s_result, sizeof(s_result), "%s", "Result out of range");
    return false;
  }

  s_has_result = true;
  s_has_error = false;
  return true;
}

const char *calculator_expression(void) {
  return s_expression;
}

const char *calculator_result(void) {
  return s_result;
}

bool calculator_has_result(void) {
  return s_has_result;
}

bool calculator_has_error(void) {
  return s_has_error;
}
