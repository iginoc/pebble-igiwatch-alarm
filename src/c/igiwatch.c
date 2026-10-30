#include "igiwatch.h"
#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>

#ifndef INT_TO_FIXED
#define INT_TO_FIXED(a) ((int32_t)(a) << 16)
#endif

// Clock state
static Layer *s_canvas_layer;
static FFont *s_time_font;
static Layer *s_status_layer;
static int s_battery_level;
static bool s_bt_connected;
static GColor s_bg_color;
static GColor s_text_color;
static int s_font_choice;
static int s_orientation_choice;
static char s_date_locale[8];

static AppTimer *s_fluid_rotation_timer;
static AppTimer *s_double_tap_timer = NULL;
static int s_tap_count = 0;
static int32_t s_current_angle = TRIG_MAX_ANGLE / 2;
static bool s_is_moving = false;

// Time display strings
static char s_hour_str[3];
static char s_minute_str[3];
static char s_time_str[6];
static char s_steps_str[12];
static char s_dow_str[8];
static char s_day_str[4];
static char s_mon_str[8];
static char s_full_date_str[20];
static char s_dow_full_str[16];
static char s_day_month_full_str[24];

static const char* DAYS_IT[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
static const char* MONTHS_IT[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};

static const char* DAYS_IT_FULL[] = {"DOMENICA", "LUNEDÌ", "MARTEDÌ", "MERCOLEDÌ", "GIOVEDÌ", "VENERDÌ", "SABATO"};
static const char* MONTHS_IT_FULL[] = {"GENNAIO", "FEBBRAIO", "MARZO", "APRILE", "MAGGIO", "GIUGNO", "LUGLIO", "AGOSTO", "SETTEMBRE", "OTTOBRE", "NOVEMBRE", "DICEMBRE"};

static const char* DAYS_EN[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char* MONTHS_EN[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static const char* DAYS_EN_FULL[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
static const char* MONTHS_EN_FULL[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};

static const char* DAYS_FR[] = {"DIM", "LUN", "MAR", "MER", "JEU", "VEN", "SAM"};
static const char* MONTHS_FR[] = {"JAN", "FÉV", "MAR", "AVR", "MAI", "JUI", "JUL", "AOÛ", "SEP", "OCT", "NOV", "DÉC"};

static const char* DAYS_FR_FULL[] = {"DIMANCHE", "LUNDI", "MARDI", "MERCREDI", "JEUDI", "VENDREDI", "SAMEDI"};
static const char* MONTHS_FR_FULL[] = {"JANVIER", "FÉVRIER", "MARS", "AVRIL", "MAI", "JUIN", "JUILLET", "AOÛT", "SEPTEMBRE", "OCTOBRE", "NOVEMBRE", "DÉCEMBRE"};

static const char* DAYS_DE[] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"};
static const char* MONTHS_DE[] = {"JAN", "FEB", "MÄR", "APR", "MAI", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEZ"};

static const char* DAYS_DE_FULL[] = {"SONNTAG", "MONTAG", "DIENSTAG", "MITTWOCH", "DONNERSTAG", "FREITAG", "SAMSTAG"};
static const char* MONTHS_DE_FULL[] = {"JANUAR", "FEBRUAR", "MÄRZ", "APRIL", "MAI", "JUNI", "JULI", "AUGUST", "SEPTEMBER", "OKTOBER", "NOVEMBER", "DEZEMBER"};

static const char* DAYS_ES[] = {"DOM", "LUN", "MAR", "MIÉ", "JUE", "VIE", "SÁB"};
static const char* MONTHS_ES[] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};

static const char* DAYS_ES_FULL[] = {"DOMINGO", "LUNES", "MARTES", "MIÉRCOLES", "JUEVES", "VIERNES", "SÁBADO"};
static const char* MONTHS_ES_FULL[] = {"ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO", "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"};

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  if (clock_is_24h_style()) {
    strftime(s_hour_str, sizeof(s_hour_str), "%H", tick_time);
  } else {
    strftime(s_hour_str, sizeof(s_hour_str), "%I", tick_time);
  }
  strftime(s_minute_str, sizeof(s_minute_str), "%M", tick_time);

  snprintf(s_time_str, sizeof(s_time_str), "%s:%s", s_hour_str, s_minute_str);

  strftime(s_dow_str, sizeof(s_dow_str), "%a", tick_time);
  snprintf(s_day_str, sizeof(s_day_str), "%d", tick_time->tm_mday);
  strftime(s_mon_str, sizeof(s_mon_str), "%m", tick_time);
  
  const char** days = DAYS_IT;
  const char** months = MONTHS_IT;
  const char** days_full = DAYS_IT_FULL;
  const char** months_full = MONTHS_IT_FULL;

  if (strncmp(s_date_locale, "en", 2) == 0) {
    days = DAYS_EN;
    months = MONTHS_EN;
    days_full = DAYS_EN_FULL;
    months_full = MONTHS_EN_FULL;
  } else if (strncmp(s_date_locale, "fr", 2) == 0) {
    days = DAYS_FR;
    months = MONTHS_FR;
    days_full = DAYS_FR_FULL;
    months_full = MONTHS_FR_FULL;
  } else if (strncmp(s_date_locale, "de", 2) == 0) {
    days = DAYS_DE;
    months = MONTHS_DE;
    days_full = DAYS_DE_FULL;
    months_full = MONTHS_DE_FULL;
  } else if (strncmp(s_date_locale, "es", 2) == 0) {
    days = DAYS_ES;
    months = MONTHS_ES;
    days_full = DAYS_ES_FULL;
    months_full = MONTHS_ES_FULL;
  }

  snprintf(s_full_date_str, sizeof(s_full_date_str), "%s %d %s", days[tick_time->tm_wday], tick_time->tm_mday, months[tick_time->tm_mon]);

  snprintf(s_dow_full_str, sizeof(s_dow_full_str), "%s", days_full[tick_time->tm_wday]);

  if (strncmp(s_date_locale, "de", 2) == 0) {
    snprintf(s_day_month_full_str, sizeof(s_day_month_full_str), "%d. %s", tick_time->tm_mday, months_full[tick_time->tm_mon]);
  } else if (strncmp(s_date_locale, "en", 2) == 0) {
    snprintf(s_day_month_full_str, sizeof(s_day_month_full_str), "%s %d", months_full[tick_time->tm_mon], tick_time->tm_mday);
  } else {
    snprintf(s_day_month_full_str, sizeof(s_day_month_full_str), "%d %s", tick_time->tm_mday, months_full[tick_time->tm_mon]);
  }

  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void fluid_rotation_timer_callback(void *data) {
  s_fluid_rotation_timer = NULL;

  s_is_moving = false;
  layer_mark_dirty(s_canvas_layer);
  layer_mark_dirty(s_status_layer);

}

static void double_tap_timer_callback(void *data) {
  s_double_tap_timer = NULL;
  s_tap_count = 0;
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Tocco rilevato! Asse: %d", (int)axis);

  s_tap_count++;
  if (s_tap_count == 1) {
    s_double_tap_timer = app_timer_register(1200, double_tap_timer_callback, NULL);
    return;
  }

  if (s_double_tap_timer) {
    app_timer_cancel(s_double_tap_timer);
    s_double_tap_timer = NULL;
  }
  s_tap_count = 0;

  s_is_moving = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Doppio tocco confermato! Mostro i passi.");
  
  // Get step count
  #if defined(PBL_HEALTH)
  HealthMetric metric = HealthMetricStepCount;
  time_t start = time_start_of_today();
  time_t end = time(NULL);
  int steps = (int)health_service_sum_today(metric);
  APP_LOG(APP_LOG_LEVEL_INFO, "PBL_HEALTH defined. Steps: %d", steps);
  snprintf(s_steps_str, sizeof(s_steps_str), "%d", steps);
  #else
  snprintf(s_steps_str, sizeof(s_steps_str), "N/A");
  APP_LOG(APP_LOG_LEVEL_INFO, "PBL_HEALTH NOT defined. Steps: %s", s_steps_str);
  #endif

  layer_mark_dirty(s_canvas_layer);
  layer_mark_dirty(s_status_layer);

  // Mostra i passi e avvia il timer per nasconderli.
  // La rotazione automatica tramite accelerometro è stata rimossa.
  if (s_fluid_rotation_timer) {
    app_timer_reschedule(s_fluid_rotation_timer, 5000);
  } else {
    s_fluid_rotation_timer = app_timer_register(5000, fluid_rotation_timer_callback, NULL);
  }
}

// Draw the clock on screen
static void layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Draw a background rectangle
  graphics_context_set_fill_color(ctx, s_bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GPoint center = grect_center_point(&bounds);

  // Ensure font is loaded for either steps or time
  if (!s_time_font) {
    uint32_t font_res_id;
    switch (s_font_choice) {
      case 1:
        font_res_id = RESOURCE_ID_LECO1976_REGULAR;
        break;
      case 2:
        font_res_id = RESOURCE_ID_AVENIR_NEXT_REGULAR;
        break;
      default: // case 0
        font_res_id = RESOURCE_ID_AVENIR_NEXT_DEMI_BOLD;
        break;
    }
    s_time_font = ffont_create_from_resource(font_res_id);
  }

  graphics_context_set_fill_color(ctx, s_text_color);
  graphics_context_set_stroke_color(ctx, s_text_color);

  // Draw a small ball attached to the big one
  if (s_is_moving) {
    APP_LOG(APP_LOG_LEVEL_INFO, "layer_update_proc: mostrando i passi");
    // Draw steps with large font
    FContext fctx;
    fctx_init_context(&fctx, ctx);
    fctx_set_fill_color(&fctx, s_text_color);
    // Fixed rotation (portrait) for steps during calibration or follow s_current_angle?
    // Let's keep it readable in portrait for now
    fctx_set_offset(&fctx, FPointI(center.x, center.y - 40));
    fctx_set_text_em_height(&fctx, s_time_font, 48); 

    APP_LOG(APP_LOG_LEVEL_INFO, "Drawing steps: %s", s_steps_str);
    fctx_begin_fill(&fctx);
    fctx_draw_string(&fctx, s_steps_str, s_time_font, GTextAlignmentCenter, FTextAnchorMiddle);
    fctx_end_fill(&fctx);

    fctx_deinit_context(&fctx); // Deinitialize fctx after drawing steps number

    const char *label_steps = s_dow_full_str;
    const char *label_today = s_day_month_full_str;

    // Usa un font più grande per le etichette
    GFont label_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    graphics_context_set_text_color(ctx, s_text_color);
    
    graphics_draw_text(ctx, label_steps, label_font, GRect(0, center.y - 12, bounds.size.w, 34), 
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, label_today, label_font, GRect(0, center.y + 26, bounds.size.w, 34), 
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  if (!s_is_moving) {
    int32_t text_rotation = 0;
    FPoint text_pos;
    int font_size;

    if (s_current_angle == TRIG_MAX_ANGLE / 4) { // 90 deg
        text_rotation = TRIG_MAX_ANGLE * 3 / 4; // 270 deg
        text_pos.x = INT_TO_FIXED(0);
        text_pos.y = INT_TO_FIXED(bounds.size.h + 4); // Abbassato di 2 pixel
        font_size = 96;
    } else if (s_current_angle == TRIG_MAX_ANGLE * 3 / 4) { // 270 deg
        text_rotation = TRIG_MAX_ANGLE / 4; // 90 deg
        text_pos.x = INT_TO_FIXED(bounds.size.w);
        text_pos.y = INT_TO_FIXED(4); // Abbassato di 2 pixel
        font_size = 96;
    } else {
        text_rotation = 0;
        text_pos.x = INT_TO_FIXED(0);
        text_pos.y = INT_TO_FIXED(2); // Abbassato di 2 pixel
        font_size = 96;
    }

    FContext fctx;
    fctx_init_context(&fctx, ctx);
    fctx_set_fill_color(&fctx, s_text_color);

    fctx_set_rotation(&fctx, text_rotation);
    fctx_set_offset(&fctx, text_pos);

    fctx_set_text_em_height(&fctx, s_time_font, font_size);

    fctx_begin_fill(&fctx);
    fctx_draw_string(&fctx, s_hour_str, s_time_font, GTextAlignmentLeft, FTextAnchorTop);
    fctx_end_fill(&fctx);

    // Reduce the line height to bring minutes closer to the hour
    int line_height = font_size * 3 / 4;
    if (s_current_angle != TRIG_MAX_ANGLE / 2) {
      line_height += 2;
    }

    int32_t dy = (cos_lookup(text_rotation) * line_height) / TRIG_MAX_RATIO;
    int32_t dx = (-sin_lookup(text_rotation) * line_height) / TRIG_MAX_RATIO;
    text_pos.x += INT_TO_FIXED(dx);
    text_pos.y += INT_TO_FIXED(dy);
    fctx_set_offset(&fctx, text_pos);

    fctx_begin_fill(&fctx);
    fctx_draw_string(&fctx, s_minute_str, s_time_font, GTextAlignmentLeft, FTextAnchorTop);
    fctx_end_fill(&fctx);

    // --- Draw Date using the chosen font ---
    int date_font_size;
    if (s_current_angle == TRIG_MAX_ANGLE / 2) { // Portrait
        // Use system font for portrait
        graphics_context_set_text_color(ctx, s_text_color);
        GFont sys_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
        GRect date_bounds = GRect(0, bounds.size.h - 28, bounds.size.w, 32);
        graphics_draw_text(ctx, s_full_date_str, sys_font, date_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    } else { // Landscape
        date_font_size = 32;
        FPoint date_pos;
        if (s_current_angle == TRIG_MAX_ANGLE / 4) { // Right Landscape
            // Visual bottom-right is physical top-right
            date_pos.x = INT_TO_FIXED(bounds.size.w - 40);
            date_pos.y = INT_TO_FIXED(40);
        } else { // Left Landscape
            // Visual bottom-right is physical bottom-left
            date_pos.x = INT_TO_FIXED(40);
            date_pos.y = INT_TO_FIXED(bounds.size.h - 40);
        }
        fctx_set_text_em_height(&fctx, s_time_font, date_font_size);

        // Calculate offset for stacking day and month
        int date_line_height = date_font_size;
        int32_t date_dy = (cos_lookup(text_rotation) * date_line_height) / TRIG_MAX_RATIO;
        int32_t date_dx = (-sin_lookup(text_rotation) * date_line_height) / TRIG_MAX_RATIO;

        FPoint line_pos = date_pos;
        line_pos.x -= INT_TO_FIXED(date_dx) / 2;
        line_pos.y -= INT_TO_FIXED(date_dy) / 2;

        fctx_set_offset(&fctx, line_pos);
        fctx_begin_fill(&fctx);
        fctx_draw_string(&fctx, s_day_str, s_time_font, GTextAlignmentCenter, FTextAnchorMiddle);
        fctx_end_fill(&fctx);

        line_pos.x += INT_TO_FIXED(date_dx);
        line_pos.y += INT_TO_FIXED(date_dy);

        fctx_set_offset(&fctx, line_pos);
        fctx_begin_fill(&fctx);
        fctx_draw_string(&fctx, s_mon_str, s_time_font, GTextAlignmentCenter, FTextAnchorMiddle);
        fctx_end_fill(&fctx);
    }

    fctx_deinit_context(&fctx);
  }
}

static void status_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Don't draw status icons while the orientation is being changed
  if (s_is_moving) {
    return;
  }

  GColor bar_color;
  if (s_battery_level > 50) {
    bar_color = GColorJaegerGreen;
  } else if (s_battery_level > 20) {
    bar_color = GColorChromeYellow;
  } else {
    bar_color = GColorRed;
  }

  GRect battery_rect;
  GRect terminal_rect;
  GRect fill_rect;
  GRect bt_rect;

  // --- Bluetooth Icon & Bar Positioning ---
  graphics_context_set_text_color(ctx, s_text_color); // Set text color for BT icon

  if (s_current_angle == TRIG_MAX_ANGLE * 3 / 4) { // Landscape Sinistro
    // L'angolo in alto a destra visuale corrisponde al basso a destra fisico
    battery_rect = GRect(bounds.size.w - 24, bounds.size.h - 52, 20, 44);
    terminal_rect = GRect(bounds.size.w - 18, bounds.size.h - 56, 8, 4);
    bt_rect = GRect(bounds.size.w - 48, bounds.size.h - 18, 20, 14);
  } else if (s_current_angle == TRIG_MAX_ANGLE / 4) { // Landscape Destro
    // L'angolo in alto a destra visuale corrisponde all'alto a sinistra fisico
    battery_rect = GRect(4, 8, 20, 44);
    terminal_rect = GRect(10, 4, 8, 4);
    bt_rect = GRect(28, 4, 20, 14);
  } else { // Portrait
    battery_rect = GRect(bounds.size.w - 24, 8, 20, 44);
    terminal_rect = GRect(bounds.size.w - 18, 4, 8, 4);
    bt_rect = GRect(bounds.size.w - 48, 4, 20, 14);
  }

  // Calcola il riempimento basato sulla percentuale (altezza max 40px)
  int fill_max_h = 40;
  int fill_h = (fill_max_h * s_battery_level) / 100;
  // Riempimento dal basso verso l'alto
  fill_rect = GRect(battery_rect.origin.x + 2, 
                    battery_rect.origin.y + 2 + (fill_max_h - fill_h), 
                    16, fill_h);

  // Draw battery frame and terminal
  graphics_context_set_stroke_color(ctx, s_text_color);
  graphics_draw_rect(ctx, battery_rect);
  graphics_context_set_fill_color(ctx, s_text_color);
  graphics_fill_rect(ctx, terminal_rect, 0, GCornerNone);

  // Draw the battery fill
  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(ctx, fill_rect, 0, GCornerNone);

  if (!s_bt_connected) {
    // Disegna il simbolo Bluetooth
    // Le coordinate del simbolo sono ora direttamente quelle di bt_rect
    int sym_w = bt_rect.size.w;
    int sym_h = bt_rect.size.h;
    int sym_x = bt_rect.origin.x;
    int sym_y = bt_rect.origin.y;

    GPoint p_top = GPoint(sym_x + sym_w / 2, sym_y);
    GPoint p_bottom = GPoint(sym_x + sym_w / 2, sym_y + sym_h);
    GPoint p_mid_left = GPoint(sym_x, sym_y + sym_h / 2);
    GPoint p_mid_right = GPoint(sym_x + sym_w, sym_y + sym_h / 2);

    // Disegna le 5 linee che compongono il simbolo Bluetooth
    graphics_draw_line(ctx, p_top, p_mid_left);
    graphics_draw_line(ctx, p_top, p_mid_right);
    graphics_draw_line(ctx, p_mid_left, p_bottom);
    graphics_draw_line(ctx, p_mid_right, p_bottom);
    graphics_draw_line(ctx, p_mid_left, p_mid_right);

  }
}

static void battery_callback(BatteryChargeState charge) {
  s_battery_level = charge.charge_percent;
  layer_mark_dirty(s_status_layer);
}

static void bluetooth_callback(bool connected) {
  s_bt_connected = connected;
  layer_mark_dirty(s_status_layer);

  if(!connected) {
    // Vibrate for disconnection
    vibes_double_pulse();
  }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if (bg_color_t) {
    s_bg_color = GColorFromHEX(bg_color_t->value->int32);
    persist_write_int(MESSAGE_KEY_BackgroundColor, bg_color_t->value->int32);
  }

  Tuple *text_color_t = dict_find(iter, MESSAGE_KEY_TextColor);
  if (text_color_t) {
    s_text_color = GColorFromHEX(text_color_t->value->int32);
    persist_write_int(MESSAGE_KEY_TextColor, text_color_t->value->int32);
  }

  Tuple *font_choice_t = dict_find(iter, MESSAGE_KEY_FontChoice);
  if (font_choice_t) {
    s_font_choice = atoi(font_choice_t->value->cstring);
    persist_write_int(MESSAGE_KEY_FontChoice, s_font_choice);

    // Unload old font, new one will be loaded in update_proc
    if (s_time_font) {
      ffont_destroy(s_time_font);
      s_time_font = NULL;
    }
  }

  Tuple *orientation_t = dict_find(iter, MESSAGE_KEY_ScreenOrientation);
  if (orientation_t) {
    s_orientation_choice = atoi(orientation_t->value->cstring);
    persist_write_int(MESSAGE_KEY_ScreenOrientation, s_orientation_choice);

    // Applica l'orientamento scelto
    if (s_orientation_choice == 2) { // Landscape Left
      s_current_angle = TRIG_MAX_ANGLE * 3 / 4;
    } else if (s_orientation_choice == 3) { // Landscape Right
      s_current_angle = TRIG_MAX_ANGLE / 4;
    } else { // Portrait
      s_current_angle = TRIG_MAX_ANGLE / 2;
    }

    layer_mark_dirty(s_status_layer);
  }

  Tuple *language_t = dict_find(iter, MESSAGE_KEY_DateLanguage);
  if (language_t) {
    strncpy(s_date_locale, language_t->value->cstring, sizeof(s_date_locale));
    persist_write_string(MESSAGE_KEY_DateLanguage, s_date_locale);
    update_time();
  }

  layer_mark_dirty(s_canvas_layer);
}

// Initialize clock
void Clock_init(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the clock display layer
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, layer_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Create status layer
  s_status_layer = layer_create(bounds);
  layer_set_update_proc(s_status_layer, status_update_proc);
  layer_add_child(window_layer, s_status_layer);

  // Subscribe to tick timer service
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_tap_service_subscribe(tap_handler);

  // Subscribe to services
  battery_state_service_subscribe(battery_callback);
  bluetooth_connection_service_subscribe(bluetooth_callback);

  // Get initial state
  battery_callback(battery_state_service_peek());
  bluetooth_callback(connection_service_peek_pebble_app_connection());

  update_time();
}

// Deinitialize clock
void Clock_deinit(void) {
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();

  if (s_fluid_rotation_timer) {
    app_timer_cancel(s_fluid_rotation_timer);
    s_fluid_rotation_timer = NULL;
  }

  if (s_double_tap_timer) {
    app_timer_cancel(s_double_tap_timer);
    s_double_tap_timer = NULL;
  }

  if (s_time_font) {
    ffont_destroy(s_time_font);
    s_time_font = NULL;
  }

  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
  }

  if (s_status_layer) {
    layer_destroy(s_status_layer);
    s_status_layer = NULL;
  }
}

static Window *s_main_window;

static void main_window_load(Window *window) { Clock_init(window); }

static void main_window_unload(Window *window) { Clock_deinit(); }

static void init(void) {
  // Load colors or set defaults
  if (persist_exists(MESSAGE_KEY_BackgroundColor)) {
    s_bg_color = GColorFromHEX(persist_read_int(MESSAGE_KEY_BackgroundColor));
  } else {
    s_bg_color = GColorBlack;
  }

  if (persist_exists(MESSAGE_KEY_TextColor)) {
    s_text_color = GColorFromHEX(persist_read_int(MESSAGE_KEY_TextColor));
  } else {
    s_text_color = GColorWhite;
  }

  // Load font choice or set default
  if (persist_exists(MESSAGE_KEY_FontChoice)) {
    s_font_choice = persist_read_int(MESSAGE_KEY_FontChoice);
  } else {
    s_font_choice = 0; // 0: Avenir, 1: Leco
  }

  // Load orientation choice or set default
  if (persist_exists(MESSAGE_KEY_ScreenOrientation)) {
    s_orientation_choice = persist_read_int(MESSAGE_KEY_ScreenOrientation);
  } else {
    s_orientation_choice = 1; // 1: Portrait
  }

  // Load date language or set default
  if (persist_exists(MESSAGE_KEY_DateLanguage)) {
    persist_read_string(MESSAGE_KEY_DateLanguage, s_date_locale, sizeof(s_date_locale));
  } else {
    strcpy(s_date_locale, "it_IT");
  }

  if (s_orientation_choice == 1) { // Portrait
    s_current_angle = TRIG_MAX_ANGLE / 2;
  } else if (s_orientation_choice == 2) { // Landscape Left
    s_current_angle = TRIG_MAX_ANGLE * 3 / 4;
  } else if (s_orientation_choice == 3) { // Landscape Right
    s_current_angle = TRIG_MAX_ANGLE / 4;
  }

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);

  s_main_window = window_create();
  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});
  window_set_background_color(s_main_window, GColorBlack);
  window_stack_push(s_main_window, true);
}

static void deinit(void) { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
