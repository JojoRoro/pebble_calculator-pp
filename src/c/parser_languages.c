#include "parser.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_TOKENS 64
#define TOKEN_SIZE 48

// Word numbers are built in 64-bit (Pebble's long is 32-bit) and capped below 10^15.
#define MAX_SPOKEN 999999999999999LL

typedef char Token[TOKEN_SIZE];

// Returns false when the token does not fit, so a long transcript fails instead of being cut short.
static bool flush(Token tokens[], int *count, char *buf, int *len) {
  if (*len == 0) return true;
  if (*count >= MAX_TOKENS) { *len = 0; return false; }
  buf[*len] = '\0';
  strncpy(tokens[*count], buf, TOKEN_SIZE - 1);
  tokens[*count][TOKEN_SIZE - 1] = '\0';
  (*count)++;
  *len = 0;
  return true;
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

// Dictation may emit typographic math symbols (÷, ×, U+2212 minus) instead of words.
static char unicode_op(const char *input, size_t *n) {
  const unsigned char *s = (const unsigned char *)input;
  if (s[0] == 0xC3 && s[1] == 0xB7) { *n = 2; return '/'; }
  if (s[0] == 0xC3 && s[1] == 0x97) { *n = 2; return '*'; }
  if (s[0] == 0xE2 && s[1] == 0x88 && s[2] == 0x92) { *n = 3; return '-'; }
  return '\0';
}

static bool push_op(Token tokens[], int *count, char op) {
  if (*count >= MAX_TOKENS) return false;
  tokens[*count][0] = op;
  tokens[*count][1] = '\0';
  (*count)++;
  return true;
}

// Returns the token count, or -1 when the transcript does not fit the token buffers.
static int tokenize(const char *input, Token tokens[]) {
  int count = 0, len = 0;
  bool ok = true;
  char buf[TOKEN_SIZE];

  for (size_t i = 0; ok && input && input[i]; ++i) {
    unsigned char raw = (unsigned char)input[i];
    if (raw < 0x80) {
      char c = (char)tolower(raw);
      if (isalnum(raw)) {
        if (len >= TOKEN_SIZE - 1) { ok = false; continue; }
        buf[len++] = c;
        // German writes numbers below a million as one word. Splitting after "tausend" keeps
        // even "siebenhundertsiebenundsiebzigtausendsiebenhundertsiebenundsiebzig" within a
        // token; the number parser already reads a "...tausend" token followed by the rest.
        if (len >= 7 && !memcmp(buf + len - 7, "tausend", 7) && next_alpha(input, i)) {
          ok = flush(tokens, &count, buf, &len);
        }
        continue;
      }
      if (c == '-' && buf_alpha(buf, len) && next_alpha(input, i)) {
        ok = flush(tokens, &count, buf, &len);
        continue;
      }
      if ((c == '.' || c == ',') && isdigit((unsigned char)input[i + 1]) &&
          (len == 0 || buf_numeric(buf, len))) {
        ok = append_piece(buf, &len, ".");
        continue;
      }
      ok = flush(tokens, &count, buf, &len);
      if (ok && (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '=')) {
        ok = push_op(tokens, &count, c);
      }
      continue;
    }

    size_t op_len = 0;
    char op = unicode_op(input + i, &op_len);
    if (op) {
      ok = flush(tokens, &count, buf, &len) && push_op(tokens, &count, op);
      i += op_len - 1;
      continue;
    }

    const char *folded = input[i + 1] ? fold_pair(raw, (unsigned char)input[i + 1]) : NULL;
    if (folded) {
      ok = append_piece(buf, &len, folded);
      i++;
      continue;
    }
    ok = flush(tokens, &count, buf, &len);
    while (input[i + 1] && (((unsigned char)input[i + 1] & 0xC0) == 0x80)) i++;
  }
  if (ok) ok = flush(tokens, &count, buf, &len);
  return ok ? count : -1;
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
  if (!strcmp(t, "^")) return '^';

  if (lang == PARSER_LANGUAGE_GERMAN) {
    if (!strcmp(t, "plus") || !strcmp(t, "addiert") || !strcmp(t, "addiere")) return '+';
    if (!strcmp(t, "minus") || !strcmp(t, "weniger") || !strcmp(t, "subtrahiere")) return '-';
    if (!strcmp(t, "mal") || !strcmp(t, "multipliziert") || !strcmp(t, "multipliziere")) return '*';
    if (!strcmp(t, "durch")) return '/';
    if (!strcmp(t, "hoch")) return '^';
  } else {
    if (!strcmp(t, "plus") || !strcmp(t, "additionne")) return '+';
    if (!strcmp(t, "moins") || !strcmp(t, "soustrais") || !strcmp(t, "soustrait")) return '-';
    if (!strcmp(t, "fois") || !strcmp(t, "multiplie")) return '*';
    if (!strcmp(t, "sur") || !strcmp(t, "divise")) return '/';
    if (!strcmp(t, "puissance") || !strcmp(t, "exposant")) return '^';
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

// Scale words from a thousand up. German and French use the long scale: Milliarde/milliard is
// 10^9 and Billion/billion is 10^12.
static int64_t scale_word(const char *t, ParserLanguage lang) {
  if (lang == PARSER_LANGUAGE_GERMAN) {
    if (!strcmp(t, "tausend")) return 1000LL;
    if (!strcmp(t, "million") || !strcmp(t, "millionen")) return 1000000LL;
    if (!strcmp(t, "milliarde") || !strcmp(t, "milliarden")) return 1000000000LL;
    if (!strcmp(t, "billion") || !strcmp(t, "billionen")) return 1000000000000LL;
    return 0;
  }
  if (!strcmp(t, "mille")) return 1000LL;
  if (!strcmp(t, "million") || !strcmp(t, "millions")) return 1000000LL;
  if (!strcmp(t, "milliard") || !strcmp(t, "milliards")) return 1000000000LL;
  if (!strcmp(t, "billion") || !strcmp(t, "billions")) return 1000000000000LL;
  return 0;
}

// Pebble's printf has no 64-bit integer conversion.
static void format_int(int64_t v, char *out, size_t size) {
  char rev[24];
  int n = 0;
  do { rev[n++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v > 0 && n < (int)sizeof(rev));
  size_t i = 0;
  while (n > 0 && i + 1 < size) out[i++] = rev[--n];
  out[i] = '\0';
}

static bool hundred_word(const char *t, ParserLanguage lang) {
  if (lang == PARSER_LANGUAGE_GERMAN) return !strcmp(t, "hundert");
  return !strcmp(t, "cent") || !strcmp(t, "cents");
}

static int64_t parse_int(const char *s, size_t n) {
  if (n == 0 || n > 15) return -1;
  int64_t v = 0;
  for (size_t i = 0; i < n; ++i) v = v * 10 + (s[i] - '0');
  return v;
}

// "1,5 Milliarden": the scale's zeros absorb decimal digits; any left over stay as the fraction.
static bool scale_decimal(int64_t whole, char *d, int *dlen, int64_t scale, int64_t *out) {
  if (whole > MAX_SPOKEN / scale) return false;
  int64_t v = whole * scale, place = scale;
  int taken = 0;
  while (place > 1 && taken < *dlen) { place /= 10; v += (d[taken++] - '0') * place; }
  memmove(d, d + taken, (size_t)(*dlen - taken) + 1);
  *dlen -= taken;
  *out = v;
  return true;
}

static bool word_number(Token tokens[], int count, int start, ParserLanguage lang,
                        int *used, char *number, size_t size) {
  int64_t total = 0, current = 0;
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
      int64_t ds = i < count ? scale_word(tokens[i], lang) : 0;
      if (ds) {
        int64_t scaled = 0;
        if (!scale_decimal(current, decimals, &dlen, ds, &scaled)) return false;
        total += scaled; current = 0; decimal = dlen > 0; i++;
      }
      break;
    }

    // Digits mixed with scale words: "3 Milliarden", "1,5 billions", "757 milliards 945".
    if (numeric(t) && current == 0) {
      int64_t next_scale = i + 1 < count ? scale_word(tokens[i + 1], lang) : 0;
      bool next_hundred = i + 1 < count && hundred_word(tokens[i + 1], lang);
      bool after_scale = i > start && scale_word(tokens[i - 1], lang);
      const char *dot = strchr(t, '.');
      if (dot && next_scale) {
        current = parse_int(t, (size_t)(dot - t));
        dlen = (int)strlen(dot + 1);
        if (current < 0 || dlen >= (int)sizeof(decimals)) return false;
        memcpy(decimals, dot + 1, (size_t)dlen + 1);
        int64_t scaled = 0;
        if (!scale_decimal(current, decimals, &dlen, next_scale, &scaled)) return false;
        total += scaled; current = 0; decimal = dlen > 0; seen = true; i += 2;
        break;
      }
      if (!dot && (next_scale || next_hundred || after_scale)) {
        current = parse_int(t, strlen(t));
        if (current < 0) return false;
        seen = true; i++; continue;
      }
      break;
    }

    int64_t scale = scale_word(t, lang);
    if (scale) {
      int64_t m = current ? current : 1;
      if (m > MAX_SPOKEN / scale) return false;
      total += m * scale; current = 0; seen = true; i++; continue;
    }

    if (lang == PARSER_LANGUAGE_GERMAN) {
      if (!strcmp(t, "und")) { i++; continue; }
      if (!strcmp(t, "hundert")) {
        if (current > MAX_SPOKEN / 100) return false;
        current = (current ? current : 1) * 100; seen = true; i++; continue;
      }
      long piece = 0;
      if (!de_compound(t, &piece)) break;
      current += piece; seen = true; i++; continue;
    }

    if (!strcmp(t, "cent") || !strcmp(t, "cents")) {
      if (current > MAX_SPOKEN / 100) return false;
      current = (current ? current : 1) * 100; seen = true; i++; continue;
    }
    int n = 0, v = 0;
    if (!fr_under100(tokens, count, i, &n, &v)) break;
    current += v; seen = true; i += n;
  }

  if (!seen || (decimal && dlen == 0)) return false;
  int64_t whole = total + current;
  if (whole > MAX_SPOKEN) return false;
  char digits[24];
  format_int(whole, digits, sizeof(digits));
  if (decimal) snprintf(number, size, "%s%s.%s", negative ? "-" : "", digits, decimals);
  else snprintf(number, size, "%s%s", negative ? "-" : "", digits);
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
  if (count < 0) return false;
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

    bool scaled_digits = i + 1 < count &&
                         (scale_word(tokens[i + 1], lang) || hundred_word(tokens[i + 1], lang));
    if (numeric(t) && !scaled_digits) {
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
