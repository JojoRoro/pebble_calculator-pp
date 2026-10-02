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

  // Newer dictation hyphenates compound number words; the hyphen is not a minus sign.
  check_voice("forty-five divided by one hundred eighty-seven", "45/187", "0.2406417112");
  check_voice("Forty-five divided by one hundred and eighty-seven.", "45/187", "0.2406417112");
  check_voice("twenty-two minus seventy-one", "22-71", "-49");
  check_voice("one-hundred-eighty-seven minus five", "187-5", "182");
  check_voice("ninety-nine thousand nine hundred ninety-nine plus one", "99999+1", "100000");
  check_voice("forty-five divided by a hundred eighty-seven", "45/187", "0.2406417112");
  check_voice("45 - 5", "45-5", "40");
  check_voice("45-5", "45-5", "40");
  check_voice("forty five - five", "45-5", "40");
  check_voice("45 \xC3\xB7 187", "45/187", "0.2406417112");
  check_voice("6 \xC3\x97 7", "6*7", "42");
  check_voice("10 \xE2\x88\x92 3", "10-3", "7");
  // Scale words up to trillion; values beyond Pebble's 32-bit long must not overflow.
  check_voice("Seven hundred and fifty seven billion nine hundred and forty five divided by "
              "two hundred eighty seven thousand.",
              "757000000945/287000", "2637630.665");
  check_voice("three billion plus one", "3000000000+1", "3000000001");
  check_voice("a billion minus a million", "1000000000-1000000", "999000000");
  check_voice("two trillion five hundred billion divided by four", "2500000000000/4",
              "6.25e+11");
  check_voice("nine hundred ninety-nine trillion plus one", "999000000000000+1", "9.99e+14");
  check_voice("one point five billion plus one", "1500000000+1", "1500000001");
  check_voice("1.5 billion plus 1", "1500000000+1", "1500000001");
  check_voice("3 billion plus 1", "3000000000+1", "3000000001");
  check_voice("757 billion 945 divided by 287 thousand", "757000000945/287000", "2637630.665");
  check_voice("one point two three four five thousand", "1234.5", "1234.5");
  char rejected[CALCULATOR_EXPRESSION_MAX];
  assert(!parser_normalize_expression("fifteen hundred trillion", rejected, sizeof(rejected)));
  assert(!parser_normalize_expression("nine hundred hundred hundred hundred hundred hundred "
                                      "hundred hundred hundred", rejected, sizeof(rejected)));
  assert(!parser_normalize_expression("one point billion", rejected, sizeof(rejected)));
  check_voice_language(PARSER_LANGUAGE_GERMAN, "drei Milliarden plus eins.", "3000000000+1",
                       "3000000001");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "zwei Billionen geteilt durch zwei.",
                       "2000000000000/2", "1e+12");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "1,5 Milliarden plus 1.", "1500000000+1",
                       "1500000001");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "trois milliards plus un.", "3000000000+1",
                       "3000000001");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "un virgule cinq milliard plus un.",
                       "1500000000+1", "1500000001");

  // Powers: "to the power of", "squared", "hoch", "puissance".
  check_voice("Ten to the power of forty-five divided by two.", "10^45/2", "5e+44");
  check_voice("2 raised to the power of 10", "2^10", "1024");
  check_voice("ten to the power of minus two", "10^-2", "0.01");
  check_voice("five squared plus four cubed", "5^2+4^3", "89");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "zehn hoch 2", "10^2", "100");
  check_voice_language(PARSER_LANGUAGE_GERMAN, "zwei hoch zehn geteilt durch vier.", "2^10/4",
                       "256");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "deux à la puissance dix.", "2^10", "1024");
  check_expression("2^3^2", "512");
  check_expression("-2^2", "-4");
  check_expression("(-2)^2", "4");
  check_expression("2*3^2", "18");
  check_expression("2^-2", "0.25");
  check_expression("0^0", "1");
  assert(calculator_set_expression("2^0.5"));
  assert(!calculator_equals());
  assert(calculator_set_expression("0^-1"));
  assert(!calculator_equals());
  assert(strcmp(calculator_result(), "Division by zero") == 0);
  assert(calculator_set_expression("10^400-10^400"));
  assert(!calculator_equals());
  assert(strcmp(calculator_result(), "Result out of range") == 0);

  check_voice_language(PARSER_LANGUAGE_GERMAN, "45 \xC3\xB7 9", "45/9", "5");
  check_voice_language(PARSER_LANGUAGE_FRENCH, "6 \xC3\x97 7", "6*7", "42");

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
