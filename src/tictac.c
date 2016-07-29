#include <pebble.h>

#define COLORS PBL_IF_COLOR_ELSE(true, false)
#define ANTIALIASING true

#define HAND_MARGIN 8
#define FINAL_RADIUS 60

#define ANIMATION_DURATION 600
#define ANIMATION_DELAY 600

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_hour_layer, *s_minute_layer;

static GPoint s_center;
static Time s_last_time, s_anim_time;

static int background_color, s_radius = 0;

static bool s_animating = false, background_on_conf = false;

/*************************** appMessage **************************/

static void inbox_received_callback(DictionaryIterator *iterator,
                                    void *context) {
  // Read tuples for data
  Tuple *background_color_tuple =
      dict_find(iterator, MESSAGE_KEY_BACKGROUND_COLOR);
  Tuple *background_on_tuple = dict_find(iterator, MESSAGE_KEY_BACKGROUND_ON);

  // If background color and enabled
  if (background_color_tuple && background_on_tuple) {
    // Set background on/off
    background_on_conf = (bool)background_on_tuple->value->int16;
    persist_write_bool(MESSAGE_KEY_BACKGROUND_ON, back‡ground_on_conf);
    // Set background color if enabled, otherwise we load the default one - red
    background_color = (int)background_color_tuple->value->int32;
    persist_write_int(MESSAGE_KEY_BACKGROUND_COLOR, background_color);
  }

  // Redraw
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

/************************************ UI **************************************/

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {

  // Store time
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  // Redraw
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {

  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // the 12 dot
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(bounds.size.w / 2, 20), 6);
  graphics_draw_circle(ctx, GPoint(bounds.size.w / 2, 20), 6);

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if (s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint){
      .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) *
                     (int32_t)(s_radius - HAND_MARGIN + 2) / TRIG_MAX_RATIO) +
           s_center.x,
      .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) *
                     (int32_t)(s_radius - HAND_MARGIN + 2) / TRIG_MAX_RATIO) +
           s_center.y,
  };
  GPoint hour_hand = (GPoint){
      .x = (int16_t)(sin_lookup(hour_angle) *
                     (int32_t)(s_radius - (2 * HAND_MARGIN) - 5) /
                     TRIG_MAX_RATIO) +
           s_center.x,
      .y = (int16_t)(-cos_lookup(hour_angle) *
                     (int32_t)(s_radius - (2 * HAND_MARGIN) - 5) /
                     TRIG_MAX_RATIO) +
           s_center.y,
  };

  graphics_context_set_stroke_width(ctx, 10);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  if (s_radius > HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, minute_hand);
  }

  graphics_context_set_stroke_width(ctx, 10);

  graphics_context_set_stroke_color(ctx, GColorFromHEX(background_color));
  // Draw hands with positive length only
  if (s_radius > 2 * HAND_MARGIN) {
    graphics_draw_line(ctx, s_center, hour_hand);
  }
}

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (
      int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) *
                  (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours =
      anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay,
                    AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if (handlers) {
    animation_set_handlers(anim,
                           (AnimationHandlers){.started = animation_started,
                                               .stopped = animation_stopped},
                           NULL);
  }
  animation_schedule(anim);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);

  s_canvas_layer = layer_create(window_bounds);

  layer_set_update_proc(s_canvas_layer, update_proc);

  layer_add_child(window_layer, s_canvas_layer);

  // Create time Layer
  s_hour_layer =
      text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(0, 0),
                              window_bounds.size.w / 2, window_bounds.size.h));

  s_minute_layer =
      text_layer_create(GRect(window_bounds.size.w / 2, PBL_IF_ROUND_ELSE(0, 0),
                              window_bounds.size.w / 2, window_bounds.size.h));

  // Style the time text
  text_layer_set_background_color(s_hour_layer, GColorClear);
  // text_layer_set_text_color(s_hour_layer, GColorBlueMoon);
  text_layer_set_text_alignment(s_hour_layer, GTextAlignmentCenter);

  text_layer_set_background_color(s_minute_layer, GColorClear);
  // text_layer_set_text_color(s_minute_layer, GColorBlack);
  text_layer_set_text_alignment(s_minute_layer, GTextAlignmentCenter);

  // Add layers
  layer_add_child(window_get_root_layer(window),
                  text_layer_get_layer(s_hour_layer));
  layer_add_child(window_get_root_layer(window),
                  text_layer_get_layer(s_minute_layer));
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  // Destroy weather elements
  text_layer_destroy(s_hour_layer);
  text_layer_destroy(s_minute_layer);
}

/*********************************** App **************************************/

static void init() {
  srand(time(NULL));

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();

  background_on_conf = persist_exists(MESSAGE_KEY_BACKGROUND_ON)
                           ? persist_read_bool(MESSAGE_KEY_BACKGROUND_ON)
                           : false;
  background_color = persist_exists(MESSAGE_KEY_BACKGROUND_COLOR)
                         ? persist_read_int(MESSAGE_KEY_BACKGROUND_COLOR)
                         : 0x0055FF;

  window_set_window_handlers(s_main_window,
                             (WindowHandlers){
                                 .load = window_load, .unload = window_unload,
                             });

  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Prepare animations
  AnimationImplementation radius_impl = {.update = radius_update};
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {.update = hands_update};
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  app_message_open(app_message_inbox_size_maximum(),
                   app_message_outbox_size_maximum());
}

static void deinit() { window_destroy(s_main_window); }

int main() {
  init();
  app_event_loop();
  deinit();
}
