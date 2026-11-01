#include "igiwatchalarm.h"
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

// Alarm state (supporto fino a 3 sveglie con selezione giorni)
#define NUM_ALARMS 3

typedef struct {
  bool active;
  char time_str[8];
  int hour;
  int min;
  uint8_t days_mask; // Bit 0 = Dom, Bit 1 = Lun, ..., Bit 6 = Sab (0-127)
  bool triggered_this_minute;
} AlarmConfig;

static AlarmConfig s_alarms[NUM_ALARMS];
static bool s_alarm_ringing = false;
static AppTimer *s_alarm_vibe_timer = NULL;
static AppTimer *s_alarm_timeout_timer = NULL;
static GBitmap *s_alarm_bitmap = NULL;
static WakeupId s_alarm_wakeup_ids[NUM_ALARMS] = {-1, -1, -1};

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

static bool tuple_get_bool(Tuple *t) {
  if (!t) return false;
  if (t->length == 1) return t->value->uint8 != 0;
  if (t->length == 2) return t->value->uint16 != 0;
  if (t->length == 4) return t->value->uint32 != 0;
  return false;
}

static void parse_alarm_time_struct(const char *time_str, int *out_h, int *out_m) {
  *out_h = -1;
  *out_m = -1;
  if (!time_str) {
    return;
  }
  while (*time_str == ' ') time_str++;
  const char *colon = time_str;
  while (*colon && *colon != ':') {
    colon++;
  }
  if (*colon == ':') {
    int h = atoi(time_str);
    int m = atoi(colon + 1);
    const char *suffix = colon + 1;
    while (*suffix >= '0' && *suffix <= '9') suffix++;
    while (*suffix == ' ') suffix++;
    if ((*suffix == 'p' || *suffix == 'P') && h < 12) {
      h += 12;
    } else if ((*suffix == 'a' || *suffix == 'A') && h == 12) {
      h = 0;
    }
    if (h >= 0 && h < 24 && m >= 0 && m < 60) {
      *out_h = h;
      *out_m = m;
    }
  }
}

static bool is_any_alarm_active(void) {
  for (int i = 0; i < NUM_ALARMS; i++) {
    if (s_alarms[i].active && s_alarms[i].hour >= 0 && s_alarms[i].min >= 0 && s_alarms[i].days_mask > 0) {
      return true;
    }
  }
  return false;
}

static void schedule_alarm_wakeups(void) {
  wakeup_cancel_all();
  for (int i = 0; i < NUM_ALARMS; i++) {
    s_alarm_wakeup_ids[i] = -1;
  }

  time_t now = time(NULL);
  struct tm *now_tm = localtime(&now);

  for (int i = 0; i < NUM_ALARMS; i++) {
    if (!s_alarms[i].active || s_alarms[i].hour < 0 || s_alarms[i].min < 0 || s_alarms[i].days_mask == 0) {
      continue;
    }

    int current_wday = now_tm->tm_wday; // 0 = Domenica, 1 = Lunedì, ...
    int days_ahead = -1;

    for (int offset = 0; offset < 7; offset++) {
      int check_wday = (current_wday + offset) % 7;
      if (s_alarms[i].days_mask & (1 << check_wday)) {
        if (offset == 0) {
          struct tm today_target = *now_tm;
          today_target.tm_hour = s_alarms[i].hour;
          today_target.tm_min = s_alarms[i].min;
          today_target.tm_sec = 0;
          time_t t_target = mktime(&today_target);
          if (t_target > now + 2) {
            days_ahead = 0;
            break;
          }
        } else {
          days_ahead = offset;
          break;
        }
      }
    }

    if (days_ahead >= 0) {
      struct tm target_tm = *now_tm;
      target_tm.tm_hour = s_alarms[i].hour;
      target_tm.tm_min = s_alarms[i].min;
      target_tm.tm_sec = 0;
      time_t target_time = mktime(&target_tm) + (days_ahead * 86400);

      s_alarm_wakeup_ids[i] = wakeup_schedule(target_time, i, false);
      if (s_alarm_wakeup_ids[i] < 0) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Errore wakeup sveglia %d: %ld", i + 1, (long)s_alarm_wakeup_ids[i]);
      } else {
        APP_LOG(APP_LOG_LEVEL_INFO, "Wakeup sveglia %d programmata per timestamp %ld (+%d gg, ID: %ld)", 
                i + 1, (long)target_time, days_ahead, (long)s_alarm_wakeup_ids[i]);
      }
    }
  }
}

