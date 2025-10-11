// StepperGUI - PlatformIO Version
// Converted from Arduino IDE to PlatformIO
// Target: JC4827W543C ESP32-S3 with 4.3" Display

#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "Touch_GT911.h"
#include <Wire.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Arial_Arrows_14.h"
#include "stepper_commands.h"
#include "stepper_helpers.h"
#include <Preferences.h>

// ===== Configuration =====
uint8_t controllerMAC[] = {0xEC, 0xE3, 0x34, 0xC0, 0x33, 0xC0};
esp_now_peer_info_t peerInfo = {0};
static uint8_t nextMessageId = 1;

// ===== Position Storage System =====
// Position Array Structure:
// [0][0] = last_band, [0][1] = last_mode, [0][2] = autosave, [0][3] = current_position 
// [1][0] = 10m CW, [1][1] = 10m SSB, [1][2] = 10m FT4, [1][3] = 10m FT8
// [2][0] = 12m CW, [2][1] = 12m SSB, [2][2] = 12m FT4, [2][3] = 12m FT8
// [3][0] = 15m CW, [3][1] = 15m SSB, [3][2] = 15m FT4, [3][3] = 15m FT8
// [4][0] = 17m CW, [4][1] = 17m SSB, [4][2] = 17m FT4, [4][3] = 17m FT8
// [5][0] = 20m CW, [5][1] = 20m SSB, [5][2] = 20m FT4, [5][3] = 20m FT8
// [6][0] = 30m CW, [6][1] = 30m SSB, [6][2] = 30m FT4, [6][3] = 30m FT8
#define POSITION_ROWS 7
#define POSITION_COLS 4
#define PREFS_NAMESPACE "magloop"

static int32_t positionArray[POSITION_ROWS][POSITION_COLS];
static int current_band_index = 0;  // Index into bandButtons array
static int current_mode_index = 0;  // Index into modeButtons array
static int32_t current_stepper_position = 0;
static bool position_system_initialized = false;
static bool autosave_on = false;
static Preferences preferences;
static bool position_update_pending = false;
static int32_t pending_position = 0;
static int8_t last_rssi = -100;
static bool signal_update_pending = false;

// Forward declarations for position storage functions
static void initialize_position_array();
static bool save_positions_to_file();
static bool save_single_position(int band_index, int mode_index, int32_t position);
static bool load_positions_from_file();
static void update_current_position(int32_t position);
static void change_band_mode(int band_index, int mode_index);
static void update_position_display(int32_t position);
static void update_signal_display(int8_t rssi);

// ===== Display Pins (defined in platformio.ini) =====
// DISP_CS, DISP_SCK, DISP_D0, DISP_D1, DISP_D2, DISP_D3, GFX_BL

// ===== Touch Config (defined in platformio.ini) =====
// TOUCH_WIDTH, TOUCH_HEIGHT, TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RES

// ===== Move Button UI Config =====
#define MOVE_BTN_COLOR_RELEASED 0x0000FF
#define MOVE_BTN_COLOR_PRESSED 0xFF0000
#define MOVE_BTN_WIDTH 60
#define MOVE_BTN_HEIGHT 30
#define MOVE_BTN_PADDING 10
#define MOVE_BTN_GAP_Y 8

