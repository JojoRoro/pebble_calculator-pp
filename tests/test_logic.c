#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/calculator.h"
#include "../src/c/parser.h"

static void check_expression(const char *expression, const char *result) {
  assert(calculator_set_expression(expression));
  assert(calculator_equals());
  assert(strcmp(calculator_result(), result) == 0);
}

static void check_voice_language(ParserLanguage language, const char *speech,
                                 const char *expression, const char *result) {
  char parsed[CALCULATOR_EXPRESSION_MAX];
  assert(parser_normalize_expression_for_language(speech, language, parsed, sizeof(parsed)));
  assert(strcmp(parsed, expression) == 0);
  check_expression(parsed, result);
}

static void check_voice(const char *speech, const char *expression, const char *result) {
  check_voice_language(PARSER_LANGUAGE_ENGLISH, speech, expression, result);
}

int main(void) {
  check_voice("22 divided by 55 plus 8 equals", "22/55+8", "8.4");
  check_voice("twenty two divided by fifty five plus eight", "22/55+8", "8.4");
  check_voice("10 times 5", "10*5", "50");
  check_voice("one hundred minus twenty", "100-20", "80");
  check_voice("fifty point five plus two", "50.5+2", "52.5");
  check_voice("negative five plus two", "-5+2", "-3");
  check_voice("2 plus 3 times 4", "2+3*4", "14");

  // Pebble dictation commonly adds sentence punctuation to an otherwise valid transcript.
  check_voice("Five over ten.", "5/10", "0.5");
  check_voice("55 plus 17.", "55+17", "72");
  check_voice("fifty point five plus two.", "50.5+2", "52.5");
  check_voice("5.5 over 10.", "5.5/10", "0.55");

  // German has inverted compound numbers and commonly writes them as one word.
  check_voice_language(PARSER_LANGUAGE_GERMAN, "fünf plus zehn.", "5+10", "15");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "fünf geteilt durch zehn.", "5/10", "0.5");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "fünf mal zehn.", "5*10", "50");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "fünfundfünfzig geteilt durch sieben.",
                       "55/7", "7.857142857");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "einundzwanzig plus neun.", "21+9", "30");
  check_voice_language(PARSER_LANGUAGE_GERMAN,
                       "einhundertfünfundzwanzig minus fünfundzwanzig.",
                       "125-25", "100");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "zwei komma fünf mal vier.", "2.5*4", "10");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "minus fünf plus zwei.", "-5+2", "-3");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "5,5 plus 2.", "5.5+2", "7.5");

  // French number grammar includes hyphenated forms and 70/80/90 constructions.
  check_voice_language(PARSER_LANGUAGE_FRENCH, "cinq plus dix.", "5+10", "15");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "cinq divisé par dix.", "5/10", "0.5");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "cinq sur dix.", "5/10", "0.5");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "cinquante-cinq divisé par sept.",
                       "55/7", "7.857142857");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "vingt et un plus neuf.", "21+9", "30");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "quatre-vingt-dix plus dix.", "90+10", "100");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "cent vingt-cinq moins vingt-cinq.",
                       "125-25", "100");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "deux virgule cinq fois quatre.", "2.5*4", "10");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "moins cinq plus deux.", "-5+2", "-3");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "5,5 plus 2.", "5.5+2", "7.5");

  // Results must not depend on printf floating-point formatting, which Pebble omits.
  check_expression("5+10", "15");
  check_expression("55/7", "7.857142857");
  check_expression("1/8", "0.125");
  check_expression("0.1+0.2", "0.3");

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
