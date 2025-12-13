// ============================================================================
// T-Display S3 AMOLED Clock - PHASE 2: With Web Interface
// 
// New in Phase 2:
// ✓ Web config at http://clock.local
// ✓ Change all settings from browser
// ✓ Live status dashboard
// ✓ Config stored in NVS (persists across reboots)
// ✓ No more editing code to change settings!
//
// Original Phase 1 features still work:
// ✓ Battery charging fixed
// ✓ Sunrise/sunset auto-brightness
// ✓ Touch controls
// ✓ WiFi reconnect
// ============================================================================

#define LILYGO_AMOLED_191_H754

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <Wire.h>
#include <math.h>

// ============================================================================
// CONFIGURATION STRUCTURE - Now stored in NVS!
// ============================================================================

struct ClockConfig {
  // Display
  bool show_seconds = true;
  bool show_date = true;
  uint8_t color_scheme = 0; // 0=Red,1=Green,2=Blue,3=White,4=Amber
  
  // Brightness
  bool auto_brightness = true;
  uint8_t day_brightness = 200;
  uint8_t night_brightness = 40;
  uint8_t transition_minutes = 30;
  
  // Location (default: San Francisco, CA - update for your location)
  float latitude = 37.7749;
  float longitude = -122.4194;
  char timezone[64] = "PST8PDT,M3.2.0/2,M11.1.0/2";
  
  // WiFi
  char wifi_ssid[32] = "YOUR_WIFI_SSID";
  char wifi_pass[64] = "YOUR_WIFI_PASSWORD";
  
  // Weather (optional)
  bool weather_enabled = false;
  char weather_api_key[64] = "";
};

ClockConfig config;
Preferences prefs;

// ============================================================================
// GLOBALS
// ============================================================================

LilyGo_Class amoled;
static const uint8_t BQ = 0x6B;

// UI elements
lv_obj_t *row_time = nullptr;
lv_obj_t *lbl_hr = nullptr, *lbl_col1 = nullptr;
lv_obj_t *lbl_min = nullptr, *lbl_col2 = nullptr;
lv_obj_t *lbl_secA = nullptr;
lv_obj_t *date_label = nullptr;
lv_obj_t *status_label = nullptr;

// Color palettes [bright, dim]
const lv_color_t COLORS[][2] = {
  {lv_color_hex(0xFF2A2A), lv_color_hex(0xCC2424)}, // Red
  {lv_color_hex(0x2AFF2A), lv_color_hex(0x24CC24)}, // Green
  {lv_color_hex(0x2A2AFF), lv_color_hex(0x2424CC)}, // Blue
  {lv_color_hex(0xFFFFFF), lv_color_hex(0xCCCCCC)}, // White
  {lv_color_hex(0xFFAA00), lv_color_hex(0xCC8800)}, // Amber
};

// State
int sunrise_time = 0, sunset_time = 0;
uint8_t current_brightness = 200;
uint8_t target_brightness = 200;
int brightness_mode = 0; // 0=auto, 1=full, 2=dim, 3=medium
const uint8_t manual_levels[] = {0, 255, 40, 128};
float battery_voltage = 0.0;

// Fonts
extern "C" {
  extern const lv_font_t ds_digib_120;
  extern const lv_font_t ds_digib_48;
}

// ============================================================================
// CONFIG MANAGEMENT - Save/Load from NVS
// ============================================================================

void save_config() {
  prefs.begin("clock", false);
  prefs.putBool("show_sec", config.show_seconds);
  prefs.putBool("show_date", config.show_date);
  prefs.putUChar("color", config.color_scheme);
  prefs.putBool("auto_br", config.auto_brightness);
  prefs.putUChar("day_br", config.day_brightness);
  prefs.putUChar("night_br", config.night_brightness);
  prefs.putUChar("trans", config.transition_minutes);
  prefs.putFloat("lat", config.latitude);
  prefs.putFloat("lon", config.longitude);
  prefs.putString("tz", config.timezone);
  prefs.putString("ssid", config.wifi_ssid);
  prefs.putString("pass", config.wifi_pass);
  prefs.putBool("wthr_en", config.weather_enabled);
  prefs.putString("wthr_key", config.weather_api_key);
  prefs.end();
  Serial.println("💾 Config saved to NVS");
}