// ===== ESP-NOW Functions =====
static bool send_message_to_controller(CommandType cmd, int32_t param = STEPPER_PARAM_UNUSED) {
  Message msg;
  msg.messageId = nextMessageId++;
  msg.command = cmd;
  msg.param = param;
  esp_err_t res = esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
  if (res != ESP_OK) {
    Serial.printf("esp_now_send failed: %d\n", (int)res);
    return false;
  }
  return true;
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (mac) {
    Serial.printf("GUI onDataRecv from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  
  // Get RSSI from WiFi - this is an approximation since ESP-NOW doesn't directly provide RSSI
  last_rssi = WiFi.RSSI(); // This gets the last WiFi RSSI, not perfect but indicative
  signal_update_pending = true;
  
  // Get RSSI from WiFi - this is an approximation since ESP-NOW doesn't directly provide RSSI
  last_rssi = WiFi.RSSI(); // This gets the last WiFi RSSI, not perfect but indicative
  signal_update_pending = true;

  if (!data || len < (int)sizeof(Message)) {
    Serial.printf("GUI onDataRecv: bad len %d\n", len);
    return;
  }

  Message msg;
  memcpy(&msg, data, sizeof(msg));

  Serial.printf("GUI onDataRecv: cmd=%u param=%ld id=%u\n",
                (unsigned)msg.command, (long)msg.param, (unsigned)msg.messageId);

  switch (msg.command) {
    case CMD_POSITION:
      // Don't update UI directly in interrupt context - defer to main loop
      pending_position = msg.param;
      position_update_pending = true;
      Serial.printf("Position received: %d (deferred)\n", (int)msg.param);
      break;
    case CMD_RESET:
      // StepperController is requesting GUI to reset
      Serial.println("CMD_RESET received from StepperController - rebooting GUI in 2 seconds...");
      delay(2000);  // Brief delay to allow serial message to be sent
      ESP.restart();  // Reboot the ESP32
      break;
    case CMD_ACK:
      // handle ack
      break;
    default:
      break;
  }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.printf("GUI onDataSent status=%s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
// I couldn't see the mac addresss in setup so put here temporarily
  Serial.print("My MAC address is ");
  Serial.println(WiFi.macAddress());

}

void send_message(CommandType cmd, int param = STEPPER_PARAM_UNUSED, int messageId = 0) {
  Message msg;
  msg.messageId = messageId;
  msg.command = cmd;
  msg.param = param;
  esp_now_send(controllerMAC, (uint8_t *)&msg, sizeof(msg));
}

// ===== Move Button Struct & Data =====
typedef struct {
  const char *arrow;
  const char *text;
  CommandType command;
} MoveButton;

MoveButton moveButtons[] = {
  { "↑", "Slow", CMD_UP_SLOW },
  { "↑", "Med", CMD_UP_MEDIUM },
  { "↑", "Fast", CMD_UP_FAST },
  { "↓", "Slow", CMD_DOWN_SLOW },
  { "↓", "Med", CMD_DOWN_MEDIUM },
  { "↓", "Fast", CMD_DOWN_FAST }
};

static int move_btn_indices[sizeof(moveButtons) / sizeof(moveButtons[0])];

// ===== Radio Button Struct & Data =====
typedef struct {
  const char *label;
} Button;

Button modeButtons[] = { { "CW" }, { "SSB" }, { "FT4" }, { "FT8" } };
Button bandButtons[] = { { "10" }, { "12" }, { "15" }, { "17" }, { "20" }, { "30" } };

static lv_obj_t *last_mode_btn = NULL;
static lv_obj_t *last_band_btn = NULL;

// Radio button dimensions
#define modeBtn_width 50
#define modeBtn_height 30
#define bandBtn_width 30
#define bandBtn_height 30

// ===== Display & Touch Objects =====
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  DISP_CS, DISP_SCK, DISP_D0, DISP_D1, DISP_D2, DISP_D3);
Arduino_NV3041A *gfx = new Arduino_NV3041A(
  bus, GFX_NOT_DEFINED, PORTRAIT_ROTATION, true);

Touch_GT911 touchController(
  TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RES,
  TOUCH_WIDTH, TOUCH_HEIGHT);

// ===== LVGL Globals =====
uint32_t screenWidth, screenHeight, bufSize_px;
lv_display_t *disp;
lv_color_t *disp_draw_buf;
lv_obj_t *g_autosave_btn = NULL;
lv_obj_t *g_position_bar = NULL;
lv_obj_t *g_position_label = NULL;
lv_obj_t *g_signal_slider = NULL;
lv_obj_t *g_signal_label = NULL;

// ===== Slider User Data =====
static const char *slider_letters[] = { "S", "M", "F" };
typedef struct {
  const char *letter;
  lv_obj_t *label;
} SliderInfo;
static SliderInfo sliderInfos[3];

// ===== LVGL Callbacks =====

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  touchController.read();

  if (touchController.isTouched && touchController.touches > 0) {
    int16_t tx, ty;
    if (PORTRAIT_ROTATION == 1) {
      tx = touchController.points[0].y;
      ty = TOUCH_WIDTH - touchController.points[0].x;
    } else {
      tx = TOUCH_HEIGHT - touchController.points[0].y;
      ty = touchController.points[0].x;
    }
    data->point.x = tx;
    data->point.y = ty;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ===== Command to String Helper =====
static const char *cmd_to_str(CommandType cmd) {
  switch (cmd) {
    case CMD_STOP: return "STOP";
    case CMD_UP_SLOW: return "UP_SLOW";
    case CMD_UP_MEDIUM: return "UP_MEDIUM";
    case CMD_UP_FAST: return "UP_FAST";
    case CMD_DOWN_SLOW: return "DOWN_SLOW";
    case CMD_DOWN_MEDIUM: return "DOWN_MEDIUM";
    case CMD_DOWN_FAST: return "DOWN_FAST";
    case CMD_MOVE_TO: return "MOVE_TO";
    case CMD_MOVE_TO_DOWN_LIMIT: return "MOVE_TO_DOWN_LIMIT";
    case CMD_DOWN_LIMIT_STATUS: return "DOWN_LIMIT_STATUS";
    case CMD_REQUEST_DOWN_STOP: return "REQUEST_DOWN_STOP";
    case CMD_GET_POSITION: return "GET_POSITION";
    case CMD_RESET: return "RESET";
    case CMD_POSITION: return "POSITION";
    case CMD_UP_LIMIT_OK: return "UP_LIMIT_OK";
    case CMD_UP_LIMIT_TRIP: return "UP_LIMIT_TRIP";
    case CMD_DOWN_LIMIT_OK: return "DOWN_LIMIT_OK";
    case CMD_DOWN_LIMIT_TRIP: return "DOWN_LIMIT_TRIP";
    case CMD_HEARTBEAT: return "HEARTBEAT";
    case CMD_ACK: return "ACK";
    default: return "UNKNOWN_CMD";
  }
}



// ===== Move Button Event Callback =====
static void move_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_current_target(e);
  void *ud = lv_event_get_user_data(e);
  MoveButton *mb = NULL;
  static MoveButton *test_mb_ptr = NULL;
  
  if (ud) {
    int idx = *((int *)ud);
    if (idx >= 0 && idx < (int)(sizeof(moveButtons) / sizeof(moveButtons[0]))) {
      mb = &moveButtons[idx];
    } else if (idx < 0) {
      static MoveButton test_mb_local = { ">", "Test", CMD_UP_SLOW };
      test_mb_ptr = &test_mb_local;
      mb = test_mb_ptr;
    }
  }

  if (!btn || !mb) return;

  if (code == LV_EVENT_PRESSED) {
    Serial.printf("%s\n", cmd_to_str(mb->command));
    send_message_to_controller(mb->command);
  } else if (code == LV_EVENT_RELEASED) {
    Serial.println("Stop");
    send_message_to_controller(CMD_STOP);
  } else if (code == LV_EVENT_PRESS_LOST) {
    send_message_to_controller(CMD_STOP);
  }
}

// ===== Radio Button Event Callbacks =====
void band_radio_button_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  const char *label = (const char *)lv_event_get_user_data(e);

  if (code == LV_EVENT_PRESSED) {
    if (last_band_btn && last_band_btn != btn) {
      lv_obj_clear_state(last_band_btn, LV_STATE_CHECKED);
    }
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    last_band_btn = btn;
    
    // Find band index
    int band_index = -1;
    for (int i = 0; i < sizeof(bandButtons) / sizeof(bandButtons[0]); i++) {
      if (strcmp(bandButtons[i].label, label) == 0) {
        band_index = i;
        break;
      }
    }
    
    if (band_index >= 0) {
      change_band_mode(band_index, current_mode_index);
    }
    
    Serial.printf("Band selected: %s (index: %d)\n", label, band_index);
    // TODO: Add ESP-NOW transmission for band change
  }
}

void mode_button_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  const char *label = (const char *)lv_event_get_user_data(e);

  if (code == LV_EVENT_PRESSED) {
    if (last_mode_btn && last_mode_btn != btn) {
      lv_obj_clear_state(last_mode_btn, LV_STATE_CHECKED);
    }
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    last_mode_btn = btn;
    
    // Find mode index
    int mode_index = -1;
    for (int i = 0; i < sizeof(modeButtons) / sizeof(modeButtons[0]); i++) {
      if (strcmp(modeButtons[i].label, label) == 0) {
        mode_index = i;
        break;
      }
    }
    
    if (mode_index >= 0) {
      change_band_mode(current_band_index, mode_index);
    }
    
    Serial.printf("Mode selected: %s (index: %d)\n", label, mode_index);
    // TODO: Add ESP-NOW transmission for mode change
  }
}