// Segmenti vibrazione per un "tic" breve e netto (80 ms)
static const uint32_t s_alarm_tic_segments[] = { 80 };
static const VibePattern s_alarm_tic_pattern = {
  .durations = (uint32_t *)s_alarm_tic_segments,
  .num_segments = ARRAY_LENGTH(s_alarm_tic_segments),
};
static int s_alarm_second_count = 0;

static void stop_alarm(void) {
  if (s_alarm_vibe_timer) {
    app_timer_cancel(s_alarm_vibe_timer);
    s_alarm_vibe_timer = NULL;
  }
  if (s_alarm_timeout_timer) {
    app_timer_cancel(s_alarm_timeout_timer);
    s_alarm_timeout_timer = NULL;
  }
  s_alarm_second_count = 0;
  vibes_cancel();

  if (s_alarm_ringing) {
    s_alarm_ringing = false;
    APP_LOG(APP_LOG_LEVEL_INFO, "Sveglia fermata.");
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
    if (s_status_layer) {
      layer_mark_dirty(s_status_layer);
    }
    schedule_alarm_wakeups();
  }
}

static void alarm_vibe_timer_callback(void *data) {
  s_alarm_vibe_timer = NULL;
  if (!s_alarm_ringing) {
    return;
  }

  s_alarm_second_count++;
  if (s_alarm_second_count >= 15) {
    APP_LOG(APP_LOG_LEVEL_INFO, "15 secondi trascorsi: sveglia silenziata automaticamente.");
    stop_alarm();
    return;
  }

  // Emette un tic per ogni secondo trascorso
  vibes_enqueue_custom_pattern(s_alarm_tic_pattern);
  s_alarm_vibe_timer = app_timer_register(1000, alarm_vibe_timer_callback, NULL);
}

static void trigger_alarm(void) {
  if (s_alarm_ringing) {
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "SVEGLIA ATTIVATA! Mostro ora e immagine, vibrazione: un tic al secondo (max 15s).");
  s_alarm_ringing = true;
  s_alarm_second_count = 0;

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
  if (s_status_layer) {
    layer_mark_dirty(s_status_layer);
  }

  // Primo tic immediato
  vibes_enqueue_custom_pattern(s_alarm_tic_pattern);

  // Pianifica il tic successivo ogni secondo (1000 ms) fino a un massimo di 15 secondi
  s_alarm_vibe_timer = app_timer_register(1000, alarm_vibe_timer_callback, NULL);
}

