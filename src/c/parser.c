#include "parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_TOKENS 40
#define TOKEN_SIZE 20

typedef char Token[TOKEN_SIZE];

static void prv_flush_token(Token tokens[], int *count, char *buffer, int *length) {
  if (*length == 0 || *count >= MAX_TOKENS) {
    *length = 0;
    return;
  }

  buffer[*length] = '\0';
  strncpy(tokens[*count], buffer, TOKEN_SIZE - 1);
  tokens[*count][TOKEN_SIZE - 1] = '\0';
  (*count)++;
  *length = 0;
}

static int prv_tokenize(const char *input, Token tokens[]) {
  int count = 0;
  char buffer[TOKEN_SIZE];
  int length = 0;

  for (size_t i = 0; input && input[i] != '\0'; ++i) {
    const unsigned char raw = (unsigned char)input[i];
    const char c = (char)tolower(raw);

    if (isalnum(raw) || c == '.') {
      if (length < TOKEN_SIZE - 1) {
        buffer[length++] = c;
      }
      continue;
    }

    prv_flush_token(tokens, &count, buffer, &length);
    if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '=') && count < MAX_TOKENS) {
      tokens[count][0] = c;
      tokens[count][1] = '\0';
      count++;
    }
  }

  prv_flush_token(tokens, &count, buffer, &length);
  return count;
}

static char prv_operator_for_token(const char *token) {
  if (!strcmp(token, "+") || !strcmp(token, "plus") || !strcmp(token, "add") ||
      !strcmp(token, "added")) {
    return '+';
  }
  if (!strcmp(token, "-") || !strcmp(token, "minus") || !strcmp(token, "subtract") ||
      !strcmp(token, "subtracted")) {
    return '-';
  }
  if (!strcmp(token, "*") || !strcmp(token, "x") || !strcmp(token, "times") ||
      !strcmp(token, "multiply") || !strcmp(token, "multiplied")) {
    return '*';
  }
  if (!strcmp(token, "/") || !strcmp(token, "over") || !strcmp(token, "divide") ||
      !strcmp(token, "divided")) {
    return '/';
  }
  return '\0';
}

static bool prv_is_terminal(const char *token) {
  return !strcmp(token, "=") || !strcmp(token, "equals") || !strcmp(token, "equal");
}

static bool prv_is_filler(const char *token) {
  return !strcmp(token, "by") || !strcmp(token, "calculate") || !strcmp(token, "please") ||
         !strcmp(token, "what") || !strcmp(token, "is") || !strcmp(token, "the") ||
         !strcmp(token, "result") || !strcmp(token, "of");
}

static int prv_small_number(const char *token) {
  static const char *const words[] = {
      "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
      "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
      "eighteen", "nineteen",
  };

  for (int i = 0; i < (int)(sizeof(words) / sizeof(words[0])); ++i) {
    if (!strcmp(token, words[i])) {
      return i;
    }
  }
  return -1;
}

static int prv_tens_number(const char *token) {
  static const struct {
    const char *word;
    int value;
  } values[] = {
      {"twenty", 20}, {"thirty", 30}, {"forty", 40}, {"fifty", 50},
      {"sixty", 60},  {"seventy", 70}, {"eighty", 80}, {"ninety", 90},
  };

  for (int i = 0; i < (int)(sizeof(values) / sizeof(values[0])); ++i) {
    if (!strcmp(token, values[i].word)) {
      return values[i].value;
    }
  }
  return -1;
}

static bool prv_is_numeric_token(const char *token) {
  bool saw_digit = false;
  bool saw_dot = false;

  for (size_t i = 0; token[i] != '\0'; ++i) {
    if (isdigit((unsigned char)token[i])) {
      saw_digit = true;
    } else if (token[i] == '.' && !saw_dot) {
      saw_dot = true;
    } else {
      return false;
    }
  }
  return saw_digit;
}

static bool prv_append_text(char *output, size_t output_size, const char *text) {
  const size_t current = strlen(output);
  const size_t add = strlen(text);
  if (current + add >= output_size) {
    return false;
  }
  memcpy(output + current, text, add + 1);
  return true;
}

static bool prv_append_char(char *output, size_t output_size, char c) {
  const size_t len = strlen(output);
  if (len + 1 >= output_size) {
    return false;
  }
  output[len] = c;
  output[len + 1] = '\0';
  return true;
}