// ===== Position Storage Functions =====
static void initialize_position_array() {
  // Clear the array
  memset(positionArray, 0, sizeof(positionArray));
  
  // Set default values
  positionArray[0][0] = 0; // last_band (10m = index 0)
  positionArray[0][1] = 0; // last_mode (CW = index 0)
  positionArray[0][2] = 0; // autosave (false)
  positionArray[0][3] = random(0, 1600); // current_position (random for testing)
  
  // Initialize all band/mode positions with random values for testing
  Serial.println("Initializing positions with random test values:");
  for (int i = 1; i < POSITION_ROWS; i++) {
    for (int j = 0; j < POSITION_COLS; j++) {
      positionArray[i][j] = random(0, 1600);
      Serial.printf("  Band %d, Mode %d: %d\n", i-1, j, (int)positionArray[i][j]);
    }
  }
}

static bool save_positions_to_file() {
  preferences.begin(PREFS_NAMESPACE, false);
  
  // Save position array to preferences
  for (int i = 0; i < POSITION_ROWS; i++) {
    for (int j = 0; j < POSITION_COLS; j++) {
      char key[16];
      snprintf(key, sizeof(key), "pos_%d_%d", i, j);
      preferences.putInt(key, positionArray[i][j]);
    }
  }
  
  preferences.end();
  Serial.println("All positions saved to preferences");
  return true;
}

static bool save_single_position(int band_index, int mode_index, int32_t position) {
  if (band_index < 0 || band_index >= 6 || mode_index < 0 || mode_index >= 4) {
    Serial.printf("Invalid band/mode index: %d/%d\n", band_index, mode_index);
    return false;
  }
  
  preferences.begin(PREFS_NAMESPACE, false);
  
  // Save the specific band/mode position
  char key[16];
  snprintf(key, sizeof(key), "pos_%d_%d", band_index + 1, mode_index);
  preferences.putInt(key, position);
  
  // Also update the current position and state
  preferences.putInt("pos_0_0", band_index);  // last_band
  preferences.putInt("pos_0_1", mode_index);  // last_mode  
  preferences.putInt("pos_0_2", autosave_on ? 1 : 0);  // autosave
  preferences.putInt("pos_0_3", position);  // current_position
  
  preferences.end();
  
  Serial.printf("Position saved for %s/%s: %d\n", 
                bandButtons[band_index].label, modeButtons[mode_index].label, (int)position);
  return true;
}

static bool load_positions_from_file() {
  preferences.begin(PREFS_NAMESPACE, true); // Read-only mode
  
  // Load position array from preferences
  bool found_data = false;
  for (int i = 0; i < POSITION_ROWS; i++) {
    for (int j = 0; j < POSITION_COLS; j++) {
      char key[16];
      snprintf(key, sizeof(key), "pos_%d_%d", i, j);
      if (preferences.isKey(key)) {
        positionArray[i][j] = preferences.getInt(key, 0);
        found_data = true;
      } else {
        positionArray[i][j] = 0;
      }
    }
  }
  
  preferences.end();
  
  if (!found_data) {
    Serial.println("No saved positions found, using defaults");
    initialize_position_array();
    return true;
  }
  
  // Restore state from loaded data
  current_band_index = positionArray[0][0];
  current_mode_index = positionArray[0][1];
  autosave_on = (positionArray[0][2] != 0);
  current_stepper_position = positionArray[0][3];
  
  Serial.println("Positions loaded from preferences");
  Serial.printf("Last band: %d, Last mode: %d, AutoSave: %s\n", 
                current_band_index, current_mode_index, autosave_on ? "On" : "Off");
  return true;
}

