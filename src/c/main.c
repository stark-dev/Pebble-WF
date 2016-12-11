#include <pebble.h>

#define COLORS PBL_IF_COLOR_ELSE(true, false)
#define ANTIALIASING true
#define RADIUS_MARGIN 15
#define DIAL_DOTS_MARGIN 9
#define BLACK_DIAL_MARGIN 18
#define BATT_DOTS_MARGIN 25
#define ANIMATION_DURATION 500
#define ANIMATION_DELAY 600

typedef struct {
  uint8_t hours;
  uint8_t minutes;
} Time;

//Windows
static Window *s_main_window;
//Layers
static Layer *s_canvas_layer, * s_date_layer;
static TextLayer *s_num_label;
//UI points
static GPoint s_center;
//Time
static Time s_last_time, s_anim_time;
static char s_num_buffer[4];
//Animation
static uint8_t s_gray_dial_radius = 0;
static uint8_t s_dial_dots_radius = 0;
static uint8_t s_black_dial_radius = 0;
static uint8_t s_bt_conn_dots_radius = 0;
static uint8_t s_hand_lenght = 0; 

static uint8_t s_gray_dial_radius_f;
static uint8_t s_dial_dots_radius_f;
static uint8_t s_black_dial_radius_f;
static uint8_t s_bt_conn_dots_radius_f;
static uint8_t s_hand_lenght_f;

static uint8_t s_anim_hours_60 = 0;
static bool s_animating = false;

//connection and battery
static uint8_t s_battery_level;
static bool s_charging;
static bool s_bt_connected;

//dots params
static uint8_t dot_width = 1;
/******************************** Battery Level *******************************/

static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
  s_battery_level = state.charge_percent;
  s_charging = state.is_charging;
  
  if(s_battery_level == 10 && !s_charging)
    vibes_double_pulse();

  // Update meter
  layer_mark_dirty(s_canvas_layer);
}

/******************************* BluetoothStatus ******************************/

static void bluetooth_callback(bool connected) {
  s_bt_connected = connected;
  vibes_double_pulse();

  // Update meter
  layer_mark_dirty(s_canvas_layer);
}

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if (handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = (tick_time->tm_hour > 12) ? tick_time->tm_hour- 12 : tick_time->tm_hour;
  s_last_time.minutes = tick_time->tm_min;

  // Redraw
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int hours_to_minutes(int hours_out_of_12) {
  return hours_out_of_12 * 60 / 12;
}

static void update_proc(Layer *layer, GContext *ctx) {
  GRect full_bounds = layer_get_bounds(layer);
  GRect bounds = layer_get_unobstructed_bounds(layer);
  s_center = grect_center_point(&bounds);
  
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, full_bounds, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 10);

  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // Gray clockface
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_circle(ctx, s_center, s_gray_dial_radius);
  
  //dial dots
  for(int i=0; i<60; i++){
    // Plot dots
    GPoint dot_centre = (GPoint) {
      .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * i / 60) * (int32_t)(s_dial_dots_radius) / TRIG_MAX_RATIO) + s_center.x,
      .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * i / 60) * (int32_t)(s_dial_dots_radius) / TRIG_MAX_RATIO) + s_center.y,
    };
    
    // Draw dots with positive length only
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_fill_color(ctx, GColorWhite);
    if(i%15 == 0)
      dot_width = 3;
    else if(i%5 == 0)
      dot_width = 2;
    else
      dot_width = 1;
    
    graphics_fill_circle(ctx, dot_centre, dot_width);
  }
  
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
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_hand_lenght) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_hand_lenght) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_hand_lenght) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_hand_lenght) / TRIG_MAX_RATIO) + s_center.y,
  };  

  // Draw hands with positive length only
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_line(ctx, s_center, hour_hand);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_line(ctx, s_center, minute_hand);
  
  // Black dial
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, s_center, s_black_dial_radius);
  
  //battery dots
  for(int i=0; i<5; i++){
    // Plot dots
    GPoint dot_centre = (GPoint) {
      .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * (20 + i*2) / 60) * (int32_t)(s_bt_conn_dots_radius) / TRIG_MAX_RATIO) + s_center.x,
      .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * (20 + i*2) / 60) * (int32_t)(s_bt_conn_dots_radius) / TRIG_MAX_RATIO) + s_center.y,
    };
  
    // Draw dots with positive length only
    if(i > ((s_battery_level + 10)/20 - 1)){
      graphics_context_set_fill_color(ctx, GColorDarkGreen);
      graphics_fill_circle(ctx, dot_centre, 1);
    }
    else {
      if(s_charging)
        graphics_context_set_fill_color(ctx, GColorYellow);
      else
        graphics_context_set_fill_color(ctx, GColorGreen);
      graphics_fill_circle(ctx, dot_centre, 2);
    }
  }
  
  //bluetooth dot
  GPoint dot_centre = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * (30) / 60) * (int32_t)(s_bt_conn_dots_radius) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * (30) / 60) * (int32_t)(s_bt_conn_dots_radius) / TRIG_MAX_RATIO) + s_center.y,
  };
  
  if(s_bt_connected){
    graphics_context_set_fill_color(ctx, GColorBlue);
    graphics_fill_circle(ctx, dot_centre, 2);
  }
  else {
    graphics_context_set_fill_color(ctx, GColorRed);
    graphics_fill_circle(ctx, dot_centre, 2);
  }
  
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
}

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(dist_normalized * max / ANIMATION_NORMALIZED_MAX);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_gray_dial_radius = anim_percentage(dist_normalized, s_gray_dial_radius_f);
  s_dial_dots_radius = anim_percentage(dist_normalized, s_dial_dots_radius_f);
  s_black_dial_radius = anim_percentage(dist_normalized, s_black_dial_radius_f);
  s_bt_conn_dots_radius = anim_percentage(dist_normalized, s_bt_conn_dots_radius_f);
  s_hand_lenght = anim_percentage(dist_normalized, s_hand_lenght_f);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}