static bool prv_parse_word_number(Token tokens[], int count, int start, int *used, char *number,
                                  size_t number_size) {
  long total = 0;
  long current = 0;
  bool seen = false;
  bool negative = false;
  bool decimal = false;
  char decimal_digits[16] = "";
  int decimal_len = 0;
  int i = start;

  if (i < count && !strcmp(tokens[i], "negative")) {
    negative = true;
    i++;
  }

  for (; i < count; ++i) {
    const char *token = tokens[i];
    if (prv_operator_for_token(token) || prv_is_terminal(token)) {
      break;
    }

    if (!strcmp(token, "and") && !decimal) {
      continue;
    }

    if (!strcmp(token, "point") || !strcmp(token, "dot")) {
      if (!seen || decimal) {
        break;
      }
      decimal = true;
      continue;
    }

    if (decimal) {
      int digit = prv_small_number(token);
      if (digit >= 0 && digit <= 9) {
        if (decimal_len < (int)sizeof(decimal_digits) - 1) {
          decimal_digits[decimal_len++] = (char)('0' + digit);
          decimal_digits[decimal_len] = '\0';
          continue;
        }
        break;
      }

      if (prv_is_numeric_token(token)) {
        for (size_t j = 0; token[j] != '\0'; ++j) {
          if (isdigit((unsigned char)token[j]) && decimal_len < (int)sizeof(decimal_digits) - 1) {
            decimal_digits[decimal_len++] = token[j];
          }
        }
        decimal_digits[decimal_len] = '\0';
        continue;
      }
      break;
    }

    const int small = prv_small_number(token);
    if (small >= 0) {
      current += small;
      seen = true;
      continue;
    }

    const int tens = prv_tens_number(token);
    if (tens >= 0) {
      current += tens;
      seen = true;
      continue;
    }

    if (!strcmp(token, "hundred")) {
      current = (current == 0 ? 1 : current) * 100;
      seen = true;
      continue;
    }

    if (!strcmp(token, "thousand")) {
      total += (current == 0 ? 1 : current) * 1000;
      current = 0;
      seen = true;
      continue;
    }

    if (!strcmp(token, "million")) {
      total += (current == 0 ? 1 : current) * 1000000;
      current = 0;
      seen = true;
      continue;
    }

    break;
  }

  if (!seen || (decimal && decimal_len == 0)) {
    return false;
  }

  const long integer_value = total + current;
  if (decimal) {
    snprintf(number, number_size, "%s%ld.%s", negative ? "-" : "", integer_value, decimal_digits);
  } else {
    snprintf(number, number_size, "%s%ld", negative ? "-" : "", integer_value);
  }

  *used = i - start;
  return *used > 0;
}

static bool prv_append_numeric_with_decimal(Token tokens[], int count, int *index, char *output,
                                            size_t output_size) {
  if (!prv_append_text(output, output_size, tokens[*index])) {
    return false;
  }

  int i = *index + 1;
  if (i >= count || (strcmp(tokens[i], "point") && strcmp(tokens[i], "dot"))) {
    return true;
  }

  if (strchr(tokens[*index], '.')) {
    return false;
  }

  if (!prv_append_char(output, output_size, '.')) {
    return false;
  }
  i++;

  bool added_decimal = false;
  for (; i < count; ++i) {
    int digit = prv_small_number(tokens[i]);
    if (digit >= 0 && digit <= 9) {
      if (!prv_append_char(output, output_size, (char)('0' + digit))) {
        return false;
      }
      added_decimal = true;
      continue;
    }

    if (prv_is_numeric_token(tokens[i])) {
      for (size_t j = 0; tokens[i][j] != '\0'; ++j) {
        if (isdigit((unsigned char)tokens[i][j])) {
          if (!prv_append_char(output, output_size, tokens[i][j])) {
            return false;
          }
          added_decimal = true;
        }
      }
      continue;
    }
    break;
  }

  if (!added_decimal) {
    return false;
  }
  *index = i - 1;
  return true;
}

bool parser_normalize_expression(const char *input, char *output, size_t output_size) {
  if (!input || !output || output_size < 2) {
    return false;
  }

  output[0] = '\0';
  Token tokens[MAX_TOKENS];
  const int count = prv_tokenize(input, tokens);
  bool expect_number = true;
  bool have_number = false;

  for (int i = 0; i < count; ++i) {
    const char *token = tokens[i];

    if (prv_is_terminal(token)) {
      break;
    }

    if (prv_is_filler(token)) {
      continue;
    }

    const char op = prv_operator_for_token(token);
    if (op) {
      if (expect_number) {
        if (op == '-' && prv_append_char(output, output_size, '-')) {
          continue;
        }
        return false;
      }

      if (!prv_append_char(output, output_size, op)) {
        return false;
      }
      expect_number = true;
      continue;
    }

    if (!expect_number) {
      return false;
    }

    if (prv_is_numeric_token(token)) {
      if (!prv_append_numeric_with_decimal(tokens, count, &i, output, output_size)) {
        return false;
      }
      expect_number = false;
      have_number = true;
      continue;
    }

    int used = 0;
    char number[40];
    if (prv_parse_word_number(tokens, count, i, &used, number, sizeof(number))) {
      if (!prv_append_text(output, output_size, number)) {
        return false;
      }
      i += used - 1;
      expect_number = false;
      have_number = true;
      continue;
    }

    return false;
  }

  return have_number && !expect_number && output[0] != '\0';
}