static void update_current_position(int32_t position) {
  current_stepper_position = position;
  positionArray[0][3] = position;
  
  // Update position display on screen (only if UI is ready)
  update_position_display(position);
  
  // Also update the current band/mode position
  if (current_band_index >= 0 && current_band_index < 6 && 
      current_mode_index >= 0 && current_mode_index < 4) {
    positionArray[current_band_index + 1][current_mode_index] = position;
    
    if (autosave_on) {
      // Only save the specific position that changed
      save_single_position(current_band_index, current_mode_index, position);
    }
  }
}

static void change_band_mode(int band_index, int mode_index) {
  // Save current position to the old band/mode before switching
  if (current_band_index >= 0 && current_band_index < 6 && 
      current_mode_index >= 0 && current_mode_index < 4) {
    positionArray[current_band_index + 1][current_mode_index] = current_stepper_position;
    Serial.printf("Saved current position %d to old %s/%s\n", 
                  (int)current_stepper_position,
                  bandButtons[current_band_index].label, 
                  modeButtons[current_mode_index].label);
    
    // Save to preferences if autosave is on
    if (autosave_on) {
      save_single_position(current_band_index, current_mode_index, current_stepper_position);
    }
  }
  
  // Update indices to new band/mode
  current_band_index = band_index;
  current_mode_index = mode_index;
  
  // Update position array header
  positionArray[0][0] = band_index;
  positionArray[0][1] = mode_index;
  positionArray[0][2] = autosave_on ? 1 : 0;
  
  // Get stored position for the new band/mode combination
  if (band_index >= 0 && band_index < 6 && mode_index >= 0 && mode_index < 4) {
    int32_t stored_position = positionArray[band_index + 1][mode_index];
    Serial.printf("Band/Mode changed to %s/%s, moving to stored position: %d\n", 
                  bandButtons[band_index].label, modeButtons[mode_index].label, (int)stored_position);
    
    // Send move command to stepper to go to stored position
    send_message_to_controller(CMD_MOVE_TO, stored_position);
    
    // Update current position immediately (will be confirmed when stepper responds)
    current_stepper_position = stored_position;
    positionArray[0][3] = stored_position;
    update_position_display(stored_position);
    
    if (autosave_on) {
      // Save the state change (current band/mode)
      preferences.begin(PREFS_NAMESPACE, false);
      preferences.putInt("pos_0_0", band_index);  // last_band
      preferences.putInt("pos_0_1", mode_index);  // last_mode
      preferences.putInt("pos_0_2", autosave_on ? 1 : 0);  // autosave
      preferences.putInt("pos_0_3", stored_position);  // current_position
      preferences.end();
      Serial.println("Band/Mode state saved to preferences");
    }
  }
}

// ===== Position Display Functions =====
static void update_position_display(int32_t position) {
  // Safety check - only update if UI is initialized
  if (!g_position_label || !g_position_bar) {
    Serial.printf("Position display not initialized yet, position: %d\n", (int)position);
    return;
  }
  
  char pos_text[32];
  snprintf(pos_text, sizeof(pos_text), "Position: %d", (int)position);
  lv_label_set_text(g_position_label, pos_text);
  
  // Debug: Verify text was set
  Serial.printf("Updated label text to: %s\n", lv_label_get_text(g_position_label));
  
  // Force label to refresh
  lv_obj_invalidate(g_position_label);
  
  // Assume position range from 0 to 10000 (adjust as needed)
  const int32_t MAX_POSITION = 10000;
  const int32_t MIN_POSITION = 0;
  
  // Calculate percentage (0-100)
  int32_t clamped_pos = position;
  if (clamped_pos < MIN_POSITION) clamped_pos = MIN_POSITION;
  if (clamped_pos > MAX_POSITION) clamped_pos = MAX_POSITION;
  
  int percentage = (int)((clamped_pos - MIN_POSITION) * 100 / (MAX_POSITION - MIN_POSITION));
  lv_bar_set_value(g_position_bar, percentage, LV_ANIM_ON);
  
    Serial.printf("Bar updated to %d%% (pos: %d)\n", percentage, (int)position);
}

static void update_signal_display(int8_t rssi) {
  // Safety check - only update if UI is initialized
  if (!g_signal_label || !g_signal_slider) {
    Serial.printf("Signal display not initialized yet, RSSI: %d\n", rssi);
    return;
  }
  
  char signal_text[32];
  snprintf(signal_text, sizeof(signal_text), "Signal: %d dBm", rssi);
  lv_label_set_text(g_signal_label, signal_text);
  
  // Debug: Verify text was set
  Serial.printf("Updated signal text to: %s\n", lv_label_get_text(g_signal_label));
  
  // Force label to refresh
  lv_obj_invalidate(g_signal_label);
  
  // Convert RSSI to percentage (RSSI typically ranges from -100 to -30 dBm)
  const int8_t MIN_RSSI = -100;  // Very weak signal
  const int8_t MAX_RSSI = -30;   // Very strong signal
  
  // Calculate percentage (0-100)
  int8_t clamped_rssi = rssi;
  if (clamped_rssi < MIN_RSSI) clamped_rssi = MIN_RSSI;
  if (clamped_rssi > MAX_RSSI) clamped_rssi = MAX_RSSI;
  
  int percentage = (int)((clamped_rssi - MIN_RSSI) * 100 / (MAX_RSSI - MIN_RSSI));
  lv_slider_set_value(g_signal_slider, percentage, LV_ANIM_ON);
  
  Serial.printf("Signal updated to %d%% (RSSI: %d dBm)\n", percentage, rssi);
}