void load_config() {
  prefs.begin("clock", true); // read-only
  config.show_seconds = prefs.getBool("show_sec", true);
  config.show_date = prefs.getBool("show_date", true);
  config.color_scheme = prefs.getUChar("color", 0);
  config.auto_brightness = prefs.getBool("auto_br", true);
  config.day_brightness = prefs.getUChar("day_br", 200);
  config.night_brightness = prefs.getUChar("night_br", 40);
  config.transition_minutes = prefs.getUChar("trans", 30);
  config.latitude = prefs.getFloat("lat", 37.7749);
  config.longitude = prefs.getFloat("lon", -122.4194);
  prefs.getString("tz", config.timezone, sizeof(config.timezone));
  prefs.getString("ssid", config.wifi_ssid, sizeof(config.wifi_ssid));
  prefs.getString("pass", config.wifi_pass, sizeof(config.wifi_pass));
  config.weather_enabled = prefs.getBool("wthr_en", false);
  prefs.getString("wthr_key", config.weather_api_key, sizeof(config.weather_api_key));
  prefs.end();
  Serial.println("📂 Config loaded from NVS");
}

// ============================================================================
// BATTERY MANAGEMENT
// ============================================================================

uint8_t i2cRead(uint8_t dev, uint8_t reg) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom((int)dev, 1) != 1) return 0xFF;
  return Wire.read();
}

