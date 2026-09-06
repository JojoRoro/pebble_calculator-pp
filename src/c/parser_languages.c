#include "parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_TOKENS 64
#define TOKEN_SIZE 48

typedef char Token[TOKEN_SIZE];

static void flush(Token tokens[], int *count, char *buf, int *len) {
  if (*len == 0 || *count >= MAX_TOKENS) { *len = 0; return; }
  buf[*len] = '\0';
  strncpy(tokens[*count], buf, TOKEN_SIZE - 1);
  tokens[*count][TOKEN_SIZE - 1] = '\0';
  (*count)++;
  *len = 0;
}

static bool append_piece(char *buf, int *len, const char *s) {
  while (*s) {
    if (*len >= TOKEN_SIZE - 1) return false;
    buf[(*len)++] = *s++;
  }
  return true;
}

static const char *fold_pair(unsigned char a, unsigned char b) {
  if (a == 0xC3) {
    switch (b) {
      case 0x84: case 0xA4: return "ae";
      case 0x96: case 0xB6: return "oe";
      case 0x9C: case 0xBC: return "ue";
      case 0x9F: return "ss";
      case 0x80: case 0x81: case 0x82: case 0x83: case 0x85:
      case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA5: return "a";
      case 0x87: case 0xA7: return "c";
      case 0x88: case 0x89: case 0x8A: case 0x8B:
      case 0xA8: case 0xA9: case 0xAA: case 0xAB: return "e";
      case 0x8C: case 0x8D: case 0x8E: case 0x8F:
      case 0xAC: case 0xAD: case 0xAE: case 0xAF: return "i";
      case 0x91: case 0xB1: return "n";
      case 0x92: case 0x93: case 0x94: case 0x95:
      case 0xB2: case 0xB3: case 0xB4: case 0xB5: return "o";
      case 0x99: case 0x9A: case 0x9B:
      case 0xB9: case 0xBA: case 0xBB: return "u";
      case 0x9D: case 0xBD: case 0xBF: return "y";
      default: return NULL;
    }
  }
  if (a == 0xC5 && (b == 0x92 || b == 0x93)) return "oe";
  return NULL;
}

static bool buf_alpha(const char *buf, int len) {
  for (int i = 0; i < len; ++i) if (buf[i] >= 'a' && buf[i] <= 'z') return true;
  return false;
}

static bool next_alpha(const char *input, size_t i) {
  unsigned char c = (unsigned char)input[i + 1];
  return isalpha(c) || c >= 0x80;
}

static bool buf_numeric(const char *buf, int len) {
  bool digit = false;
  for (int i = 0; i < len; ++i) {
    if (isdigit((unsigned char)buf[i])) digit = true;
    else if (buf[i] != '.') return false;
  }
  return digit;
}

static int tokenize(const char *input, Token tokens[]) {
  int count = 0, len = 0;
  char buf[TOKEN_SIZE];

  for (size_t i = 0; input && input[i]; ++i) {
    unsigned char raw = (unsigned char)input[i];
    if (raw < 0x80) {
      char c = (char)tolower(raw);
      if (isalnum(raw)) {
        if (len < TOKEN_SIZE - 1) buf[len++] = c;
        continue;
      }
      if (c == '-' && buf_alpha(buf, len) && next_alpha(input, i)) {
        flush(tokens, &count, buf, &len);
        continue;
      }
      if ((c == '.' || c == ',') && isdigit((unsigned char)input[i + 1]) &&
          (len == 0 || buf_numeric(buf, len))) {
        if (len < TOKEN_SIZE - 1) buf[len++] = '.';
        continue;
      }
      flush(tokens, &count, buf, &len);
      if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '=') && count < MAX_TOKENS) {
        tokens[count][0] = c;
        tokens[count][1] = '\0';
        count++;
      }
      continue;
    }

    const char *folded = input[i + 1] ? fold_pair(raw, (unsigned char)input[i + 1]) : NULL;
    if (folded) {
      append_piece(buf, &len, folded);
      i++;
      continue;
    }
    flush(tokens, &count, buf, &len);
    while (input[i + 1] && (((unsigned char)input[i + 1] & 0xC0) == 0x80)) i++;
  }
  flush(tokens, &count, buf, &len);
  return count;
}