static void wakeup_handler(WakeupId id, int32_t cookie) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Evento Wakeup scattato! ID: %ld", (long)id);
  trigger_alarm();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();

  for (int i = 0; i < NUM_ALARMS; i++) {
    if (s_alarms[i].active && s_alarms[i].hour >= 0 && s_alarms[i].min >= 0) {
      uint8_t today_bit = (1 << tick_time->tm_wday);
      if (s_alarms[i].days_mask & today_bit) {
        if (tick_time->tm_hour == s_alarms[i].hour && tick_time->tm_min == s_alarms[i].min) {
          if (!s_alarms[i].triggered_this_minute) {
            s_alarms[i].triggered_this_minute = true;
            trigger_alarm();
          }
        } else {
          s_alarms[i].triggered_this_minute = false;
        }
      } else {
        s_alarms[i].triggered_this_minute = false;
      }
    }
  }
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
  if (s_alarm_ringing) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Sveglia silenziata con tocco.");
    stop_alarm();
    return;
  }
  vibes_cancel();
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

  if (s_alarm_ringing) {
    graphics_context_set_text_color(ctx, s_text_color);

    #if defined(PBL_ROUND)
    int time_y = 12;
    #else
    int time_y = 4;
    #endif

    // Mostra in alto l'ora attuale
    GFont time_font = fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS);
    GRect time_rect = GRect(0, time_y, bounds.size.w, 38);
    graphics_draw_text(ctx, s_time_str, time_font, time_rect, 
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Mostra la grande immagine
    if (!s_alarm_bitmap) {
      s_alarm_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ALARM_LARGE);
    }
    if (s_alarm_bitmap) {
      GRect bmp_bounds = gbitmap_get_bounds(s_alarm_bitmap);
      int bmp_w = bmp_bounds.size.w;
      int bmp_h = bmp_bounds.size.h;
      int bmp_x = (bounds.size.w - bmp_w) / 2;
      int bmp_y = time_y + 40 + (bounds.size.h - (time_y + 40) - bmp_h - 22) / 2;
      if (bmp_y < time_y + 38) {
        bmp_y = time_y + 38;
      }
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, s_alarm_bitmap, GRect(bmp_x, bmp_y, bmp_w, bmp_h));
    }

    // Mostra indicazione per fermare la sveglia
    GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    #if defined(PBL_ROUND)
    int hint_y = bounds.size.h - 26;
    #else
    int hint_y = bounds.size.h - 22;
    #endif
    graphics_draw_text(ctx, "Tocca orologio", hint_font, GRect(0, hint_y, bounds.size.w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

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

    char alarm_info[40];
    bool alarm_found = false;
    for (int i = 0; i < NUM_ALARMS; i++) {
      if (s_alarms[i].active && s_alarms[i].hour >= 0 && s_alarms[i].min >= 0 && s_alarms[i].days_mask) {
        snprintf(alarm_info, sizeof(alarm_info), "Sveglia %d: %02d:%02d",
                 i + 1, s_alarms[i].hour, s_alarms[i].min);
        alarm_found = true;
        break;
      }
    }
    if (!alarm_found) {
      snprintf(alarm_info, sizeof(alarm_info), "Sveglia: OFF");
    }
    GFont info_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    graphics_draw_text(ctx, alarm_info, info_font, GRect(0, bounds.size.h - 22, bounds.size.w, 20),
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

  // Don't draw status icons while orientation is being changed or alarm is ringing
  if (s_is_moving || s_alarm_ringing) {
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

  if (is_any_alarm_active()) {
    GPoint clock_center;
    if (s_current_angle == TRIG_MAX_ANGLE * 3 / 4) { // Landscape Sinistro
      clock_center = GPoint(battery_rect.origin.x - 14, battery_rect.origin.y + battery_rect.size.h / 2);
    } else if (s_current_angle == TRIG_MAX_ANGLE / 4) { // Landscape Destro
      clock_center = GPoint(battery_rect.origin.x + battery_rect.size.w + 14, battery_rect.origin.y + battery_rect.size.h / 2);
    } else { // Portrait - posizionata direttamente sotto la batteria
      clock_center = GPoint(battery_rect.origin.x + battery_rect.size.w / 2, 
                            battery_rect.origin.y + battery_rect.size.h + 10);
    }

    // Disegna icona sveglia stilizzata
    graphics_context_set_stroke_color(ctx, s_text_color);
    graphics_context_set_fill_color(ctx, s_text_color);
    graphics_draw_circle(ctx, clock_center, 5);
    graphics_draw_line(ctx, clock_center, GPoint(clock_center.x, clock_center.y - 3));
    graphics_draw_line(ctx, clock_center, GPoint(clock_center.x + 3, clock_center.y));
    // Campanelli in alto
    graphics_draw_pixel(ctx, GPoint(clock_center.x - 4, clock_center.y - 5));
    graphics_draw_pixel(ctx, GPoint(clock_center.x + 4, clock_center.y - 5));
    // Piedini in basso
    graphics_draw_pixel(ctx, GPoint(clock_center.x - 4, clock_center.y + 5));
    graphics_draw_pixel(ctx, GPoint(clock_center.x + 4, clock_center.y + 5));
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

  const uint32_t active_keys[NUM_ALARMS] = {
    MESSAGE_KEY_Alarm1Active, MESSAGE_KEY_Alarm2Active, MESSAGE_KEY_Alarm3Active
  };
  const uint32_t time_keys[NUM_ALARMS] = {
    MESSAGE_KEY_Alarm1Time, MESSAGE_KEY_Alarm2Time, MESSAGE_KEY_Alarm3Time
  };
  const uint32_t days_keys[NUM_ALARMS] = {
    MESSAGE_KEY_Alarm1Days, MESSAGE_KEY_Alarm2Days, MESSAGE_KEY_Alarm3Days
  };
  bool alarms_changed = false;
  for (int i = 0; i < NUM_ALARMS; i++) {
    Tuple *active_t = dict_find(iter, active_keys[i]);
    if (active_t) {
      s_alarms[i].active = tuple_get_bool(active_t);
      persist_write_bool(active_keys[i], s_alarms[i].active);
      alarms_changed = true;
    }

    Tuple *alarm_time_tuple = dict_find(iter, time_keys[i]);
    if (alarm_time_tuple) {
      strncpy(s_alarms[i].time_str, alarm_time_tuple->value->cstring, sizeof(s_alarms[i].time_str) - 1);
      s_alarms[i].time_str[sizeof(s_alarms[i].time_str) - 1] = '\0';
      parse_alarm_time_struct(s_alarms[i].time_str, &s_alarms[i].hour, &s_alarms[i].min);
      persist_write_string(time_keys[i], s_alarms[i].time_str);
      alarms_changed = true;
    }

    Tuple *days_t = dict_find(iter, days_keys[i]);
    if (days_t) {
      uint32_t days_mask = days_t->value->uint32;
      s_alarms[i].days_mask = days_mask & 0x7f;
      persist_write_int(days_keys[i], s_alarms[i].days_mask);
      alarms_changed = true;
    }
  }

  if (alarms_changed) {
    schedule_alarm_wakeups();
    vibes_short_pulse();
    layer_mark_dirty(s_status_layer);
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
  stop_alarm();
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

  if (s_alarm_bitmap) {
    gbitmap_destroy(s_alarm_bitmap);
    s_alarm_bitmap = NULL;
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

  const uint32_t active_keys[NUM_ALARMS] = {
    MESSAGE_KEY_Alarm1Active, MESSAGE_KEY_Alarm2Active, MESSAGE_KEY_Alarm3Active
  };
  const uint32_t time_keys[NUM_ALARMS] = {
    MESSAGE_KEY_Alarm1Time, MESSAGE_KEY_Alarm2Time, MESSAGE_KEY_Alarm3Time
  };
  const uint32_t days_keys[NUM_ALARMS] = {
    MESSAGE_KEY_Alarm1Days, MESSAGE_KEY_Alarm2Days, MESSAGE_KEY_Alarm3Days
  };
  static const char *default_alarm_times[NUM_ALARMS] = {"07:00", "08:00", "09:00"};
  for (int i = 0; i < NUM_ALARMS; i++) {
    s_alarms[i].active = persist_exists(active_keys[i]) && persist_read_bool(active_keys[i]);
    if (persist_exists(time_keys[i])) {
      persist_read_string(time_keys[i], s_alarms[i].time_str, sizeof(s_alarms[i].time_str));
    } else {
      strncpy(s_alarms[i].time_str, default_alarm_times[i], sizeof(s_alarms[i].time_str));
    }
    parse_alarm_time_struct(s_alarms[i].time_str, &s_alarms[i].hour, &s_alarms[i].min);
    s_alarms[i].days_mask = persist_exists(days_keys[i]) ? persist_read_int(days_keys[i]) & 0x7f : 0x7f;
    s_alarms[i].triggered_this_minute = false;
  }

  // Migrate the previous single-alarm settings once, if no new alarm was saved yet.
  if (!persist_exists(MESSAGE_KEY_Alarm1Active) && persist_exists(MESSAGE_KEY_AlarmActive)) {
    s_alarms[0].active = persist_read_bool(MESSAGE_KEY_AlarmActive);
    if (persist_exists(MESSAGE_KEY_AlarmTime)) {
      persist_read_string(MESSAGE_KEY_AlarmTime, s_alarms[0].time_str, sizeof(s_alarms[0].time_str));
      parse_alarm_time_struct(s_alarms[0].time_str, &s_alarms[0].hour, &s_alarms[0].min);
    }
    s_alarms[0].days_mask = 0x7f;
    persist_write_bool(MESSAGE_KEY_Alarm1Active, s_alarms[0].active);
    persist_write_string(MESSAGE_KEY_Alarm1Time, s_alarms[0].time_str);
    persist_write_int(MESSAGE_KEY_Alarm1Days, s_alarms[0].days_mask);
  }
  schedule_alarm_wakeups();

  if (s_orientation_choice == 1) { // Portrait
    s_current_angle = TRIG_MAX_ANGLE / 2;
  } else if (s_orientation_choice == 2) { // Landscape Left
    s_current_angle = TRIG_MAX_ANGLE * 3 / 4;
  } else if (s_orientation_choice == 3) { // Landscape Right
    s_current_angle = TRIG_MAX_ANGLE / 4;
  }

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  s_main_window = window_create();
  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});
  window_set_background_color(s_main_window, GColorBlack);
  window_stack_push(s_main_window, true);

  wakeup_service_subscribe(wakeup_handler);
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    WakeupId id;
    int32_t reason;
    if (wakeup_get_launch_event(&id, &reason)) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Avvio da sveglia programmata! ID: %ld", (long)id);
    }
    trigger_alarm();
  }
}

static void deinit(void) { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