// ===== Slider Helper Functions ===
static void update_slider_label_pos(lv_obj_t *slider, lv_obj_t *label) {
  if (!slider || !label) return;
  const int KNOB_SIZE = 14;
  int value = lv_slider_get_value(slider);
  int sy = lv_obj_get_y(slider);
  int sh = lv_obj_get_height(slider);

  int knob_center_y = sy + ((100 - value) * (sh - KNOB_SIZE) / 100) + (KNOB_SIZE / 2);
  int slider_center_y = sy + (sh / 2);
  int offset_y = knob_center_y - slider_center_y;

  lv_obj_align_to(label, slider, LV_ALIGN_CENTER, 0, offset_y);
}

static void slider_event_cb(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
  SliderInfo *info = (SliderInfo *)lv_event_get_user_data(e);
  int value = lv_slider_get_value(slider);

  char buf[8];
  snprintf(buf, sizeof(buf), "%s %d", info->letter, value);
  lv_label_set_text(info->label, buf);
  Serial.printf("%s slider value: %d\n", info->letter, value);

  update_slider_label_pos(slider, info->label);
}

// ===== Style Objects =====
lv_style_t style_move_default;
lv_style_t style_move_pressed;
lv_style_t style_slider_track;
lv_style_t style_slider_indicator;
lv_style_t style_slider_knob;
lv_style_t style_toggle_on;
lv_style_t style_toggle_off;
lv_style_t style_btn;
lv_style_t style_btn_checked;
lv_style_t style_position_bar;
lv_style_t style_signal_slider;

void init_button_styles(void) {
  // Default (released) style
  lv_style_init(&style_move_default);
  lv_style_set_bg_opa(&style_move_default, LV_OPA_COVER);
  lv_style_set_bg_color(&style_move_default, lv_color_hex(MOVE_BTN_COLOR_RELEASED));
  lv_style_set_radius(&style_move_default, 6);
  lv_style_set_pad_all(&style_move_default, 4);
  lv_style_set_border_width(&style_move_default, 0);
  lv_style_set_text_color(&style_move_default, lv_color_hex(0xFFFFFF));

  // Pressed style
  lv_style_init(&style_move_pressed);
  lv_style_set_bg_opa(&style_move_pressed, LV_OPA_COVER);
  lv_style_set_bg_color(&style_move_pressed, lv_color_hex(MOVE_BTN_COLOR_PRESSED));
  lv_style_set_radius(&style_move_pressed, 6);
  lv_style_set_pad_all(&style_move_pressed, 4);
  lv_style_set_border_width(&style_move_pressed, 0);
  lv_style_set_text_color(&style_move_pressed, lv_color_hex(0xFFFFFF));

  // Slider styles
  lv_style_init(&style_slider_track);
  lv_style_set_bg_opa(&style_slider_track, LV_OPA_COVER);
  lv_style_set_bg_color(&style_slider_track, lv_color_hex(0x000000));
  lv_style_set_radius(&style_slider_track, 4);

  lv_style_init(&style_slider_indicator);
  lv_style_set_bg_opa(&style_slider_indicator, LV_OPA_COVER);
  lv_style_set_bg_color(&style_slider_indicator, lv_color_hex(0xFF0000));
  lv_style_set_radius(&style_slider_indicator, 4);

  lv_style_init(&style_slider_knob);
  lv_style_set_bg_opa(&style_slider_knob, LV_OPA_COVER);
  lv_style_set_bg_color(&style_slider_knob, lv_color_hex(0xFFFF00));
  lv_style_set_radius(&style_slider_knob, 0);
  lv_style_set_width(&style_slider_knob, 14);
  lv_style_set_height(&style_slider_knob, 14);

  // Toggle styles
  lv_style_init(&style_toggle_off);
  lv_style_set_bg_opa(&style_toggle_off, LV_OPA_COVER);
  lv_style_set_bg_color(&style_toggle_off, lv_color_hex(0x0000FF));
  lv_style_set_radius(&style_toggle_off, 6);
  lv_style_set_text_color(&style_toggle_off, lv_color_hex(0xFFFFFF));

  lv_style_init(&style_toggle_on);
  lv_style_set_bg_opa(&style_toggle_on, LV_OPA_COVER);
  lv_style_set_bg_color(&style_toggle_on, lv_color_hex(0xFF0000));
  lv_style_set_radius(&style_toggle_on, 6);
  lv_style_set_text_color(&style_toggle_on, lv_color_hex(0xFFFFFF));

  // Radio button styles
  lv_style_init(&style_btn);
  lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
  lv_style_set_bg_color(&style_btn, lv_color_make(0, 0, 128));
  lv_style_set_text_color(&style_btn, lv_color_white());
  lv_style_set_radius(&style_btn, 6);

  lv_style_init(&style_btn_checked);
  lv_style_set_bg_opa(&style_btn_checked, LV_OPA_COVER);
  lv_style_set_bg_color(&style_btn_checked, lv_color_make(255, 0, 0));
  lv_style_set_text_color(&style_btn_checked, lv_color_white());
  lv_style_set_radius(&style_btn_checked, 6);

  // Position bar style
  lv_style_init(&style_position_bar);
  lv_style_set_bg_opa(&style_position_bar, LV_OPA_COVER);
  lv_style_set_bg_color(&style_position_bar, lv_color_hex(0x00FF00)); // Green
  lv_style_set_radius(&style_position_bar, 4);

  // Signal strength slider style
  lv_style_init(&style_signal_slider);
  lv_style_set_bg_opa(&style_signal_slider, LV_OPA_COVER);
  lv_style_set_bg_color(&style_signal_slider, lv_color_hex(0xFF8000)); // Orange
  lv_style_set_radius(&style_signal_slider, 4);
}