static void start_animation() {
  // Prepare animations
  static AnimationImplementation s_radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &s_radius_impl, false);

  static AnimationImplementation s_hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &s_hands_impl, true);
}

static void create_canvas() {
  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect bounds = layer_get_unobstructed_bounds(window_layer);
  
  //set dial width
  s_gray_dial_radius_f = (bounds.size.w - RADIUS_MARGIN) / 2;
  //set dial dots witdh
  s_dial_dots_radius_f = s_gray_dial_radius_f - DIAL_DOTS_MARGIN;
  //set black dial width
  s_black_dial_radius_f = s_gray_dial_radius_f - BLACK_DIAL_MARGIN;
  //set battery dots width
  s_bt_conn_dots_radius_f = s_gray_dial_radius_f - BATT_DOTS_MARGIN;
  //set hand lenght
  s_hand_lenght_f = s_gray_dial_radius_f;

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);

  s_num_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(90, 114, 18, 20),
    GRect(100, 72, 18, 20)));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorClear);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));

  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));
  
}

/*********************************** App **************************************/

// Event fires once, before the obstruction appears or disappears
static void unobstructed_will_change(GRect final_unobstructed_screen_area, void *context) {
  if(s_animating) {
    return;
  }
  // Reset the clock animation
  s_gray_dial_radius = 0;
  s_dial_dots_radius = 0;
  s_black_dial_radius = 0;
  s_bt_conn_dots_radius = 0;
  s_hand_lenght = 0; 
  
  s_anim_hours_60 = 0;
}

// Event fires once, after obstruction appears or disappears
static void unobstructed_did_change(void *context) {
  if(s_animating) {
    return;
  }
  // Play the clock animation
  start_animation();
}

static void window_load(Window *window) {
  create_canvas();

  start_animation();

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Subscribe to the unobstructed area events
  UnobstructedAreaHandlers handlers = {
    .will_change = unobstructed_will_change,
    .did_change = unobstructed_did_change
  };
  unobstructed_area_service_subscribe(handlers, NULL);  
}

static void window_unload(Window *window) {
  text_layer_destroy(s_num_label);
  layer_destroy(s_date_layer);
  layer_destroy(s_canvas_layer);
}

static void init() {
  srand(time(NULL));

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);
  
  // Register for battery level updates
  battery_state_service_subscribe(battery_callback);
  // Ensure battery level is displayed from the start
  battery_callback(battery_state_service_peek());
  
  // Register for bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  // Ensure bluetooth connection is displayed from the start
  bluetooth_callback(connection_service_peek_pebble_app_connection());
}

static void deinit() {
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}