static bool numeric(const char *s) {
  bool digit = false, dot = false;
  for (; *s; ++s) {
    if (isdigit((unsigned char)*s)) digit = true;
    else if (*s == '.' && !dot) dot = true;
    else return false;
  }
  return digit;
}

static bool append_text(char *out, size_t size, const char *text) {
  size_t a = strlen(out), b = strlen(text);
  if (a + b >= size) return false;
  memcpy(out + a, text, b + 1);
  return true;
}

static bool append_char(char *out, size_t size, char c) {
  size_t n = strlen(out);
  if (n + 1 >= size) return false;
  out[n] = c;
  out[n + 1] = '\0';
  return true;
}

static char op_for(const char *t, ParserLanguage lang) {
  if (!strcmp(t, "+")) return '+';
  if (!strcmp(t, "-")) return '-';
  if (!strcmp(t, "*") || !strcmp(t, "x")) return '*';
  if (!strcmp(t, "/")) return '/';

  if (lang == PARSER_LANGUAGE_GERMAN) {
    if (!strcmp(t, "plus") || !strcmp(t, "addiert") || !strcmp(t, "addiere")) return '+';
    if (!strcmp(t, "minus") || !strcmp(t, "weniger") || !strcmp(t, "subtrahiere")) return '-';
    if (!strcmp(t, "mal") || !strcmp(t, "multipliziert") || !strcmp(t, "multipliziere")) return '*';
    if (!strcmp(t, "durch")) return '/';
  } else {
    if (!strcmp(t, "plus") || !strcmp(t, "additionne")) return '+';
    if (!strcmp(t, "moins") || !strcmp(t, "soustrais") || !strcmp(t, "soustrait")) return '-';
    if (!strcmp(t, "fois") || !strcmp(t, "multiplie")) return '*';
    if (!strcmp(t, "sur") || !strcmp(t, "divise")) return '/';
  }
  return '\0';
}

static bool terminal(const char *t, ParserLanguage lang) {
  if (!strcmp(t, "=")) return true;
  if (lang == PARSER_LANGUAGE_GERMAN) return !strcmp(t, "gleich") || !strcmp(t, "ergibt");
  return !strcmp(t, "egal") || !strcmp(t, "egale") || !strcmp(t, "egalent") || !strcmp(t, "donne");
}

static bool filler(const char *t, ParserLanguage lang) {
  if (lang == PARSER_LANGUAGE_GERMAN) {
    return !strcmp(t, "bitte") || !strcmp(t, "ist") || !strcmp(t, "das") ||
           !strcmp(t, "ergebnis") || !strcmp(t, "berechne") || !strcmp(t, "rechne") ||
           !strcmp(t, "geteilt") || !strcmp(t, "dividiert") || !strcmp(t, "mit") ||
           !strcmp(t, "von");
  }
  return !strcmp(t, "calcule") || !strcmp(t, "calculer") || !strcmp(t, "est") ||
         !strcmp(t, "le") || !strcmp(t, "la") || !strcmp(t, "resultat") ||
         !strcmp(t, "de") || !strcmp(t, "par") || !strcmp(t, "a") ||
         !strcmp(t, "s") || !strcmp(t, "il") || !strcmp(t, "vous") || !strcmp(t, "plait");
}

static bool decimal_word(const char *t, ParserLanguage lang) {
  if (lang == PARSER_LANGUAGE_GERMAN) return !strcmp(t, "komma") || !strcmp(t, "punkt");
  return !strcmp(t, "virgule") || !strcmp(t, "point");
}