void create_move_buttons() {
  const int numMoves = sizeof(moveButtons) / sizeof(moveButtons[0]);
  const int cols = 3;
  const int spacing_x = 20, spacing_y = 5;
  const int btn_w = MOVE_BTN_WIDTH, btn_h = MOVE_BTN_HEIGHT;
  const int startX = (LV_HOR_RES - (cols * btn_w + (cols - 1) * spacing_x)) / 2;
  const int startY = 390;

  for (int i = 0; i < numMoves; i++) {
    int row = i / cols;
    int col = i % cols;
    int x = startX + col * (btn_w + spacing_x);
    int y = startY + row * (btn_h + spacing_y);

    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, x, y);

    lv_obj_add_style(btn, &style_move_default, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &style_move_pressed, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t *row_cont = lv_obj_create(btn);
    lv_obj_set_size(row_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row_cont, 2, 0);
    lv_obj_clear_flag(row_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(row_cont, LV_OPA_TRANSP, 0);
    lv_obj_center(row_cont);

    lv_obj_t *arrow_label = lv_label_create(row_cont);
    lv_label_set_text(arrow_label, moveButtons[i].arrow);
    lv_obj_set_style_text_font(arrow_label, &Arial_Arrows_14, 0);
    lv_obj_clear_flag(arrow_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(arrow_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(arrow_label, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *text_label = lv_label_create(row_cont);
    lv_label_set_text(text_label, moveButtons[i].text);
    lv_obj_set_style_text_font(text_label, LV_FONT_DEFAULT, 0);
    lv_obj_clear_flag(text_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(text_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(text_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(row_cont);
    
    move_btn_indices[i] = i;
    lv_obj_add_event_cb(btn, move_btn_event_cb, LV_EVENT_ALL, &move_btn_indices[i]);
  }
}

void create_sliders() {
  // Test button
  static int test_idx = -1;
  lv_obj_t *test_btn = lv_btn_create(lv_scr_act());
  lv_obj_remove_style_all(test_btn);
  lv_obj_set_size(test_btn, 80, 40);
  lv_obj_set_pos(test_btn, 10, 10);
  lv_obj_add_style(test_btn, &style_move_default, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_style(test_btn, &style_move_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_t *tlabel = lv_label_create(test_btn);
  lv_label_set_text(tlabel, "TEST");
  lv_obj_center(tlabel);
  test_idx = -1;
  lv_obj_add_event_cb(test_btn, move_btn_event_cb, LV_EVENT_ALL, &test_idx);

  const int cols = 3;
  const int total_sliders = cols * 2;
  const int SLIDER_WIDTH = 5;
  const int SLIDER_GAP = 5;
  int screen_w = lv_disp_get_hor_res(NULL);
  int screen_h = lv_disp_get_ver_res(NULL);

  int usable_w = screen_w - SLIDER_WIDTH - SLIDER_GAP;
  int spacing_x = (usable_w - (cols * MOVE_BTN_WIDTH)) / (cols + 1);
  int x_offset = 5;
  int bottom_row_y = screen_h - MOVE_BTN_HEIGHT - MOVE_BTN_PADDING;
  int top_row_y = bottom_row_y - MOVE_BTN_HEIGHT - MOVE_BTN_GAP_Y;

  for (int i = 0; i < total_sliders; i++) {
    int row = (i < cols) ? 0 : 1;
    int col = i % cols;
    int btn_x = SLIDER_WIDTH + SLIDER_GAP + spacing_x + col * (MOVE_BTN_WIDTH + spacing_x) - x_offset;
    
    if (row == 0) {
      lv_obj_t *slider = lv_slider_create(lv_scr_act());
      lv_obj_set_size(slider, SLIDER_WIDTH, MOVE_BTN_HEIGHT * 2 + MOVE_BTN_GAP_Y);
      lv_obj_set_pos(slider, btn_x - SLIDER_WIDTH - SLIDER_GAP, top_row_y);
      lv_slider_set_range(slider, 0, 100);
      lv_slider_set_value(slider, 50, LV_ANIM_OFF);
      
      lv_obj_add_style(slider, &style_slider_track, LV_PART_MAIN);
      lv_obj_add_style(slider, &style_slider_indicator, LV_PART_INDICATOR);
      lv_obj_add_style(slider, &style_slider_knob, LV_PART_KNOB);

      lv_obj_t *slabel = lv_label_create(lv_scr_act());
      char init_buf[8];
      snprintf(init_buf, sizeof(init_buf), "%s %d", slider_letters[col], 50);
      lv_label_set_text(slabel, init_buf);
      lv_obj_align_to(slabel, slider, LV_ALIGN_OUT_TOP_MID, 0, -2);

      update_slider_label_pos(slider, slabel);

      sliderInfos[col].letter = slider_letters[col];
      sliderInfos[col].label = slabel;
      lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, &sliderInfos[col]);
    }
  }
}

// ===== AutoSave & Power Buttons =====

static void autosave_toggle_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_current_target(e);
  autosave_on = !autosave_on;
  
  // Update position array
  positionArray[0][2] = autosave_on ? 1 : 0;
  
  if (autosave_on) {
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    Serial.println("AutoSave On");
    save_positions_to_file(); // Save immediately when enabled
  } else {
    lv_obj_clear_state(btn, LV_STATE_CHECKED);
    Serial.println("AutoSave Off");
  }
}

static void power_btn_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("Sending CMD_RESET to StepperController...");
    send_message_to_controller(CMD_RESET);
    Serial.println("Reset command sent. Waiting for controller to restart and send reset back...");
  }
}

void create_reset_button() {
  lv_obj_t *autosave_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(autosave_btn, 48, 28);
  lv_obj_align(autosave_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
  lv_obj_add_style(autosave_btn, &style_toggle_off, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_style(autosave_btn, &style_toggle_on, LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_add_event_cb(autosave_btn, autosave_toggle_cb, LV_EVENT_CLICKED, NULL);
  g_autosave_btn = autosave_btn;
  
  // Restore autosave state from position system
  if (position_system_initialized && autosave_on) {
    lv_obj_add_state(autosave_btn, LV_STATE_CHECKED);
  }
  
  lv_obj_t *ab_label = lv_label_create(autosave_btn);
  lv_label_set_text(ab_label, LV_SYMBOL_SAVE);
  lv_obj_center(ab_label);
}

// ===== Radio Button Creators =====
void create_band_radio_buttons() {
  int numBands = sizeof(bandButtons) / sizeof(bandButtons[0]);
  int spacing = 10;
  int totalWidth = numBands * bandBtn_width + (numBands - 1) * spacing;
  int startX = (LV_HOR_RES - totalWidth) / 2;
  int y_offset = 350; // Position just above move buttons

  for (int i = 0; i < numBands; i++) {
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, bandBtn_width, bandBtn_height);
    lv_obj_set_pos(btn, startX + i * (bandBtn_width + spacing), y_offset);
    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_checked, LV_STATE_CHECKED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, bandButtons[i].label);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, band_radio_button_event_cb, LV_EVENT_ALL, (void *)bandButtons[i].label);

    // Restore last selected band from position system
    if (position_system_initialized && i == current_band_index) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
      last_band_btn = btn;
    } else if (!position_system_initialized && i == 0) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
      last_band_btn = btn;
    }
  }
}

void create_mode_radio_buttons() {
  int numModes = sizeof(modeButtons) / sizeof(modeButtons[0]);
  int spacing = 10;
  int totalWidth = numModes * modeBtn_width + (numModes - 1) * spacing;
  int startX = (LV_HOR_RES - totalWidth) / 2;
  int y_offset = 315; // Position between band buttons and move buttons

  for (int i = 0; i < numModes; i++) {
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, modeBtn_width, modeBtn_height);
    lv_obj_set_pos(btn, startX + i * (modeBtn_width + spacing), y_offset);
    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_checked, LV_STATE_CHECKED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, modeButtons[i].label);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, mode_button_event_cb, LV_EVENT_ALL, (void *)modeButtons[i].label);

    // Restore last selected mode from position system
    if (position_system_initialized && i == current_mode_index) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
      last_mode_btn = btn;
    } else if (!position_system_initialized && i == 0) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
      last_mode_btn = btn;
    }
  }
}