bool i2cWriteMask(uint8_t dev, uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t v = i2cRead(dev, reg);
  if (v == 0xFF) return false;
  v = (uint8_t)((v & ~mask) | (value & mask));
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

void configure_battery_charging() {
  Wire.begin();
  if (i2cRead(BQ, 0x0B) == 0xFF) {
    Serial.println("⚠️  BQ2589x not detected");
    return;
  }
  Serial.println("🔋 Configuring charging...");
  i2cWriteMask(BQ, 0x00, 0x80, 0x00);
  i2cWriteMask(BQ, 0x00, 0x3F, 0x14);
  i2cWriteMask(BQ, 0x01, 0x10, 0x10);
  i2cWriteMask(BQ, 0x07, 0x30, 0x00);
  i2cWriteMask(BQ, 0x04, 0x3F, 0x10);
  i2cWriteMask(BQ, 0x06, 0xFC, (0x17 << 2));
  i2cWriteMask(BQ, 0x0D, 0x7F, 0x0F);
  i2cWriteMask(BQ, 0x02, 0x80, 0x80);
  i2cWriteMask(BQ, 0x03, 0xF0, 0x50);  // CHG_CONFIG=1 to enable charging
  i2cWriteMask(BQ, 0x03, 0x0F, 0x04);
  delay(100);
  Serial.println("✓ Charging configured");
}

bool is_charging() {
  uint8_t stat = i2cRead(BQ, 0x0B);
  if (stat == 0xFF) return false;
  return (stat >> 2) & 1;
}

// ============================================================================
// SUNRISE/SUNSET
// ============================================================================

double julian_day(int y, int m, int d) {
  int a = (14 - m) / 12;
  int yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

void calculate_sun_times(int year, int month, int day, float lat, float lon, int &sunrise_min, int &sunset_min) {
  double jd = julian_day(year, month, day);
  double n = jd - 2451545.0 + 0.0008;
  double j_star = n - lon / 360.0;
  double M = fmod(357.5291 + 0.98560028 * j_star, 360.0) * M_PI / 180.0;
  double C = 1.9148 * sin(M) + 0.0200 * sin(2 * M) + 0.0003 * sin(3 * M);
  double lambda = fmod(280.4665 + 0.98564736 * j_star + C, 360.0);
  double j_transit = 2451545.0 + j_star + 0.0053 * sin(M) - 0.0069 * sin(2 * lambda * M_PI / 180.0);
  double delta = asin(sin(lambda * M_PI / 180.0) * sin(23.44 * M_PI / 180.0));
  double omega = acos((sin(-0.833 * M_PI / 180.0) - sin(lat * M_PI / 180.0) * sin(delta)) / 
                      (cos(lat * M_PI / 180.0) * cos(delta)));
  double j_rise = j_transit - omega * 180.0 / M_PI / 360.0;
  double j_set = j_transit + omega * 180.0 / M_PI / 360.0;
  sunrise_min = (int)((j_rise - floor(j_rise)) * 1440.0);
  sunset_min = (int)((j_set - floor(j_set)) * 1440.0);
  sunrise_min = (sunrise_min + 480) % 1440;
  sunset_min = (sunset_min + 480) % 1440;
}

void update_sun_times() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;
  calculate_sun_times(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                     config.latitude, config.longitude, sunrise_time, sunset_time);
  Serial.printf("🌅 Sunrise: %02d:%02d, Sunset: %02d:%02d\n",
                sunrise_time / 60, sunrise_time % 60,
                sunset_time / 60, sunset_time % 60);
}

uint8_t calculate_target_brightness() {
  if (brightness_mode != 0) return manual_levels[brightness_mode];
  if (!config.auto_brightness) return config.day_brightness;
  
  struct tm ti;
  if (!getLocalTime(&ti)) return config.day_brightness;
  int now_min = ti.tm_hour * 60 + ti.tm_min;
  
  if (now_min >= sunrise_time - config.transition_minutes && 
      now_min <= sunrise_time + config.transition_minutes) {
    float progress = (float)(now_min - (sunrise_time - config.transition_minutes)) / 
                    (2.0 * config.transition_minutes);
    return config.night_brightness + progress * (config.day_brightness - config.night_brightness);
  }
  
  if (now_min >= sunset_time - config.transition_minutes && 
      now_min <= sunset_time + config.transition_minutes) {
    float progress = (float)(now_min - (sunset_time - config.transition_minutes)) / 
                    (2.0 * config.transition_minutes);
    return config.day_brightness - progress * (config.day_brightness - config.night_brightness);
  }
  
  bool is_day = (now_min > sunrise_time + config.transition_minutes && 
                 now_min < sunset_time - config.transition_minutes);
  return is_day ? config.day_brightness : config.night_brightness;
}

// ============================================================================
// WIFI
// ============================================================================

bool connect_wifi() {
  Serial.printf("📡 Connecting to %s...\n", config.wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("✓ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("✗ WiFi failed");
    return false;
  }
}

void setup_time() {
  // Set timezone FIRST
  setenv("TZ", config.timezone, 1);
  tzset();
  
  // Configure NTP - use local Starlink server first, then fallbacks
  configTzTime(config.timezone, "192.168.100.1", "time.cloudflare.com", "time.google.com");
  
  Serial.print("⏰ Syncing time");
  for (int i = 0; i < 120 && time(nullptr) < 1700000000; i++) {
    delay(250);
    if (i % 4 == 0) Serial.print(".");
  }
  Serial.println();
  if (time(nullptr) >= 1700000000) {
    Serial.println("✓ Time synced");
    update_sun_times();
  } else {
    Serial.println("✗ Time sync failed");
  }
}

// ============================================================================
// UI
// ============================================================================

void style_label(lv_obj_t* obj, const lv_font_t* font, lv_color_t color, int16_t spacing = 2) {
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_set_style_text_color(obj, color, 0);
  lv_obj_set_style_text_letter_space(obj, spacing, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

void setup_ui() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
  row_time = lv_obj_create(scr);
  lv_obj_set_size(row_time, LV_HOR_RES, LV_SIZE_CONTENT);
  lv_obj_center(row_time);
  lv_obj_set_flex_flow(row_time, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row_time, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(row_time, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row_time, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row_time, 0, 0);
  lv_obj_set_style_pad_column(row_time, 3, 0);
  lbl_hr = lv_label_create(row_time);
  lbl_col1 = lv_label_create(row_time);
  lbl_min = lv_label_create(row_time);
  lbl_col2 = lv_label_create(row_time);
  lbl_secA = lv_label_create(row_time);
  uint8_t c = config.color_scheme;
  style_label(lbl_hr, &ds_digib_120, COLORS[c][0], 2);
  style_label(lbl_col1, &ds_digib_120, COLORS[c][1], 2);
  style_label(lbl_min, &ds_digib_120, COLORS[c][0], 2);
  style_label(lbl_col2, &ds_digib_120, COLORS[c][1], 2);
  style_label(lbl_secA, &ds_digib_120, COLORS[c][0], 2);
  date_label = lv_label_create(scr);
  lv_obj_set_width(date_label, LV_HOR_RES);
  lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
  style_label(date_label, &ds_digib_48, COLORS[c][1], 2);
  lv_obj_align_to(date_label, row_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
  status_label = lv_label_create(scr);
  lv_obj_set_width(status_label, LV_HOR_RES);
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
  style_label(status_label, &ds_digib_48, COLORS[c][1], 0);
  lv_obj_align_to(status_label, date_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
}

void handle_touch(lv_event_t* e) {
  brightness_mode = (brightness_mode + 1) % 4;
  const char* modes[] = {"Auto", "Full", "Dim", "Medium"};
  Serial.printf("👆 Brightness: %s\n", modes[brightness_mode]);
  target_brightness = calculate_target_brightness();
}

void update_display(lv_timer_t*) {
  struct tm ti;
  if (!getLocalTime(&ti)) {
    lv_label_set_text(lbl_hr, "--");
    lv_label_set_text(lbl_col1, ":");
    lv_label_set_text(lbl_min, "--");
    lv_label_set_text(lbl_col2, ":");
    lv_label_set_text(lbl_secA, "-- A");
    if (config.show_date) lv_label_set_text(date_label, "Syncing...");
    return;
  }
  
  char hbuf[3]; strftime(hbuf, sizeof(hbuf), "%I", &ti);
  if (hbuf[0] == '0' && hbuf[1] == '0') {
    strcpy(hbuf, "12");
  } else if (hbuf[0] == '0') {
    hbuf[0] = hbuf[1]; hbuf[1] = '\0';
  }
  
  char mbuf[3]; strftime(mbuf, sizeof(mbuf), "%M", &ti);
  char sbuf[3]; strftime(sbuf, sizeof(sbuf), "%S", &ti);
  char apbuf[3]; strftime(apbuf, sizeof(apbuf), "%p", &ti);
  
  lv_label_set_text(lbl_hr, hbuf);
  lv_label_set_text(lbl_col1, ":");
  lv_label_set_text(lbl_min, mbuf);
  lv_label_set_text(lbl_col2, config.show_seconds ? ":" : "");
  
  if (config.show_seconds) {
    char secA[6];
    snprintf(secA, sizeof(secA), "%s %s", sbuf, apbuf);
    lv_label_set_text(lbl_secA, secA);
  } else {
    lv_label_set_text(lbl_secA, apbuf);
  }
  
  if (config.show_date) {
    char dbuf[40];
    strftime(dbuf, sizeof(dbuf), "%a, %b %d", &ti);
    lv_label_set_text(date_label, dbuf);
  }
  
  battery_voltage = amoled.getBattVoltage() / 1000.0;
  int batt_pct = (int)((battery_voltage - 3.0) / 1.2 * 100.0);
  batt_pct = constrain(batt_pct, 0, 100);
  
  char status[64];
  if (is_charging()) {
    snprintf(status, sizeof(status), "chg %d%% - %s", batt_pct, WiFi.localIP().toString().c_str());
  } else {
    snprintf(status, sizeof(status), "bat %d%% - %s", batt_pct, WiFi.localIP().toString().c_str());
  }
  lv_label_set_text(status_label, status);
}

void update_brightness(lv_timer_t*) {
  target_brightness = calculate_target_brightness();
  if (current_brightness < target_brightness) current_brightness++;
  else if (current_brightness > target_brightness) current_brightness--;
  amoled.setBrightness(current_brightness);
}

void check_wifi(lv_timer_t*) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 Reconnecting WiFi...");
    connect_wifi();
    if (WiFi.status() == WL_CONNECTED) setup_time();
  }
}

// ============================================================================
// WEB INTERFACE - Include after all declarations
// ============================================================================

#include "web_interface.h"

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  T-Display S3 AMOLED Flip Clock       ║");
  Serial.println("║  Phase 2: With Web Interface          ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  
  // Load config from NVS
  load_config();
  
  if (!amoled.begin()) {
    Serial.println("❌ AMOLED init failed!");
    while(1) delay(1000);
  }
  amoled.setBrightness(config.day_brightness);
  
  configure_battery_charging();
  
  beginLvglHelper(amoled, true);
  setup_ui();
  
  lv_obj_add_event_cb(lv_scr_act(), handle_touch, LV_EVENT_CLICKED, nullptr);
  
  if (connect_wifi()) {
    setup_time();
    setup_web_server(); // NEW! Start web server
  }
  
  lv_timer_create(update_display, 1000, nullptr);
  lv_timer_create(update_brightness, 100, nullptr);
  lv_timer_create(check_wifi, 60000, nullptr);
  
  Serial.println("✓ Clock ready!");
  Serial.println("👆 Tap screen to cycle brightness");
  Serial.println("🌐 Web interface: http://clock.local");
  Serial.println();
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  lv_timer_handler();
  handle_web_server(); // NEW! Handle web requests
  delay(5);
  
  static int last_day = -1;
  struct tm ti;
  if (getLocalTime(&ti) && ti.tm_mday != last_day) {
    last_day = ti.tm_mday;
    update_sun_times();
  }
}

