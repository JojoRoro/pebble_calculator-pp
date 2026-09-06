#include "parser.h"
#include <string.h>

void parser_normalize(const char *input, char *output, int size) {
  strncpy(output, input, size - 1);
  output[size - 1] = 0;
}