void create_power_button() {
  lv_obj_t *power_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(power_btn, 48, 28);
  if (g_autosave_btn) {
    lv_obj_align_to(power_btn, g_autosave_btn, LV_ALIGN_OUT_LEFT_MID, -8, 0);
  } else {
    lv_obj_align(power_btn, LV_ALIGN_TOP_RIGHT, -64, 8);
  }
  lv_obj_add_style(power_btn, &style_toggle_on, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(power_btn, power_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *p_label = lv_label_create(power_btn);
  lv_label_set_text(p_label, LV_SYMBOL_POWER);
  lv_obj_center(p_label);
}

void create_position_display() {
  Serial.println("Creating position display...");
  
  // Create position label in center of screen (very visible location)
  g_position_label = lv_label_create(lv_scr_act());
  if (!g_position_label) {
    Serial.println("Failed to create position label!");
    return;
  }
  lv_label_set_text(g_position_label, "Position: 0");
  
  // Make text very visible with blue background and white text
  lv_obj_set_style_text_color(g_position_label, lv_color_hex(0xFFFFFF), 0);  // White text
  lv_obj_set_style_bg_opa(g_position_label, LV_OPA_COVER, 0);  // Solid background
  lv_obj_set_style_bg_color(g_position_label, lv_color_hex(0x0000FF), 0);  // Blue background
  lv_obj_set_style_pad_all(g_position_label, 5, 0);  // Add padding
  lv_obj_set_style_radius(g_position_label, 3, 0);  // Rounded corners
  
  // Position in center-top area (above all other elements)
  lv_obj_align(g_position_label, LV_ALIGN_TOP_MID, 0, 150);
  
  Serial.printf("Label created at center-top, text: %s\n", lv_label_get_text(g_position_label));
  
  // Create position bar below the label
  g_position_bar = lv_bar_create(lv_scr_act());
  if (!g_position_bar) {
    Serial.println("Failed to create position bar!");
    return;
  }
  lv_obj_set_size(g_position_bar, 200, 15);
  lv_obj_align_to(g_position_bar, g_position_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  lv_bar_set_range(g_position_bar, 0, 100);
  lv_bar_set_value(g_position_bar, 0, LV_ANIM_OFF);
  
  // Style the bar with black background
  lv_obj_add_style(g_position_bar, &style_position_bar, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(g_position_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_position_bar, lv_color_hex(0x000000), LV_PART_MAIN);
  
  Serial.println("Position display created successfully");
  
  // Initialize display with current position
  update_position_display(current_stepper_position);
}

void create_signal_display() {
  Serial.println("Creating signal strength display...");
  
  // Create signal label (below position display)
  g_signal_label = lv_label_create(lv_scr_act());
  if (!g_signal_label) {
    Serial.println("Failed to create signal label!");
    return;
  }
  lv_label_set_text(g_signal_label, "Signal: -- dBm");
  
  // Make text visible with blue background and white text (same as position)
  lv_obj_set_style_text_color(g_signal_label, lv_color_hex(0xFFFFFF), 0);  // White text
  lv_obj_set_style_bg_opa(g_signal_label, LV_OPA_COVER, 0);  // Solid background
  lv_obj_set_style_bg_color(g_signal_label, lv_color_hex(0x0000FF), 0);  // Blue background
  lv_obj_set_style_pad_all(g_signal_label, 5, 0);  // Add padding
  lv_obj_set_style_radius(g_signal_label, 3, 0);  // Rounded corners
  
  // Position below the position display
  lv_obj_align(g_signal_label, LV_ALIGN_TOP_MID, 0, 200);
  
  Serial.printf("Signal label created, text: %s\n", lv_label_get_text(g_signal_label));
  
  // Create signal slider below the label
  g_signal_slider = lv_slider_create(lv_scr_act());
  if (!g_signal_slider) {
    Serial.println("Failed to create signal slider!");
    return;
  }
  lv_obj_set_size(g_signal_slider, 200, 15);
  lv_obj_align_to(g_signal_slider, g_signal_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  lv_slider_set_range(g_signal_slider, 0, 100);
  lv_slider_set_value(g_signal_slider, 0, LV_ANIM_OFF);
  
  // Style the slider (same as position bar, no knob) with black background
  lv_obj_add_style(g_signal_slider, &style_position_bar, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(g_signal_slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_signal_slider, lv_color_hex(0x000000), LV_PART_MAIN);
  
  // Hide the knob by making it transparent and small
  lv_obj_set_style_bg_opa(g_signal_slider, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_width(g_signal_slider, 0, LV_PART_KNOB);
  lv_obj_set_style_height(g_signal_slider, 0, LV_PART_KNOB);
  
  Serial.println("Signal display created successfully");
  
  // Initialize display with current signal strength
  update_signal_display(last_rssi);
}

// ===== UI Builder =====
void build_ui() {
  init_button_styles();
  create_position_display();
  create_signal_display();
  create_move_buttons();
  create_sliders();
  create_band_radio_buttons();
  create_mode_radio_buttons();
  create_reset_button();
  create_power_button();
}

// ===== Display & Touch Initialization =====
void display_touch_init() {
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
    while (true) {}
  }

  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  gfx->fillScreen(RGB565_BLACK);

  touchController.begin();
  Wire.setClock(40000);
  delay(10);

  touchController.setRotation(ROTATION_INVERTED);

  lv_init();

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  bufSize_px = screenWidth * 40;
  size_t buf_bytes = bufSize_px * sizeof(lv_color_t);
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf) {
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_8BIT);
  }
  if (!disp_draw_buf) {
    Serial.println("LVGL disp_draw_buf allocate failed!");
    while (true) {}
  }

  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
}

// ===== Arduino Setup & Loop =====
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("LVGL: %d.%d.%d  (PORTRAIT_ROTATION=%d)\n",
                LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                PORTRAIT_ROTATION);

  // Initialize position storage system
  Serial.println("Initializing position storage system...");
  if (!load_positions_from_file()) {
    Serial.println("Using default position values");
    initialize_position_array();
  }
  position_system_initialized = true;

  display_touch_init();
  Serial.println("Display and touch initialized");
  
  build_ui();
  Serial.println("UI built successfully");
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1) ;
  }

  Serial.print("My MAC address is ");
  Serial.println(WiFi.macAddress());

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);
  
  memcpy(peerInfo.peer_addr, controllerMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }

  Serial.println("Setup done");
}

void loop() {
  // Handle deferred position updates from ESP-NOW interrupt
  if (position_update_pending) {
    position_update_pending = false;
    update_current_position(pending_position);
    Serial.printf("Position updated to: %d\n", (int)pending_position);
  }
  
  // Handle deferred signal strength updates
  if (signal_update_pending) {
    signal_update_pending = false;
    update_signal_display(last_rssi);
  }
  
  lv_tick_inc(5);
  lv_timer_handler();
  delay(5);
}