static int de_small(const char *t) {
  static const struct { const char *s; int v; } a[] = {
    {"null",0},{"ein",1},{"eins",1},{"eine",1},{"einen",1},{"zwei",2},{"drei",3},
    {"vier",4},{"fuenf",5},{"funf",5},{"sechs",6},{"sieben",7},{"acht",8},{"neun",9},
    {"zehn",10},{"elf",11},{"zwoelf",12},{"zwolf",12},{"dreizehn",13},{"vierzehn",14},
    {"fuenfzehn",15},{"funfzehn",15},{"sechzehn",16},{"siebzehn",17},{"achtzehn",18},{"neunzehn",19}
  };
  for (int i = 0; i < (int)(sizeof(a)/sizeof(a[0])); ++i) if (!strcmp(t, a[i].s)) return a[i].v;
  return -1;
}

static int de_tens(const char *t) {
  static const struct { const char *s; int v; } a[] = {
    {"zwanzig",20},{"dreissig",30},{"dreisig",30},{"vierzig",40},{"fuenfzig",50},
    {"funfzig",50},{"sechzig",60},{"siebzig",70},{"achtzig",80},{"neunzig",90}
  };
  for (int i = 0; i < (int)(sizeof(a)/sizeof(a[0])); ++i) if (!strcmp(t, a[i].s)) return a[i].v;
  return -1;
}

static bool de_compound(const char *t, long *value);

static bool de_under100(const char *t, long *value) {
  int v = de_small(t);
  if (v >= 0) { *value = v; return true; }
  v = de_tens(t);
  if (v >= 0) { *value = v; return true; }

  const char *u = strstr(t, "und");
  if (u && u != t && u[3]) {
    char left[TOKEN_SIZE];
    size_t n = (size_t)(u - t);
    if (n >= sizeof(left)) return false;
    memcpy(left, t, n);
    left[n] = '\0';
    int unit = de_small(left), tens = de_tens(u + 3);
    if (unit >= 1 && unit <= 9 && tens >= 20) {
      *value = unit + tens;
      return true;
    }
  }
  return false;
}

static bool de_compound(const char *t, long *value) {
  if (de_under100(t, value)) return true;

  const char *p = strstr(t, "tausend");
  if (p) {
    char left[TOKEN_SIZE], right[TOKEN_SIZE];
    size_t n = (size_t)(p - t);
    if (n >= sizeof(left)) return false;
    memcpy(left, t, n); left[n] = '\0';
    strncpy(right, p + 7, sizeof(right) - 1); right[sizeof(right) - 1] = '\0';
    long a = 1, b = 0;
    if (left[0] && !de_compound(left, &a)) return false;
    if (right[0] && !de_compound(right, &b)) return false;
    *value = a * 1000 + b;
    return true;
  }

  p = strstr(t, "hundert");
  if (p) {
    char left[TOKEN_SIZE], right[TOKEN_SIZE];
    size_t n = (size_t)(p - t);
    if (n >= sizeof(left)) return false;
    memcpy(left, t, n); left[n] = '\0';
    strncpy(right, p + 7, sizeof(right) - 1); right[sizeof(right) - 1] = '\0';
    long a = 1, b = 0;
    if (left[0] && !de_under100(left, &a)) return false;
    if (right[0] && !de_compound(right, &b)) return false;
    *value = a * 100 + b;
    return true;
  }
  return false;
}

static int fr_small(const char *t) {
  static const struct { const char *s; int v; } a[] = {
    {"zero",0},{"un",1},{"une",1},{"deux",2},{"trois",3},{"quatre",4},{"cinq",5},
    {"six",6},{"sept",7},{"huit",8},{"neuf",9},{"dix",10},{"onze",11},{"douze",12},
    {"treize",13},{"quatorze",14},{"quinze",15},{"seize",16},
    {"dixsept",17},{"dixhuit",18},{"dixneuf",19}
  };
  for (int i = 0; i < (int)(sizeof(a)/sizeof(a[0])); ++i) if (!strcmp(t, a[i].s)) return a[i].v;
  return -1;
}

