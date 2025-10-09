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

// ===== Configuration =====
uint8_t controllerMAC[] = {0xEC, 0xE3, 0x34, 0xC0, 0x33, 0xC0};
esp_now_peer_info_t peerInfo = {0};
static uint8_t nextMessageId = 1;

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
static bool sendMessageToController(CommandType cmd, int32_t param = STEPPER_PARAM_UNUSED) {
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
      // update your UI with (int)msg.param
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
}

void sendMessage(CommandType cmd, int param = STEPPER_PARAM_UNUSED, int messageId = 0) {
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

static void to_camel_case(const char *in, char *out, size_t out_sz) {
  if (!in || !out || out_sz == 0) return;
  bool new_word = true;
  size_t o = 0;
  for (size_t i = 0; in[i] != '\0' && o + 1 < out_sz; ++i) {
    char c = in[i];
    if (c == '_') {
      new_word = true;
      continue;
    }
    if (new_word) {
      if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
      out[o++] = c;
      new_word = false;
    } else {
      if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
      out[o++] = c;
    }
  }
  out[o] = '\0';
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
    char buf[32];
    to_camel_case(cmd_to_str(mb->command), buf, sizeof(buf));
    Serial.printf("%s\n", buf);
    sendMessageToController(mb->command);
  } else if (code == LV_EVENT_RELEASED) {
    Serial.println("Stop");
    sendMessageToController(CMD_STOP);
  } else if (code == LV_EVENT_PRESS_LOST) {
    sendMessageToController(CMD_STOP);
  }
}

// ===== Slider Helper Functions =====
static void update_slider_label_pos(lv_obj_t *slider, lv_obj_t *label) {
  if (!slider || !label) return;
  const int KNOB_SIZE = 14;
  int value = lv_slider_get_value(slider);
  int sx = lv_obj_get_x(slider);
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
    int btn_y = (row == 0) ? top_row_y : bottom_row_y;
    
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
static bool autosave_on = false;

static void autosave_toggle_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_current_target(e);
  autosave_on = !autosave_on;
  if (autosave_on) {
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    Serial.println("AutoSave On");
  } else {
    lv_obj_clear_state(btn, LV_STATE_CHECKED);
    Serial.println("AutoSave Off");
  }
}

static void power_btn_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("Reset");
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
  lv_obj_t *ab_label = lv_label_create(autosave_btn);
  lv_label_set_text(ab_label, LV_SYMBOL_SAVE);
  lv_obj_center(ab_label);
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

// ===== UI Builder =====
void build_ui() {
  init_button_styles();
  create_move_buttons();
  create_sliders();
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

  display_touch_init();
  build_ui();
  
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
  lv_tick_inc(5);
  lv_timer_handler();
  delay(5);
}