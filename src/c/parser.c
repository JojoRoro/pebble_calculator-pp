#include "parser.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_TOKENS 40
#define TOKEN_SIZE 20

// Largest spoken number accepted. Pebble's long is 32-bit, so word numbers are built in 64-bit,
// and the cap keeps them well inside both int64 and the exact-integer range of double.
#define MAX_SPOKEN_NUMBER 999999999999999LL

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

// Dictation may emit typographic math symbols instead of words. Returns the ASCII operator for a
// UTF-8 sequence at input and stores its byte length, or returns '\0' when it is not one.
static char prv_unicode_operator(const char *input, size_t *length) {
  const unsigned char *s = (const unsigned char *)input;
  if (s[0] == 0xC3 && s[1] == 0xB7) {  // ÷
    *length = 2;
    return '/';
  }
  if (s[0] == 0xC3 && s[1] == 0x97) {  // ×
    *length = 2;
    return '*';
  }
  if (s[0] == 0xE2 && s[1] == 0x88 && s[2] == 0x92) {  // − (minus sign)
    *length = 3;
    return '-';
  }
  return '\0';
}

static int prv_tokenize(const char *input, Token tokens[]) {
  int count = 0;
  char buffer[TOKEN_SIZE];
  int length = 0;

  for (size_t i = 0; input && input[i] != '\0'; ++i) {
    const unsigned char raw = (unsigned char)input[i];
    const char c = (char)tolower(raw);

    if (isalnum(raw)) {
      if (length < TOKEN_SIZE - 1) {
        buffer[length++] = c;
      }
      continue;
    }

    // Keep a period only when it begins/continues a decimal number. Dictation commonly appends
    // sentence punctuation (for example "five over ten."), which must not become part of a word
    // token such as "ten.". Looking at the following character preserves both 5.5 and .5 while
    // treating a trailing period as punctuation.
    if (c == '.' && isdigit((unsigned char)input[i + 1])) {
      if (length < TOKEN_SIZE - 1) {
        buffer[length++] = c;
      }
      continue;
    }

    // Hyphenated number words such as "forty-five" are a single spoken number, not a subtraction.
    // Spoken "minus" arrives as a word or a spaced/digit-adjacent '-', never between letters.
    if (c == '-' && length > 0 && isalpha((unsigned char)buffer[length - 1]) &&
        isalpha((unsigned char)input[i + 1])) {
      prv_flush_token(tokens, &count, buffer, &length);
      continue;
    }

    prv_flush_token(tokens, &count, buffer, &length);
    size_t symbol_length = 0;
    const char symbol = prv_unicode_operator(input + i, &symbol_length);
    if (symbol) {
      if (count < MAX_TOKENS) {
        tokens[count][0] = symbol;
        tokens[count][1] = '\0';
        count++;
      }
      i += symbol_length - 1;
      continue;
    }
    if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '=') &&
        count < MAX_TOKENS) {
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
  if (!strcmp(token, "^") || !strcmp(token, "power")) {
    return '^';
  }
  return '\0';
}

// "to" and "raised" only introduce "the power of"; they carry no meaning on their own.
static bool prv_is_power_lead(Token tokens[], int count, int index) {
  if (strcmp(tokens[index], "to") && strcmp(tokens[index], "raised")) {
    return false;
  }
  for (int i = index + 1; i < count; ++i) {
    if (!strcmp(tokens[i], "power")) {
      return true;
    }
    if (strcmp(tokens[i], "to") && strcmp(tokens[i], "the")) {
      return false;
    }
  }
  return false;
}