static bool fr_under20(Token t[], int count, int start, int *used, int *value) {
  if (start >= count) return false;
  int v = fr_small(t[start]);
  if (v < 0) return false;
  if (v == 10 && start + 1 < count) {
    int u = fr_small(t[start + 1]);
    if (u >= 7 && u <= 9) { *value = 10 + u; *used = 2; return true; }
  }
  *value = v; *used = 1; return true;
}

static bool fr_under100(Token t[], int count, int start, int *used, int *value) {
  if (start >= count) return false;

  if (!strcmp(t[start], "quatre") && start + 1 < count &&
      (!strcmp(t[start + 1], "vingt") || !strcmp(t[start + 1], "vingts"))) {
    int v = 80, n = 2, j = start + 2, ru = 0, r = 0;
    if (j < count && !strcmp(t[j], "et")) { j++; n++; }
    if (fr_under20(t, count, j, &ru, &r) && r >= 1) { v += r; n += ru; }
    *value = v; *used = n; return true;
  }

  if (!strcmp(t[start], "soixante")) {
    int v = 60, n = 1, j = start + 1, ru = 0, r = 0;
    if (j < count && !strcmp(t[j], "et")) { j++; n++; }
    if (fr_under20(t, count, j, &ru, &r) && r >= 1) { v += r; n += ru; }
    *value = v; *used = n; return true;
  }

  int base = -1;
  if (!strcmp(t[start], "vingt") || !strcmp(t[start], "vingts")) base = 20;
  else if (!strcmp(t[start], "trente")) base = 30;
  else if (!strcmp(t[start], "quarante")) base = 40;
  else if (!strcmp(t[start], "cinquante")) base = 50;

  if (base >= 0) {
    int v = base, n = 1, j = start + 1;
    if (j < count && !strcmp(t[j], "et")) { j++; n++; }
    if (j < count) {
      int u = fr_small(t[j]);
      if (u >= 1 && u <= 9) { v += u; n++; }
    }
    *value = v; *used = n; return true;
  }
  return fr_under20(t, count, start, used, value);
}

static int digit_word(const char *t, ParserLanguage lang) {
  int v = lang == PARSER_LANGUAGE_GERMAN ? de_small(t) : fr_small(t);
  return v >= 0 && v <= 9 ? v : -1;
}

static bool decimal_tail(Token tokens[], int count, int *i, ParserLanguage lang,
                         char *digits, int *len, int max) {
  bool any = false;
  for (; *i < count; ++(*i)) {
    int d = digit_word(tokens[*i], lang);
    if (d >= 0) {
      if (*len >= max - 1) break;
      digits[(*len)++] = (char)('0' + d); digits[*len] = '\0'; any = true; continue;
    }
    if (numeric(tokens[*i])) {
      for (size_t j = 0; tokens[*i][j] && *len < max - 1; ++j) {
        if (isdigit((unsigned char)tokens[*i][j])) {
          digits[(*len)++] = tokens[*i][j]; any = true;
        }
      }
      digits[*len] = '\0';
      continue;
    }
    break;
  }
  return any;
}

