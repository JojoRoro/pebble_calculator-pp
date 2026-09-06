#include <pebble.h>

static Window *window;
static TextLayer *display_layer;

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  display_layer = text_layer_create(GRect(0, 40, bounds.size.w, 50));
  text_layer_set_text_alignment(display_layer, GTextAlignmentCenter);
  text_layer_set_font(display_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(display_layer, "0");

  layer_add_child(window_layer, text_layer_get_layer(display_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(display_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
