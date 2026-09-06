#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  PARSER_LANGUAGE_ENGLISH = 0,
  PARSER_LANGUAGE_GERMAN = 1,
  PARSER_LANGUAGE_FRENCH = 2,
} ParserLanguage;

bool parser_normalize_expression_for_language(const char *input, ParserLanguage language,
                                              char *output, size_t output_size);

bool parser_normalize_expression(const char *input, char *output, size_t output_size);