static bool word_number(Token tokens[], int count, int start, ParserLanguage lang,
                        int *used, char *number, size_t size) {
  long total = 0, current = 0;
  bool seen = false, negative = false, decimal = false;
  char decimals[16] = "";
  int dlen = 0, i = start;

  if (lang == PARSER_LANGUAGE_GERMAN && i < count && !strcmp(tokens[i], "negativ")) {
    negative = true; i++;
  } else if (lang == PARSER_LANGUAGE_FRENCH && i < count &&
             (!strcmp(tokens[i], "negatif") || !strcmp(tokens[i], "negative"))) {
    negative = true; i++;
  }

  while (i < count) {
    const char *t = tokens[i];
    if (op_for(t, lang) || terminal(t, lang)) break;

    if (decimal_word(t, lang)) {
      if (!seen) break;
      decimal = true; i++;
      if (!decimal_tail(tokens, count, &i, lang, decimals, &dlen, sizeof(decimals))) return false;
      break;
    }

    if (lang == PARSER_LANGUAGE_GERMAN) {
      if (!strcmp(t, "und")) { i++; continue; }
      if (!strcmp(t, "hundert")) {
        current = (current ? current : 1) * 100; seen = true; i++; continue;
      }
      if (!strcmp(t, "tausend")) {
        total += (current ? current : 1) * 1000; current = 0; seen = true; i++; continue;
      }
      if (!strcmp(t, "million") || !strcmp(t, "millionen")) {
        total += (current ? current : 1) * 1000000; current = 0; seen = true; i++; continue;
      }
      long piece = 0;
      if (!de_compound(t, &piece)) break;
      current += piece; seen = true; i++; continue;
    }

    if (!strcmp(t, "mille")) {
      total += (current ? current : 1) * 1000; current = 0; seen = true; i++; continue;
    }
    if (!strcmp(t, "million") || !strcmp(t, "millions")) {
      total += (current ? current : 1) * 1000000; current = 0; seen = true; i++; continue;
    }
    if (!strcmp(t, "cent") || !strcmp(t, "cents")) {
      current = (current ? current : 1) * 100; seen = true; i++; continue;
    }
    int n = 0, v = 0;
    if (!fr_under100(tokens, count, i, &n, &v)) break;
    current += v; seen = true; i += n;
  }

  if (!seen || (decimal && dlen == 0)) return false;
  long whole = total + current;
  if (decimal) snprintf(number, size, "%s%ld.%s", negative ? "-" : "", whole, decimals);
  else snprintf(number, size, "%s%ld", negative ? "-" : "", whole);
  *used = i - start;
  return *used > 0;
}

static bool append_numeric(Token tokens[], int count, int *index, ParserLanguage lang,
                           char *out, size_t size) {
  if (!append_text(out, size, tokens[*index])) return false;
  int i = *index + 1;
  if (i >= count || !decimal_word(tokens[i], lang)) return true;
  if (strchr(tokens[*index], '.') || !append_char(out, size, '.')) return false;
  i++;
  char digits[16] = "";
  int len = 0;
  if (!decimal_tail(tokens, count, &i, lang, digits, &len, sizeof(digits))) return false;
  if (!append_text(out, size, digits)) return false;
  *index = i - 1;
  return true;
}

bool parser_normalize_expression_for_language(const char *input, ParserLanguage lang,
                                              char *out, size_t size) {
  if (lang == PARSER_LANGUAGE_ENGLISH) return parser_normalize_expression(input, out, size);
  if (!input || !out || size < 2 ||
      (lang != PARSER_LANGUAGE_GERMAN && lang != PARSER_LANGUAGE_FRENCH)) return false;

  out[0] = '\0';
  Token tokens[MAX_TOKENS];
  int count = tokenize(input, tokens);
  bool expect_number = true, have_number = false;

  for (int i = 0; i < count; ++i) {
    const char *t = tokens[i];
    if (terminal(t, lang)) break;
    if (filler(t, lang)) continue;

    char op = op_for(t, lang);
    if (op) {
      if (expect_number) {
        if (op == '-' && append_char(out, size, '-')) continue;
        return false;
      }
      if (!append_char(out, size, op)) return false;
      expect_number = true;
      continue;
    }

    if (!expect_number) return false;

    if (numeric(t)) {
      if (!append_numeric(tokens, count, &i, lang, out, size)) return false;
      expect_number = false; have_number = true; continue;
    }

    int used = 0;
    char number[40];
    if (!word_number(tokens, count, i, lang, &used, number, sizeof(number))) return false;
    if (!append_text(out, size, number)) return false;
    i += used - 1;
    expect_number = false;
    have_number = true;
  }
  return have_number && !expect_number && out[0];
}
