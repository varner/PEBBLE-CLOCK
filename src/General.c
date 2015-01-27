#include "pebble.h"
  
#define MATH_PI 3.141592653589793238462
#define NUM_DISCS 59
#define DISC_DENSITY 0.25
#define ACCEL_RATIO 0.05
#define ACCEL_STEP_MS 50
  
//declare variable, use local time.  
int tt;
bool min = true;

typedef struct Vec2d {
  double x;
  double y;
} Vec2d;

typedef struct Disc {
  Vec2d pos;
  Vec2d vel;
  double mass;
  double radius;
} Disc;

static Disc discs[NUM_DISCS];

static double next_radius = 10;
static double count = 0;

static Window *window;

static GRect window_frame;

static Layer *disc_layer;

static AppTimer *timer;

static double disc_calc_mass(Disc *disc) {
  return MATH_PI * disc->radius * disc->radius * DISC_DENSITY;
}

static void disc_init(Disc *disc) {
  GRect frame = window_frame;
  disc->pos.x = frame.size.w/2;
  disc->pos.y = frame.size.h/2;
  disc->vel.x = 0;
  disc->vel.y = 0;
  disc->radius = 2;
  disc->mass = disc_calc_mass(disc) + next_radius;
  next_radius += 0.5;
  count++;
}

static void disc_apply_force(Disc *disc, Vec2d force) {
  disc->vel.x += force.x / disc->mass;
  disc->vel.y += force.y / disc->mass;
}

static void disc_apply_accel(Disc *disc, AccelData accel) {
  Vec2d force;
  force.x = accel.x * ACCEL_RATIO;
  force.y = -accel.y * ACCEL_RATIO;
  disc_apply_force(disc, force);
}

static void disc_update(Disc *disc) {
  const GRect frame = window_frame;
  double e = 0.5;
  if ((disc->pos.x - disc->radius < 0 && disc->vel.x < 0)
    || (disc->pos.x + disc->radius > frame.size.w && disc->vel.x > 0)) {
    disc->vel.x = -disc->vel.x * e;
  }
  if ((disc->pos.y - disc->radius < 0 && disc->vel.y < 0)
    || (disc->pos.y + disc->radius > frame.size.h && disc->vel.y > 0)) {
    disc->vel.y = -disc->vel.y * e;
  }
  disc->pos.x += disc->vel.x;
  disc->pos.y += disc->vel.y;
}

static void disc_draw(GContext *ctx, Disc *disc) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(disc->pos.x, disc->pos.y), disc->radius);
}

static void disc_layer_update_callback(Layer *me, GContext *ctx) {
  for (int i = 0; i < tt; i++) {
    disc_draw(ctx, &discs[i]);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
 // text_layer_set_text(s_output_layer, "Up pressed!");
  min = !min;
  int new_rad;
  if (min == true) {
    new_rad = 2;
  } else {
    new_rad = 10;
  }
  
  for (int i = 0; i < NUM_DISCS; i++) {
    Disc *disc = &discs[i];
    disc->radius = new_rad;
  }
}

static void click_config_provider(void *context) {
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
}

static bool disc_check_collisions(Disc *disc, Disc *disc_2) {
  double dx = disc_2->pos.x - disc->pos.x;
  double dy = disc_2->pos.y - disc->pos.y;
  double change = (dx * dx) + (dy * dy);
  if (change < disc->radius * disc->radius) return true;
  return false;
}

static void timer_callback(void *data) {
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };

  accel_service_peek(&accel);
  
  for (int i = 0; i < tt; i++) {
    Disc *disc = &discs[i];
    disc_apply_accel(disc, accel);
    for (int j = i + 1; j < tt; j++) {
      Disc *disc_2 = &discs[j];
      if (disc_check_collisions(disc, disc_2)) {
        disc->pos.x -= disc->vel.x;
        disc->pos.x -= disc->vel.y;
      }
    }
    disc_update(disc);
  }

  layer_mark_dirty(disc_layer);

  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tm;
  tm=localtime(&temp);
  if (min == true) { 
    tt=tm->tm_min;
  } else {
    tt=tm->tm_hour;
  }
  // this is for testing collisions lol
 // tt=50;
}


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = window_frame = layer_get_frame(window_layer);

  disc_layer = layer_create(frame);
  layer_set_update_proc(disc_layer, disc_layer_update_callback);
  layer_add_child(window_layer, disc_layer);

  for (int i = 0; i < NUM_DISCS; i++) {
    disc_init(&discs[i]);
  }
}

static void window_unload(Window *window) {
  layer_destroy(disc_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);
  
  window_set_click_config_provider(window, click_config_provider);

  accel_data_service_subscribe(0, NULL);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void deinit(void) {
  accel_data_service_unsubscribe();

  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