static const char *prv_postfix_power(const char *token) {
  if (!strcmp(token, "squared")) {
    return "^2";
  }
  if (!strcmp(token, "cubed")) {
    return "^3";
  }
  return NULL;
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

static int64_t prv_scale_number(const char *token) {
  if (!strcmp(token, "thousand")) {
    return 1000LL;
  }
  if (!strcmp(token, "million")) {
    return 1000000LL;
  }
  if (!strcmp(token, "billion")) {
    return 1000000000LL;
  }
  if (!strcmp(token, "trillion")) {
    return 1000000000000LL;
  }
  return 0;
}

// Pebble's printf has no 64-bit integer conversion, so format the digits here.
static void prv_format_integer(int64_t value, char *buffer, size_t buffer_size) {
  char reversed[24];
  int length = 0;
  do {
    reversed[length++] = (char)('0' + (int)(value % 10));
    value /= 10;
  } while (value > 0 && length < (int)sizeof(reversed));

  size_t i = 0;
  while (length > 0 && i + 1 < buffer_size) {
    buffer[i++] = reversed[--length];
  }
  buffer[i] = '\0';
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

static int64_t prv_parse_integer_text(const char *text, size_t length) {
  if (length == 0 || length > 15) {
    return -1;
  }
  int64_t value = 0;
  for (size_t i = 0; i < length; ++i) {
    value = value * 10 + (text[i] - '0');
  }
  return value;
}

// Applies a scale word to a decimal such as "1.5 billion": the scale's zeros absorb decimal
// digits, and any that do not fit (as in "1.2345 thousand") stay as the fraction.
static bool prv_scale_decimal(int64_t whole, char *digits, int *digit_count, int64_t scale,
                              int64_t *out) {
  if (whole > MAX_SPOKEN_NUMBER / scale) {
    return false;
  }
  int64_t value = whole * scale;
  int64_t place = scale;
  int taken = 0;
  while (place > 1 && taken < *digit_count) {
    place /= 10;
    value += (digits[taken++] - '0') * place;
  }
  memmove(digits, digits + taken, (size_t)(*digit_count - taken) + 1);
  *digit_count -= taken;
  *out = value;
  return true;
}

static bool prv_parse_word_number(Token tokens[], int count, int start, int *used, char *number,
                                  size_t number_size) {
  int64_t total = 0;
  int64_t current = 0;
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

    // "a hundred", "a thousand": the multiplier below already treats a missing count as one.
    if (!strcmp(token, "a") && !decimal && i + 1 < count &&
        (!strcmp(tokens[i + 1], "hundred") || prv_scale_number(tokens[i + 1]))) {
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
      const int64_t decimal_scale = prv_scale_number(token);
      if (decimal_scale > 0) {
        int64_t scaled = 0;
        if (decimal_len == 0 ||
            !prv_scale_decimal(current, decimal_digits, &decimal_len, decimal_scale, &scaled)) {
          return false;
        }
        total += scaled;
        current = 0;
        decimal = decimal_len > 0;
        i++;
        break;
      }

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

    // Digits mixed with scale words: "3 billion", "1.5 trillion", "757 billion 945".
    if (prv_is_numeric_token(token) && current == 0) {
      const bool next_scales =
          i + 1 < count && (prv_scale_number(tokens[i + 1]) || !strcmp(tokens[i + 1], "hundred"));
      const bool after_scale = i > start && prv_scale_number(tokens[i - 1]);
      const char *dot = strchr(token, '.');
      if (dot && next_scales && prv_scale_number(tokens[i + 1])) {
        current = prv_parse_integer_text(token, (size_t)(dot - token));
        decimal_len = (int)strlen(dot + 1);
        if (current < 0 || decimal_len >= (int)sizeof(decimal_digits)) {
          return false;
        }
        memcpy(decimal_digits, dot + 1, (size_t)decimal_len + 1);
        decimal = true;
        seen = true;
        continue;
      }
      const bool next_point = i + 1 < count &&
                              (!strcmp(tokens[i + 1], "point") || !strcmp(tokens[i + 1], "dot"));
      if (!dot && (next_scales || after_scale || next_point)) {
        current = prv_parse_integer_text(token, strlen(token));
        if (current < 0) {
          return false;
        }
        seen = true;
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
      if (current > MAX_SPOKEN_NUMBER / 100) {
        return false;
      }
      current = (current == 0 ? 1 : current) * 100;
      seen = true;
      continue;
    }

    const int64_t scale = prv_scale_number(token);
    if (scale > 0) {
      const int64_t multiplier = current == 0 ? 1 : current;
      if (multiplier > MAX_SPOKEN_NUMBER / scale) {
        return false;
      }
      total += multiplier * scale;
      current = 0;
      seen = true;
      continue;
    }

    break;
  }

  if (!seen || (decimal && decimal_len == 0)) {
    return false;
  }

  const int64_t integer_value = total + current;
  if (integer_value > MAX_SPOKEN_NUMBER) {
    return false;
  }

  char integer_text[24];
  prv_format_integer(integer_value, integer_text, sizeof(integer_text));
  if (decimal) {
    snprintf(number, number_size, "%s%s.%s", negative ? "-" : "", integer_text, decimal_digits);
  } else {
    snprintf(number, number_size, "%s%s", negative ? "-" : "", integer_text);
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

    if (prv_is_filler(token) || prv_is_power_lead(tokens, count, i)) {
      continue;
    }

    const char *postfix = prv_postfix_power(token);
    if (postfix) {
      if (expect_number || !prv_append_text(output, output_size, postfix)) {
        return false;
      }
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

    const bool scaled_digits =
        i + 1 < count && (prv_scale_number(tokens[i + 1]) || !strcmp(tokens[i + 1], "hundred"));
    if (prv_is_numeric_token(token) && !scaled_digits) {
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
