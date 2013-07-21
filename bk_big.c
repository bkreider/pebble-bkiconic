#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#define MY_UUID { 0xCA, 0xE7, 0x04, 0xB9, 0x8E, 0x3B, 0x4E, 0x83, 0xB1, 0x61, 0x3E, 0x07, 0x67, 0x1C, 0xA6, 0x86 }
PBL_APP_INFO(MY_UUID,
             "BK Big", "BK Corp",
             1, 1, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

void line_layer_update_callback(Layer *me, GContext* ctx);
void update_time(PblTm *p_time);
void init_text_layer(TextLayer *layer, GRect window_frame, GFont font, GRect frame );

Window window;
TextLayer text_time_layer;
Layer line_layer;

typedef enum {
    none_e         = 0x00,
    top_left_e     = 0x01,
    top_right_e    = 0x02,
    bottom_left_e  = 0x04,
    bottom_right_e = 0x08,
} POSITION;

POSITION position = none_e;
int  minutes = 0;
char time_text[] = "00:00";

void handle_init(AppContextRef ctx) {
  window_init(&window, "BK Big");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorWhite);

  //resource_init_current_app(&APP_RESOURCES);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  init_text_layer(&text_time_layer, window.layer.frame, font, GRect(7, 148, 144-7, 168-148));
  layer_add_child(&window.layer, &text_time_layer.layer);

  layer_init(&line_layer, window.layer.frame);
  line_layer.update_proc = &line_layer_update_callback;
  layer_add_child(&window.layer, &line_layer);

  /* Immediately update the time before tick callbacks are called */
  PblTm current;
  get_time(&current);
  update_time(&current);
}

// Initialize Layers - still need to add to parent
void init_text_layer(TextLayer *layer, GRect window_frame, GFont font, GRect frame ) {
  text_layer_init(layer, window_frame);
  text_layer_set_text_color(layer, GColorBlack);
  text_layer_set_background_color(layer, GColorClear);
  layer_set_frame(&(layer->layer), frame);
  text_layer_set_font(layer, font);
}

// Graphics need to be called via a layer.update_proc
void line_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  // divider lines
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(8, 144), GPoint(131, 144));
  graphics_draw_line(ctx, GPoint(8, 145), GPoint(131, 145));
  graphics_draw_line(ctx, GPoint(8, 146), GPoint(131, 146));

  // White boxes
  uint16_t s       = 57,
           padding = 10,
           hstart  =  0,
           vstart  =  0,
           shading =  0,
           modulo  =  0;
 
  graphics_context_set_fill_color(ctx, GColorBlack);
  if ((position & top_left_e))
    graphics_fill_rect(ctx, GRect(padding, padding,  s, s), 4, GCornersAll);
  if ((position & top_right_e))
    graphics_fill_rect(ctx, GRect(s + (2*padding), padding, s, s), 4, GCornersAll);
  if ((position & bottom_left_e))
    graphics_fill_rect(ctx, GRect(padding,  s + (2 * padding), s, s), 4, GCornersAll);
  if ((position & bottom_right_e))
    graphics_fill_rect(ctx, GRect(s+ (2*padding), s + (2 * padding), s, s), 4, GCornersAll);

  if (minutes < 15) {
    hstart = padding;
    vstart = padding; 
  } else if (minutes < 30) {
    hstart = s + (2 * padding);
    vstart = padding; 
  } else if (minutes < 45) {
    hstart = padding;
    vstart = s + (2 * padding); 
  } else {
    hstart = s + (2 * padding);
    vstart = s + (2 * padding); 
  }

  // lighten time block
  graphics_context_set_stroke_color(ctx, GColorWhite);
  modulo = minutes % 15;

  // Non-linear shading - watch infinite loop with linear shading 0
  // early minutes are very gray
  // Last 5 minutes show 1 white line per minute
  if (modulo < 3)
    shading = 1;
  else if (modulo < 6)
    shading = 2;
  else if (modulo < 9)
    shading = 3;
  else if (modulo < 11)
    shading = 4;
  else if (modulo == 11)
    shading = 14;
  else if (modulo == 12)
    shading = 18;
  else if (modulo == 13)
    shading = 20;
  else if (modulo == 14)
    shading = 29;
  else
    shading = 36;

  for (int i = vstart; i < vstart + s;) {
    graphics_draw_line(ctx, GPoint(hstart, i), GPoint(hstart + s, i));
    i = i + shading;
  }
}

/* Update Watchface function */
void update_time(PblTm *p_time) {
  char *time_format;


  if (p_time->tm_min == 0) {
    position = none_e;
    vibes_short_pulse();
  } else if(p_time->tm_min < 15) {
    position = top_left_e;
  } else if(p_time->tm_min < 30) {
    position = top_left_e | top_right_e;
  } else if(p_time->tm_min < 45) {
    position = top_left_e | top_right_e | bottom_left_e;
  } else if (p_time->tm_min < 60) {
    position = top_left_e | top_right_e | bottom_left_e | bottom_right_e;
  }

  // Save minutes for graphics routines
  minutes = p_time->tm_min;

  layer_mark_dirty(&line_layer);

  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }
  string_format_time(time_text, sizeof(time_text), time_format, p_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&text_time_layer, time_text);
}

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)ctx;
  update_time(t->tick_time);
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
