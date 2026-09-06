#include <pebble.h>

#include <stdio.h>
#include <string.h>

#include "calculator.h"
#include "parser.h"

#define PERSIST_KEY_VOICE_AUTO_SUBMIT 1
#define PERSIST_KEY_DEBUG_MODE 2
#define PERSIST_KEY_INPUT_LANGUAGE 3
#define DICTATION_BUFFER_SIZE 160

#define TOOLBAR_Y 78
#define TOOLBAR_HEIGHT 30
#define KEYPAD_Y 108
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

#define TOUCH_TARGET_NONE (-1000)
#define TOUCH_TARGET_CLEAR (-1)
#define TOUCH_TARGET_DELETE (-2)
#define TOUCH_TARGET_VOICE (-3)

static Window *s_window;
static Layer *s_canvas_layer;
static DictationSession *s_dictation_session;

static bool s_voice_auto_submit = true;
static bool s_debug_mode = false;
static bool s_debug_overlay_visible = false;
static ParserLanguage s_input_language = PARSER_LANGUAGE_ENGLISH;
static int s_focus_index = -1;
static int s_touch_down_target = TOUCH_TARGET_NONE;
static int s_pressed_target = TOUCH_TARGET_NONE;
static char s_notice[64];
static char s_debug_transcription[DICTATION_BUFFER_SIZE];

static const char *const s_key_labels[KEYPAD_ROWS * KEYPAD_COLS] = {
    "7", "8", "9", "/",
    "4", "5", "6", "x",
    "1", "2", "3", "-",
    "0", ".", "=", "+",
};

static const char *prv_language_short_name(void) {
  switch (s_input_language) {
    case PARSER_LANGUAGE_GERMAN: return "DE";
    case PARSER_LANGUAGE_FRENCH: return "FR";
    default: return "EN";
  }
}

static void prv_mark_dirty(void) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void prv_set_notice(const char *text) {
  snprintf(s_notice, sizeof(s_notice), "%s", text ? text : "");
  prv_mark_dirty();
}

static void prv_set_default_notice(void) {
  snprintf(s_notice, sizeof(s_notice), "%s | Voice: %s | SELECT",
           prv_language_short_name(), s_voice_auto_submit ? "AUTO" : "EDIT");
  prv_mark_dirty();
}

static void prv_hide_debug_overlay(void) {
  if (!s_debug_overlay_visible) {
    return;
  }
  s_debug_overlay_visible = false;
  s_debug_transcription[0] = '\0';
  prv_mark_dirty();
}

static void prv_show_debug_transcript(const char *transcription) {
  if (!s_debug_mode || !transcription) {
    return;
  }
  snprintf(s_debug_transcription, sizeof(s_debug_transcription), "%s", transcription);
  s_debug_overlay_visible = true;
  s_touch_down_target = TOUCH_TARGET_NONE;
  s_pressed_target = TOUCH_TARGET_NONE;
  prv_mark_dirty();
}

static void prv_draw_button(GContext *ctx, GRect rect, const char *label, bool emphasized,
                            bool focused, bool pressed, bool small_font) {
  const bool dark = emphasized || pressed;
  graphics_context_set_fill_color(ctx, dark ? GColorBlack : GColorWhite);
  graphics_fill_rect(ctx, rect, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_rect(ctx, rect);

  if (focused) {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    GRect inner = GRect(rect.origin.x + 1, rect.origin.y + 1,
                        rect.size.w - 2, rect.size.h - 2);
    graphics_draw_rect(ctx, inner);
  }

  graphics_context_set_text_color(ctx, dark ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, label,
                     fonts_get_system_font(small_font ? FONT_KEY_GOTHIC_18_BOLD
                                                      : FONT_KEY_GOTHIC_24_BOLD),
                     GRect(rect.origin.x, rect.origin.y - 1, rect.size.w, rect.size.h + 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void prv_draw_debug_overlay(GContext *ctx, GRect bounds) {
  const GRect panel = GRect(7, 18, bounds.size.w - 14, bounds.size.h - 34);
  const GRect transcript_box = GRect(13, 63, bounds.size.w - 26, bounds.size.h - 101);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, panel, 3, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, panel);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "VOICE DEBUG",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(12, 23, bounds.size.w - 24, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, "Parser failed - raw transcript:",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(12, 45, bounds.size.w - 24, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, transcript_box, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_rect(ctx, transcript_box);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_debug_transcription,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(transcript_box.origin.x + 4, transcript_box.origin.y + 3,
                           transcript_box.size.w - 8, transcript_box.size.h - 6),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, "BACK to dismiss",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(12, bounds.size.h - 31, bounds.size.w - 24, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const char *expression = calculator_expression();
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, expression[0] ? expression : "0",
                     fonts_get_system_font(FONT_KEY_GOTHIC_24),
                     GRect(4, 2, bounds.size.w - 8, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  char result_line[CALCULATOR_RESULT_MAX + 4];
  result_line[0] = '\0';
  if (calculator_has_error()) {
    snprintf(result_line, sizeof(result_line), "%s", calculator_result());
  } else if (calculator_has_result()) {
    snprintf(result_line, sizeof(result_line), "= %s", calculator_result());
  }

  graphics_draw_text(ctx, result_line,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(4, 31, bounds.size.w - 8, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, s_notice,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(4, 59, bounds.size.w - 8, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  const int toolbar_widths[] = {67, 66, 67};
  const int toolbar_targets[] = {TOUCH_TARGET_CLEAR, TOUCH_TARGET_DELETE, TOUCH_TARGET_VOICE};
  const char *const toolbar_labels[] = {"C", "DEL", "VOICE"};
  int toolbar_x = 0;
  for (int i = 0; i < 3; ++i) {
    GRect rect = GRect(toolbar_x, TOOLBAR_Y, toolbar_widths[i], TOOLBAR_HEIGHT);
    const int target = toolbar_targets[i];
    prv_draw_button(ctx, rect, toolbar_labels[i], target == TOUCH_TARGET_VOICE,
                    false, s_pressed_target == target, true);
    toolbar_x += toolbar_widths[i];
  }

  const int key_width = bounds.size.w / KEYPAD_COLS;
  const int key_height = (bounds.size.h - KEYPAD_Y) / KEYPAD_ROWS;
  for (int row = 0; row < KEYPAD_ROWS; ++row) {
    for (int col = 0; col < KEYPAD_COLS; ++col) {
      const int index = row * KEYPAD_COLS + col;
      GRect rect = GRect(col * key_width, KEYPAD_Y + row * key_height,
                         key_width, key_height);
      const char *label = s_key_labels[index];
      const bool emphasized = !strcmp(label, "=");
      prv_draw_button(ctx, rect, label, emphasized,
                      index == s_focus_index, s_pressed_target == index, false);
    }
  }

  if (s_debug_overlay_visible) {
    prv_draw_debug_overlay(ctx, bounds);
  }
}

static void prv_activate_key(int index) {
  if (s_debug_overlay_visible || index < 0 || index >= KEYPAD_ROWS * KEYPAD_COLS) {
    return;
  }

  const char *label = s_key_labels[index];
  if (label[0] >= '0' && label[0] <= '9' && label[1] == '\0') {
    calculator_append_digit(label[0]);
  } else if (!strcmp(label, ".")) {
    calculator_append_decimal();
  } else if (!strcmp(label, "=")) {
    (void)calculator_equals();
  } else if (!strcmp(label, "x")) {
    calculator_append_operator('*');
  } else if (!strcmp(label, "/")) {
    calculator_append_operator('/');
  } else if (!strcmp(label, "+")) {
    calculator_append_operator('+');
  } else if (!strcmp(label, "-")) {
    calculator_append_operator('-');
  }

  prv_set_default_notice();
}

static void prv_dictation_callback(DictationSession *session, DictationSessionStatus status,
                                   char *transcription, void *context) {
  (void)session;
  (void)context;

  if (status != DictationSessionStatusSuccess || !transcription) {
    switch (status) {
      case DictationSessionStatusFailureConnectivityError:
        prv_set_notice("Voice needs phone connectivity");
        break;
      case DictationSessionStatusFailureNoSpeechDetected:
        prv_set_notice("No speech detected");
        break;
      case DictationSessionStatusFailureDisabled:
        prv_set_notice("Voice dictation is disabled");
        break;
      default:
        prv_set_notice("Voice input cancelled/failed");
        break;
    }
    return;
  }

  char expression[CALCULATOR_EXPRESSION_MAX];
  if (!parser_normalize_expression_for_language(transcription, s_input_language,
                                                expression, sizeof(expression)) ||
      !calculator_set_expression(expression)) {
    prv_set_notice("Couldn't parse that calculation");
    prv_show_debug_transcript(transcription);
    return;
  }

  if (s_voice_auto_submit) {
    (void)calculator_equals();
    prv_set_notice("Voice calculated");
  } else {
    prv_set_notice("Voice ready - edit or press =");
  }
}

static void prv_start_voice(void) {
  if (s_debug_overlay_visible) {
    return;
  }

  if (!s_dictation_session) {
    s_dictation_session = dictation_session_create(DICTATION_BUFFER_SIZE,
                                                   prv_dictation_callback, NULL);
  }

  if (!s_dictation_session) {
    prv_set_notice("Voice unavailable - check phone");
    return;
  }

  dictation_session_enable_confirmation(s_dictation_session, !s_voice_auto_submit);
  (void)dictation_session_start(s_dictation_session);
}

static void prv_activate_target(int target) {
  if (s_debug_overlay_visible) {
    return;
  }

  if (target >= 0) {
    s_focus_index = target;
    prv_activate_key(target);
    return;
  }

  switch (target) {
    case TOUCH_TARGET_CLEAR:
      calculator_clear();
      prv_set_default_notice();
      break;
    case TOUCH_TARGET_DELETE:
      calculator_backspace();
      prv_set_default_notice();
      break;
    case TOUCH_TARGET_VOICE:
      prv_start_voice();
      break;
    default:
      break;
  }
}

static int prv_touch_target_for_point(int16_t x, int16_t y) {
  if (x < 0 || x >= 200 || y < 0 || y >= 228) {
    return TOUCH_TARGET_NONE;
  }

  if (y >= TOOLBAR_Y && y < TOOLBAR_Y + TOOLBAR_HEIGHT) {
    if (x < 67) return TOUCH_TARGET_CLEAR;
    if (x < 133) return TOUCH_TARGET_DELETE;
    return TOUCH_TARGET_VOICE;
  }

  if (y >= KEYPAD_Y) {
    const int col = x / 50;
    const int row = (y - KEYPAD_Y) / 30;
    if (row >= 0 && row < KEYPAD_ROWS && col >= 0 && col < KEYPAD_COLS) {
      return row * KEYPAD_COLS + col;
    }
  }
  return TOUCH_TARGET_NONE;
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  if (!event || s_debug_overlay_visible) {
    return;
  }

  const int target = prv_touch_target_for_point(event->x, event->y);
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touch_down_target = target;
      s_pressed_target = target;
      prv_mark_dirty();
      break;
    case TouchEvent_PositionUpdate:
      s_pressed_target = (target == s_touch_down_target) ? target : TOUCH_TARGET_NONE;
      prv_mark_dirty();
      break;
    case TouchEvent_Liftoff: {
      const bool activate = target != TOUCH_TARGET_NONE && target == s_touch_down_target;
      s_pressed_target = TOUCH_TARGET_NONE;
      s_touch_down_target = TOUCH_TARGET_NONE;
      if (activate) {
        prv_activate_target(target);
      }
      prv_mark_dirty();
      break;
    }
  }
}

static void prv_move_focus(int delta) {
  if (s_debug_overlay_visible) {
    return;
  }

  const int count = KEYPAD_ROWS * KEYPAD_COLS;
  if (s_focus_index < 0) {
    s_focus_index = delta > 0 ? 0 : count - 1;
  } else {
    s_focus_index = (s_focus_index + delta + count) % count;
  }
  prv_mark_dirty();
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  prv_move_focus(-1);
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  prv_move_focus(1);
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_focus_index < 0) {
    prv_start_voice();
    return;
  }
  prv_activate_key(s_focus_index);
}

static void prv_select_long_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  prv_start_voice();
}

static void prv_back_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_debug_overlay_visible) {
    prv_hide_debug_overlay();
    return;
  }

  if (!calculator_expression()[0] && !calculator_result()[0]) {
    window_stack_pop(true);
    return;
  }

  calculator_backspace();
  prv_set_default_notice();
}

static void prv_back_long_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;

  if (s_debug_overlay_visible) {
    prv_hide_debug_overlay();
    return;
  }

  calculator_clear();
  prv_set_default_notice();
}

static void prv_click_config_provider(void *context) {
  (void)context;
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 160, prv_up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 160, prv_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 650, prv_select_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click);
  window_long_click_subscribe(BUTTON_ID_BACK, 700, prv_back_long_click, NULL);
}

static ParserLanguage prv_language_from_tuple(const Tuple *tuple) {
  if (!tuple) {
    return s_input_language;
  }

  if (tuple->type == TUPLE_CSTRING) {
    if (!strcmp(tuple->value->cstring, "de")) return PARSER_LANGUAGE_GERMAN;
    if (!strcmp(tuple->value->cstring, "fr")) return PARSER_LANGUAGE_FRENCH;
    return PARSER_LANGUAGE_ENGLISH;
  }

  int32_t value = tuple->value->int32;
  if (value == PARSER_LANGUAGE_GERMAN) return PARSER_LANGUAGE_GERMAN;
  if (value == PARSER_LANGUAGE_FRENCH) return PARSER_LANGUAGE_FRENCH;
  return PARSER_LANGUAGE_ENGLISH;
}

static void prv_inbox_received(DictionaryIterator *iterator, void *context) {
  (void)context;
  bool settings_changed = false;

  Tuple *voice_mode = dict_find(iterator, MESSAGE_KEY_VoiceAutoSubmit);
  if (voice_mode) {
    s_voice_auto_submit = voice_mode->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_VOICE_AUTO_SUBMIT, s_voice_auto_submit);
    settings_changed = true;
  }

  Tuple *debug_mode = dict_find(iterator, MESSAGE_KEY_DebugMode);
  if (debug_mode) {
    s_debug_mode = debug_mode->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_DEBUG_MODE, s_debug_mode);
    if (!s_debug_mode) {
      prv_hide_debug_overlay();
    }
    settings_changed = true;
  }

  Tuple *language = dict_find(iterator, MESSAGE_KEY_InputLanguage);
  if (language) {
    s_input_language = prv_language_from_tuple(language);
    persist_write_int(PERSIST_KEY_INPUT_LANGUAGE, (int)s_input_language);
    settings_changed = true;
  }

  if (settings_changed) {
    prv_set_default_notice();
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, prv_canvas_update_proc);
  layer_add_child(root, s_canvas_layer);

  window_set_click_config_provider(window, prv_click_config_provider);
}

static void prv_window_unload(Window *window) {
  (void)window;
  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
}

static void prv_init(void) {
  calculator_clear();
  s_voice_auto_submit = persist_exists(PERSIST_KEY_VOICE_AUTO_SUBMIT)
                            ? persist_read_bool(PERSIST_KEY_VOICE_AUTO_SUBMIT)
                            : true;
  s_debug_mode = persist_exists(PERSIST_KEY_DEBUG_MODE)
                     ? persist_read_bool(PERSIST_KEY_DEBUG_MODE)
                     : false;

  int saved_language = persist_exists(PERSIST_KEY_INPUT_LANGUAGE)
                           ? persist_read_int(PERSIST_KEY_INPUT_LANGUAGE)
                           : PARSER_LANGUAGE_ENGLISH;
  if (saved_language < PARSER_LANGUAGE_ENGLISH || saved_language > PARSER_LANGUAGE_FRENCH) {
    saved_language = PARSER_LANGUAGE_ENGLISH;
  }
  s_input_language = (ParserLanguage)saved_language;

  s_debug_transcription[0] = '\0';
  s_focus_index = -1;
  prv_set_default_notice();

  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  touch_service_subscribe(prv_touch_handler, NULL);

  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(64, 64);

  if (!touch_service_is_enabled()) {
    prv_set_notice("Touch unavailable - use buttons");
  }
}

static void prv_deinit(void) {
  touch_service_unsubscribe();
  if (s_dictation_session) {
    dictation_session_destroy(s_dictation_session);
  }
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
