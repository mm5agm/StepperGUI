
#include "Arial_Arrows_14.h"
#include "Touch_GT911.h"
//#include "fsm/stepper_gui_fsm.hpp"
#include "stepper_commands.h"
#include "stepper_helpers.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <stdint.h>
#include "globals.hpp"
#include "fsm/stepper_gui_fsm.hpp"
// Preferences API for persistent storage
#include <Preferences.h>
static Preferences preferences;

// FSM instance (single global, globally visible)
static fsm::StepperGuiFsm g_fsm;

// FSM send callback: sends correct command for each button (slow, medium, fast,
// up/down)

// ===== Style Objects =====
lv_style_t style_move_default;
lv_style_t style_move_pressed;
lv_style_t style_slider_knob;
// Removed duplicate definition of style_slider_knob

lv_style_t style_toggle_on;
lv_style_t style_toggle_off;
lv_style_t style_btn;
lv_style_t style_btn_checked;
lv_style_t style_position_bar;
lv_style_t style_signal_slider;

#define MOVE_BTN_SPACING_X 20
#define MOVE_BTN_SPACING_Y 5
#define MOVE_BTN_COLS 3

#define SLIDER_WIDTH 5
#define SLIDER_GAP 5
#define SLIDER_LABEL_OFFSET_Y -2

#define MESSAGE_BOX_WIDTH 250
#define MESSAGE_BOX_HEIGHT 75 // Reduced from 110 for more space
#define TX_MESSAGE_BOX_X 10
#define TX_MESSAGE_BOX_Y                                                       \
    (POSITION_BOX_Y + POSITION_BOX_HEIGHT + 5) // 5px below position box
#define RX_MESSAGE_BOX_X 10
#define RX_MESSAGE_BOX_Y                                                       \
    (TX_MESSAGE_BOX_Y + MESSAGE_BOX_HEIGHT + 5) // 5px below TX message box

// Message box deferred update system with scrolling history
#ifndef MAX_MESSAGES
#define MAX_MESSAGES 4 // Reduced from 7 for less clutter
#endif

#ifndef PREFS_NAMESPACE
#define PREFS_NAMESPACE "magloop"
#endif

// Preferences schema version - bump this when code defaults change and you
// want devices to adopt the new defaults on next firmware update.
#ifndef PREFS_VERSION
#define PREFS_VERSION 1
#endif

static int recent_message_failures = 0;
static uint32_t last_send_attempt = 0;
static int stable_failure_count = 0;

// ===== Removed: Limit Switch Status Variables =====
static lv_obj_t* tx_message_box = NULL;
static lv_obj_t* rx_message_box = NULL;

// ===== UI Element Globals (moved here for ESP-NOW access) =====
static lv_obj_t* g_position_label = NULL;
static lv_obj_t* g_signal_label = NULL;

// Forward declarations for position storage functions
static void initialize_position_array();
static bool save_positions_to_file();
static bool save_single_position(int band_index, int mode_index,
                                 int32_t position);
static bool load_positions_from_file();
// Removed: clamp_position declaration
static void update_current_position(int32_t position);
static void change_band_mode(int band_index, int mode_index);
static void update_position_display(int32_t position);
static void update_signal_display(int8_t rssi);
// static void evaluate_signal_strength();
// Removed: update_limit_indicators declaration
static void create_message_boxes();
static void add_tx_message(const char* message);
static void add_rx_message(const char* message);

// Forward declarations for all helper functions used before their definition
static void add_rx_message(const char* message);
// Removed: clamp_position declaration
static void change_band_mode(int band_index, int mode_index);
static void update_position_display(int32_t position);
static void update_signal_display(int8_t rssi);
// Removed: update_limit_indicators declaration
void create_speed_delay_controls();
void init_button_styles(void);
void create_move_buttons();
static bool send_message_to_controller(CommandType cmd, int32_t param);
void band_radio_button_event_cb(lv_event_t* e);
void mode_button_event_cb(lv_event_t* e);
void on_data_recv(const uint8_t* mac, const uint8_t* data, int len);
void on_data_sent(const uint8_t* mac, esp_now_send_status_t status);
static const char* cmd_to_str(CommandType cmd);
void send_message(CommandType cmd, int param, int messageId);

// Forward declarations for message box update functions
static void add_tx_message(const char* message);
static void add_rx_message(const char* message);
static void speed_dec_btn_event_cb(lv_event_t* e);
static void speed_inc_btn_event_cb(lv_event_t* e);
// ESP-NOW and preferences globals
// new esp3268:25:DD:30:87:C8
uint8_t controller_mac[] = {0x68, 0x25, 0xDD, 0x30, 0x87, 0xC8};
static esp_now_peer_info_t peer_info;
// Function definitions (move any misplaced ones here)
// Stepper speed pulse delay variables (single definition at file scope)
static int32_t slow_speed_pulse_delay = 1000;
static int32_t medium_speed_pulse_delay = 500;
static int32_t fast_speed_pulse_delay = 200;
static int32_t move_to_pulse_delay = 800;

// Explicit compile-time defaults used for resets
#define DEFAULT_SLOW_PULSE_DELAY 1000
#define DEFAULT_MEDIUM_PULSE_DELAY 500
#define DEFAULT_FAST_PULSE_DELAY 200
#define DEFAULT_MOVE_TO_PULSE_DELAY 800

// Data attached to each speed control to allow callbacks to update value,
// persist it, and notify the StepperController.
typedef struct {
    int32_t* value; // pointer to the pulse delay variable
    int min;
    int max;
    lv_obj_t* val_label;        // label showing current numeric value
    CommandType controller_cmd; // CMD_SLOW_SPEED_PULSE_DELAY etc
    const char* pref_key;       // Preferences key for persistence
    lv_timer_t* save_timer;     // debounce timer for saving to prefs
} SpeedControlData;

// Pointers to per-control contexts so Reset/Save UI can update them
static SpeedControlData* g_speed_controls[4] = {NULL, NULL, NULL, NULL};

#define MOVE_BTN_COLOR_RELEASED 0x0000FF
#define MOVE_BTN_COLOR_PRESSED 0xFF0000
#define MOVE_BTN_WIDTH 60
#define MOVE_BTN_HEIGHT 30
#define MOVE_BTN_PADDING 10
#define MOVE_BTN_GAP_Y 8

#define DOWN_LIMIT_X 10
#define DOWN_LIMIT_Y 8
#define UP_LIMIT_X 81
#define UP_LIMIT_Y 8
#define LIMIT_BTN_WIDTH 70
#define LIMIT_BTN_HEIGHT 28

#define RESET_BTN_X -10 // Relative to top-right
#define RESET_BTN_Y 8
#define POWER_BTN_X -10 // Relative to top-right
#define POWER_BTN_Y 40
#define POWER_BTN_GAP -8         // Gap between power and reset buttons
#define POWER_BTN_FALLBACK_X -64 // Fallback position if no reset button
#define RESET_POWER_BTN_WIDTH 48
#define RESET_POWER_BTN_HEIGHT 28

#define POSITION_BOX_X_OFFSET 10 // Fixed position from left edge
#define POSITION_BOX_WIDTH                                                     \
    ((lv_disp_get_hor_res(NULL) - 2 * POSITION_BOX_X_OFFSET - 5) / 2)
#define POSITION_BOX_HEIGHT 28 // Reasonable height for visibility
#define SIGNAL_BOX_WIDTH                                                       \
    ((lv_disp_get_hor_res(NULL) - 2 * POSITION_BOX_X_OFFSET - 5) / 2)
#define SIGNAL_BOX_HEIGHT 28 // Same height as position box for alignment
#define SIGNAL_BOX_X_OFFSET (POSITION_BOX_X_OFFSET + POSITION_BOX_WIDTH + 5)
#define POSITION_BOX_Y                                                         \
    (DOWN_LIMIT_Y + LIMIT_BTN_HEIGHT + 5) // 5px below limit buttons
#define SIGNAL_BOX_Y                                                           \
    (DOWN_LIMIT_Y + LIMIT_BTN_HEIGHT + 5) // 5px below limit buttons

#define BAND_BTN_SPACING 10
#define BAND_BTN_Y 355 // Position above move buttons
#define MODE_BTN_SPACING 10
#define MODE_BTN_Y 320 // Position above band buttons

#define MOVE_BTN_START_Y 395
#define MOVE_BTN_SPACING_X 20
#define MOVE_BTN_SPACING_Y 5
#define MOVE_BTN_COLS 3

#define SLIDER_WIDTH 5
#define SLIDER_GAP 5
#define SLIDER_LABEL_OFFSET_Y -2

#define MESSAGE_BOX_WIDTH 250
#define MESSAGE_BOX_HEIGHT 75 // Reduced from 110 for more space
#define TX_MESSAGE_BOX_X 10
#define TX_MESSAGE_BOX_Y                                                       \
    (POSITION_BOX_Y + POSITION_BOX_HEIGHT + 5) // 5px below position box
#define RX_MESSAGE_BOX_X 10
#define RX_MESSAGE_BOX_Y                                                       \
    (TX_MESSAGE_BOX_Y + MESSAGE_BOX_HEIGHT + 5) // 5px below TX message box

// Forward declarations for all helper functions used before their definition
static void add_rx_message(const char* message);
// Removed: clamp_position declaration
static void change_band_mode(int band_index, int mode_index);
static void update_position_display(int32_t position);
static void update_signal_display(int8_t rssi);
// Removed: update_limit_indicators declaration
void create_speed_delay_controls();
void init_button_styles(void);
void create_move_buttons();
void band_radio_button_event_cb(lv_event_t* e);
void mode_button_event_cb(lv_event_t* e);
void on_data_recv(const uint8_t* mac, const uint8_t* data, int len);
void on_data_sent(const uint8_t* mac, esp_now_send_status_t status);
void send_message(CommandType cmd, int param, int messageId);

// Forward declarations for message box update functions
static void add_tx_message(const char* message);
static void add_rx_message(const char* message);
static uint8_t next_message_id = 1;
// Function definitions (move any misplaced ones here)
// Helper function for speed control blocks (single definition at file scope)
// Removed unused function: add_speed_control

// Single definition at file scope
static void create_speed_delay_controls_impl() {
    // Add each speed control directly to the screen, smaller size
    int base_x = RX_MESSAGE_BOX_X;
    int base_y = RX_MESSAGE_BOX_Y + MESSAGE_BOX_HEIGHT + 10;
    // Removed unused variables: control_w, control_h, gap_x
    int gap_y = 5;

    // Arrange controls in 2 rows of 2, centered below RX message box, using
    // containers Removed unused variable: num_controls
    int container_w = 180; // Wider for all elements
    int container_h = 31;

    // Removed unused variable: total_height
    int start_x = base_x + (MESSAGE_BOX_WIDTH - container_w) / 2;
    // Determine start_y dynamically from the actual RX message box position
    int start_y = base_y; // fallback
    if (rx_message_box) {
        // Query LVGL for the RX box geometry; sometimes layout isn't applied
        // yet
        int rx_y = lv_obj_get_y(rx_message_box);
        int rx_h = lv_obj_get_height(rx_message_box);
        Serial.printf("DEBUG: rx_message_box ptr=%p rx_y=%d rx_h=%d\n",
                      rx_message_box, rx_y, rx_h);
        if (rx_h > 8) {
            start_y = rx_y + rx_h + 5; // 5px below RX message box bottom
        } else {
            // Fallback to the constant-based calculation if geometry seems
            // invalid
            start_y = RX_MESSAGE_BOX_Y + 52 + 5;
            Serial.printf("DEBUG: using fallback start_y=%d\n", start_y);
        }
    } else {
        start_y = RX_MESSAGE_BOX_Y + 52 + 5;
        Serial.println("DEBUG: rx_message_box is NULL, using fallback start_y");
    }

    lv_obj_t* prev_cont = NULL;
    auto add_speed_control_grouped =
        [&](const char* label, int32_t* value, int min, int max, int idx,
            const char* pref_key, CommandType controller_cmd) {
            lv_obj_t* parent = lv_scr_act();
            lv_obj_t* cont = lv_obj_create(parent);
            lv_obj_set_size(cont, container_w, container_h);
            // Position relative to rx_message_box when possible
            if (rx_message_box && lv_obj_is_valid(rx_message_box)) {
                if (!prev_cont) {
                    // Align first control to bottom-left of RX box, offset to
                    // start_x
                    int x_ofs = start_x - RX_MESSAGE_BOX_X;
                    lv_obj_align_to(cont, rx_message_box,
                                    LV_ALIGN_OUT_BOTTOM_LEFT, x_ofs, 5);
                } else {
                    // Stack below previous control
                    lv_obj_align_to(cont, prev_cont, LV_ALIGN_OUT_BOTTOM_LEFT,
                                    0, gap_y);
                }
            } else {
                int x = start_x;
                int y = start_y + idx * (container_h + gap_y);
                lv_obj_set_pos(cont, x, y);
            }
            lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(cont, lv_color_hex(0x1E3A8A), 0); // blue
            lv_obj_set_style_border_width(cont, 2, 0);
            lv_obj_set_style_border_color(cont, lv_color_hex(0xFFFFFF),
                                          0); // white border
            lv_obj_set_style_pad_all(cont, 2, 0);
            // Create label, minus, value, plus horizontally in the container
            lv_obj_t* lbl = lv_label_create(cont);
            lv_label_set_text(lbl, label);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 2, 0);

            lv_obj_t* dec_btn = lv_btn_create(cont);
            lv_obj_set_size(dec_btn, 24, 24);
            lv_obj_add_style(dec_btn, &style_move_default, 0);
            lv_obj_t* dec_lbl = lv_label_create(dec_btn);
            lv_label_set_text(dec_lbl, "-");
            lv_obj_center(dec_lbl);
            // Allocate per-control context
            SpeedControlData* scd =
                (SpeedControlData*)malloc(sizeof(SpeedControlData));
            scd->value = value;
            scd->min = min;
            scd->max = max;
            scd->val_label = NULL; // set after creating val_lbl
            scd->pref_key = pref_key;
            scd->controller_cmd = controller_cmd;
            scd->save_timer = NULL;
            lv_obj_add_event_cb(dec_btn, speed_dec_btn_event_cb,
                                LV_EVENT_CLICKED, scd);
            lv_obj_align(dec_btn, LV_ALIGN_LEFT_MID, 70, 0);

            lv_obj_t* val_lbl = lv_label_create(cont);
            char val_buf[16];
            snprintf(val_buf, sizeof(val_buf), "%d", *value);
            lv_label_set_text(val_lbl, val_buf);
            lv_obj_set_style_text_color(val_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
            lv_obj_align(val_lbl, LV_ALIGN_LEFT_MID, 105, 0);

            lv_obj_t* inc_btn = lv_btn_create(cont);
            lv_obj_set_size(inc_btn, 24, 24);
            lv_obj_add_style(inc_btn, &style_move_default, 0);
            lv_obj_t* inc_lbl = lv_label_create(inc_btn);
            lv_label_set_text(inc_lbl, "+");
            lv_obj_center(inc_lbl);
            lv_obj_add_event_cb(inc_btn, speed_inc_btn_event_cb,
                                LV_EVENT_CLICKED, scd);
            // Wire up val_label in the context now that it's created
            scd->val_label = val_lbl;
            lv_obj_align(inc_btn, LV_ALIGN_LEFT_MID, 135, 0);
            // Remember this container so the next one can be aligned below it
            prev_cont = cont;
            // Save global pointer for reset/save operations
            if (idx >= 0 && idx < 4)
                g_speed_controls[idx] = scd;
            Serial.printf("DEBUG: created speed control '%s' at cont=%p\n",
                          label, cont);
        };

    add_speed_control_grouped("Slow", &slow_speed_pulse_delay, 100, 5000, 0,
                              "spd_slow", CMD_SLOW_SPEED_PULSE_DELAY);
    add_speed_control_grouped("Medium", &medium_speed_pulse_delay, 50, 2000, 1,
                              "spd_med", CMD_MEDIUM_SPEED_PULSE_DELAY);
    add_speed_control_grouped("Fast", &fast_speed_pulse_delay, 10, 1000, 2,
                              "spd_fast", CMD_FAST_SPEED_PULSE_DELAY);
    add_speed_control_grouped("MoveTo", &move_to_pulse_delay, 50, 3000, 3,
                              "spd_move", CMD_MOVE_TO_PULSE_DELAY);

    // No Save/Reset buttons here — persistence happens immediately on +/-
    // and reset is handled via the power button long-press if needed.
}

static void hard_reset_pulse_delays(bool persist) {
    Serial.println("Hard reset: restoring pulse-delay defaults");
    // Cancel any pending save timers and reset in-memory values and UI labels
    const int32_t defaults[4] = {
        DEFAULT_SLOW_PULSE_DELAY, DEFAULT_MEDIUM_PULSE_DELAY,
        DEFAULT_FAST_PULSE_DELAY, DEFAULT_MOVE_TO_PULSE_DELAY};
    for (int i = 0; i < 4; ++i) {
        if (g_speed_controls[i]) {
            SpeedControlData* sd = g_speed_controls[i];
            // Cancel any pending timer
            if (sd->save_timer) {
                lv_timer_del(sd->save_timer);
                sd->save_timer = NULL;
            }
            // Update in-memory value
            *(sd->value) = defaults[i];
            // Update UI label
            if (sd->val_label) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", (int)*(sd->value));
                lv_label_set_text(sd->val_label, buf);
                lv_obj_invalidate(sd->val_label);
            }
            // Notify controller of new value
            send_message_to_controller(sd->controller_cmd, *(sd->value));
        }
    }
    if (persist) {
        preferences.begin(PREFS_NAMESPACE, false);
        preferences.putInt("spd_slow", (int)defaults[0]);
        preferences.putInt("spd_med", (int)defaults[1]);
        preferences.putInt("spd_fast", (int)defaults[2]);
        preferences.putInt("spd_move", (int)defaults[3]);
        int v0 = preferences.getInt("spd_slow", -1);
        int v1 = preferences.getInt("spd_med", -1);
        int v2 = preferences.getInt("spd_fast", -1);
        int v3 = preferences.getInt("spd_move", -1);
        preferences.end();
        Serial.printf(
            "Defaults persisted to preferences (verified: %d %d %d %d)\n", v0,
            v1, v2, v3);
    } else {
        Serial.println("Defaults applied in-memory only (not persisted)");
    }
}

// Timer callback to retry creation when LVGL has finished layout
static void speed_controls_timer_cb(lv_timer_t* t) {
    if (rx_message_box && lv_obj_is_valid(rx_message_box) &&
        lv_obj_get_height(rx_message_box) > 8) {
        create_speed_delay_controls_impl();
        lv_timer_del(t);
    } else {
        // keep trying; let the timer run again
        Serial.println("DEBUG: speed_controls_timer_cb - RX box still not "
                       "ready, will retry");
    }
}

void create_speed_delay_controls() {
    // Try immediate creation if RX message box has valid geometry
    if (rx_message_box && lv_obj_is_valid(rx_message_box) &&
        lv_obj_get_height(rx_message_box) > 8) {
        create_speed_delay_controls_impl();
    } else {
        Serial.println("DEBUG: create_speed_delay_controls - scheduling timer "
                       "to create controls");
        // Schedule a timer to retry after 50 ms
        lv_timer_create(speed_controls_timer_cb, 50, NULL);
    }
}

static void add_tx_message(const char* message) {
    // Store message in circular buffer for scrolling history (interrupt-safe)
    if (message && strlen(message) < sizeof(tx_messages[0])) {
        Serial.printf("add_tx_message called: %s\n", message);
        // Shift messages up (oldest falls off)
        for (int i = MAX_MESSAGES - 1; i > 0; i--) {
            strcpy(tx_messages[i], tx_messages[i - 1]);
        }
        // Add new message at top
        strcpy(tx_messages[0], message);
        if (tx_message_count < MAX_MESSAGES)
            tx_message_count++;

        // LVGL is not interrupt-safe - must use deferred updates
        tx_message_update_pending = true;
        Serial.printf("tx_message_update_pending set TRUE\n");
        // Trigger FSM UI callback by pushing a dummy event
        fsm::Event ev{};
        ev.type = fsm::EventType::TIMEOUT; // Use a harmless event
        g_fsm.push_event(ev);
    }
}

static void add_rx_message(const char* message) {
    // Store message in circular buffer for scrolling history (interrupt-safe)
    if (message && strlen(message) < sizeof(rx_messages[0])) {
        Serial.printf("add_rx_message called: %s\n", message);
        // Shift messages up (oldest falls off)
        for (int i = MAX_MESSAGES - 1; i > 0; i--) {
            strcpy(rx_messages[i], rx_messages[i - 1]);
        }
        // Add new message at top
        strcpy(rx_messages[0], message);
        if (rx_message_count < MAX_MESSAGES)
            rx_message_count++;

        // LVGL is not interrupt-safe - must use deferred updates
        rx_message_update_pending = true;
        Serial.printf("rx_message_update_pending set TRUE\n");
        // Trigger FSM UI callback by pushing a dummy event
        fsm::Event ev{};
        ev.type = fsm::EventType::TIMEOUT; // Use a harmless event
        g_fsm.push_event(ev);
    }
}

// ===== ESP-NOW Functions =====

// Sends a command to the controller via ESP-NOW and updates the TX message box
static bool send_message_to_controller(CommandType cmd,
                                       int32_t param = STEPPER_PARAM_UNUSED) {
    Serial.printf("[DEBUG] send_message_to_controller called: cmd=%s param=%ld\n", cmd_to_str(cmd), (long)param);
    Message msg;
    msg.messageId = next_message_id++;
    msg.command = cmd;
    msg.param = param;
    Serial.println("--- [SENT CMD] StepperController ---");
    Serial.printf("  [SENT CMD] Command: %s (%u)\n", cmd_to_str(cmd),
                  (unsigned)cmd);
    Serial.printf("  [SENT CMD] Param:   %ld\n", (long)param);
    Serial.printf("  [SENT CMD] Msg ID:  %u\n", (unsigned)msg.messageId);
    Serial.println("------------------------------------");
    esp_err_t res = esp_now_send(controller_mac, (uint8_t*)&msg, sizeof(msg));
    if (res != ESP_OK) {
        Serial.printf("TX FAILED: %s (cmd=%s)\n", esp_err_to_name(res),
                      cmd_to_str(cmd));
        return false;
    }
    Serial.printf("TX: %s, param=%d, id=%u\n", cmd_to_str(cmd), (int)param,
                  msg.messageId);

    // Add to GUI message box
    char tx_msg[64];
    snprintf(tx_msg, sizeof(tx_msg), "%s p=%d id=%u", cmd_to_str(cmd),
             (int)param, msg.messageId);
    add_tx_message(tx_msg);
    return true;
}

// Helper function to send correct command for each button (slow, medium, fast,
// up/down)
static void send_move_command(fsm::EventType type, int speed) {
    CommandType cmd = CMD_STOP;
    switch (type) {
    case fsm::EventType::BTN_MOVE_UP:
        switch (speed) {
        case 0:
            cmd = CMD_UP_SLOW;
            break;
        case 1:
            cmd = CMD_UP_MEDIUM;
            break;
        case 2:
            cmd = CMD_UP_FAST;
            break;
        default:
            cmd = CMD_UP_FAST;
            break;
        }
        break;
    case fsm::EventType::BTN_MOVE_DOWN:
        switch (speed) {
        case 0:
            cmd = CMD_DOWN_SLOW;
            break;
        case 1:
            cmd = CMD_DOWN_MEDIUM;
            break;
        case 2:
            cmd = CMD_DOWN_FAST;
            break;
        default:
            cmd = CMD_DOWN_FAST;
            break;
        }
        break;
    case fsm::EventType::BTN_STOP:
        cmd = CMD_STOP;
        break;
    default:
        cmd = CMD_STOP;
        break;
    }
    send_message_to_controller(cmd, STEPPER_PARAM_UNUSED);
}

void on_data_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (mac) {
        Serial.printf("GUI onDataRecv from %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    Serial.printf("on_data_recv: len=%d\n", len);
    if (!data || len < (int)sizeof(Message)) {
        Serial.printf("GUI onDataRecv: bad len %d\n", len);
        return;
    }

    Message msg;
    memcpy(&msg, data, sizeof(msg));

    Serial.println("--- [RECV CMD] StepperController ---");
    Serial.printf("  [RECV CMD] Command: %s (%u)\n",
                  cmd_to_str((CommandType)msg.command), (unsigned)msg.command);
    Serial.printf("  [RECV CMD] Param:   %ld\n", (long)msg.param);
    Serial.printf("  [RECV CMD] Msg ID:  %u\n", (unsigned)msg.messageId);
    Serial.println("----------------------------------------");

    // Add to GUI message box
    char rx_msg[64];
    snprintf(rx_msg, sizeof(rx_msg), "%s p=%ld id=%u",
             cmd_to_str((CommandType)msg.command), (long)msg.param,
             (unsigned)msg.messageId);
    add_rx_message(rx_msg);

    // Debug: Track position commands specifically
    if (msg.command == CMD_POSITION) {
        Serial.printf("*** POSITION MESSAGE: RX box shows p=%ld, setting "
                      "pending_position=%ld ***\n",
                      (long)msg.param, (long)msg.param);
    }

    // Any successful ESP-NOW message indicates good link quality
    uint32_t now = millis();
    last_esp_now_msg_time = now; // Update last communication time

    if (now - last_esp_now_msg_time < 2000) { // Messages within 2 seconds
        consecutive_msgs++;
    } else {
        consecutive_msgs = 1; // Reset if gap too long
    }

    // Any message received means excellent signal quality during active
    // communication Only update RSSI if signal is not forced offline due to
    // disconnection
    if (!signal_forced_offline) {
        last_rssi = -35 + random(-3, 3); // Excellent signal: -32 to -38 dBm
        signal_update_pending = true;
        Serial.printf("Active communication RSSI: %d dBm (msg received)\n",
                      last_rssi);
    } else {
        Serial.println("RSSI update blocked - signal forced offline");
    }

    switch (msg.command) {
    case CMD_POSITION:
        // Don't update UI directly in interrupt context - defer to main loop
        pending_position = msg.param;
        position_update_pending = true;
        Serial.printf("Position received: %d -> passthrough to %d (deferred, "
                      "no limiting) - MOVEMENT DETECTED at %u\n",
                      (int)msg.param, (int)pending_position, (unsigned int)now);
        break;
    case CMD_RESET:
        // StepperController is requesting GUI to reset
        Serial.println("CMD_RESET received from StepperController - rebooting "
                       "GUI in 2 seconds...");
        delay(2000);   // Brief delay to allow serial message to be sent
        ESP.restart(); // Reboot the ESP32
        break;
    case CMD_HOME:
        // Home command acknowledged
        Serial.println("CMD_HOME received - homing initiated");
        break;
    case CMD_MOVE_TO_HOME:
        // Move to home command acknowledged
        Serial.println("CMD_MOVE_TO_HOME received - moving to home position");
        break;

    case CMD_HOME_COMPLETE:
        // Homing operation completed
        Serial.println("CMD_HOME_COMPLETE received - homing operation complete");
        {
            fsm::Event ev{};
            ev.type = fsm::EventType::HOME_COMPLETE;
            Serial.printf("[DEBUG] main.cpp: pushing FSM HOME_COMPLETE event, type=%d\n", (int)ev.type);
            g_fsm.push_event(ev);
            Serial.println("[DEBUG] main.cpp: FSM HOME_COMPLETE event pushed");
        }
        break;
    case CMD_HOME_FAILED:
        // Homing operation failed
        Serial.println("CMD_HOME_FAILED received - homing operation failed");
        break;
    case CMD_SENSOR_STATUS:
        // Sensor status update
        Serial.println("CMD_SENSOR_STATUS received - updating sensor status");
        break;
    case CMD_SET_HOME_POSITION:
        // Set home position command acknowledged
        Serial.println(
            "CMD_SET_HOME_POSITION received - setting home position");
        break;
    case CMD_HEARTBEAT:
        // Heartbeat response received - this indicates good link quality
        last_heartbeat_received = now;
        heartbeat_success_count++;
        Serial.printf("Heartbeat response received (success rate: %d/%d)\n",
                      heartbeat_success_count, heartbeat_total_count);
        break;
    // Removed: limit switch status handling
    case CMD_ACK:
        // handle ack
        break;
    default:
        break;
    }
}

void on_data_sent(const uint8_t* mac, esp_now_send_status_t status) {
    // Track send failures more conservatively to reduce bouncing
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (recent_message_failures > 0) {
            recent_message_failures =
                max(0, recent_message_failures - 1); // Slow improvement
        }
        stable_failure_count = max(0, stable_failure_count - 1);
        Serial.printf("GUI onDataSent: SUCCESS (failures: %d, stable: %d)\n",
                      recent_message_failures, stable_failure_count);
    } else {
        recent_message_failures =
            min(8, recent_message_failures + 1); // Slower degradation
        stable_failure_count = min(10, stable_failure_count + 1);
        Serial.printf("GUI onDataSent: FAILED (failures: %d, stable: %d)\n",
                      recent_message_failures, stable_failure_count);
    }

    last_send_attempt = millis();

    // Only show MAC address occasionally to reduce spam
    static uint32_t last_mac_display = 0;
    if (millis() - last_mac_display > 30000) {
        last_mac_display = millis();
        Serial.print("My MAC address is ");
        Serial.println(WiFi.macAddress());
    }
}

void send_message(CommandType cmd, int param = STEPPER_PARAM_UNUSED,
                  int messageId = 0) {
    // Route all outgoing messages through send_message_to_controller for proper
    // labeling
    send_message_to_controller(cmd, param);
}

// ===== Move Button Struct & Data =====
typedef struct {
    const char* arrow;
    const char* text;
    CommandType command;
} MoveButton;

MoveButton move_buttons[] = {
    {"↑", "Slow", CMD_UP_SLOW},    {"↑", "Med", CMD_UP_MEDIUM},
    {"↑", "Fast", CMD_UP_FAST},    {"↓", "Slow", CMD_DOWN_SLOW},
    {"↓", "Med", CMD_DOWN_MEDIUM}, {"↓", "Fast", CMD_DOWN_FAST}};

static int move_btn_indices[sizeof(move_buttons) / sizeof(move_buttons[0])];

// ===== Radio Button Struct & Data =====

static lv_obj_t* last_mode_btn = NULL;
static lv_obj_t* last_band_btn = NULL;

// Radio button dimensions
#define modeBtn_width 50
#define modeBtn_height 30
#define bandBtn_width 30
#define bandBtn_height 30

// ===== Display & Touch Objects =====
Arduino_DataBus* bus = new Arduino_ESP32QSPI(DISP_CS, DISP_SCK, DISP_D0,
                                             DISP_D1, DISP_D2, DISP_D3);
Arduino_NV3041A* gfx =
    new Arduino_NV3041A(bus, GFX_NOT_DEFINED, PORTRAIT_ROTATION, true);

Touch_GT911 touch_controller(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RES,
                             TOUCH_WIDTH, TOUCH_HEIGHT);

// ===== LVGL Globals =====
uint32_t screen_width, screen_height, buf_size_px;
lv_display_t* disp;
lv_color_t* disp_draw_buf;
lv_obj_t* g_autosave_btn = NULL;

// ===== LVGL Callbacks =====

void my_disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px_map, w, h);
    lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t* indev, lv_indev_data_t* data) {
    touch_controller.read();

    if (touch_controller.isTouched && touch_controller.touches > 0) {
        int16_t tx, ty;
        if (PORTRAIT_ROTATION == 1) {
            tx = touch_controller.points[0].y;
            ty = TOUCH_WIDTH - touch_controller.points[0].x;
        } else {
            tx = TOUCH_HEIGHT - touch_controller.points[0].y;
            ty = touch_controller.points[0].x;
        }
        data->point.x = tx;
        data->point.y = ty;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ===== Command to String Helper =====
static const char* cmd_to_str(CommandType cmd) {
    switch (cmd) {
    case CMD_STOP:
        return "STOP";
    case CMD_UP_SLOW:
        return "UP_SLOW";
    case CMD_UP_MEDIUM:
        return "UP_MEDIUM";
    case CMD_UP_FAST:
        return "UP_FAST";
    case CMD_DOWN_SLOW:
        return "DOWN_SLOW";
    case CMD_DOWN_MEDIUM:
        return "DOWN_MEDIUM";
    case CMD_DOWN_FAST:
        return "DOWN_FAST";
    case CMD_MOVE_TO:
        return "MOVE_TO";

    // Removed: CMD_MOVE_TO_DOWN_LIMIT case
    // Removed: CMD_DOWN_LIMIT_STATUS case
    // Removed: CMD_REQUEST_DOWN_STOP case
    case CMD_GET_POSITION:
        return "GET_POSITION";
    case CMD_RESET:
        return "RESET";
    case CMD_POSITION:
        return "POSITION";

    case CMD_HOME:
        return "HOME";
    case CMD_MOVE_TO_HOME:
        return "MOVE_TO_HOME";
    case CMD_HOME_COMPLETE:
        return "HOME_COMPLETE";
    case CMD_HOME_FAILED:
        return "HOME_FAILED";
    case CMD_SENSOR_STATUS:
        return "SENSOR_STATUS";

    // Removed: limit switch command string cases
    case CMD_HEARTBEAT:
        return "HEARTBEAT";
    case CMD_ACK:
        return "ACK";
    case CMD_SET_HOME_POSITION:
        return "SET_HOME_POSITION";
    default:
        String unknown_cmd = "UNKNOWN_CMD   " + String((unsigned)cmd);
        return unknown_cmd.c_str();
    }
}

// ===== Move Button Event Callback =====
static void move_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    void* ud = lv_event_get_user_data(e);
    MoveButton* mb = NULL;
    static MoveButton* test_mb_ptr = NULL;

    if (ud) {
        int idx = *((int*)ud);
        if (idx >= 0 &&
            idx < (int)(sizeof(move_buttons) / sizeof(move_buttons[0]))) {
            mb = &move_buttons[idx];
        } else if (idx < 0) {
            static MoveButton test_mb_local = {">", "Test", CMD_UP_SLOW};
            test_mb_ptr = &test_mb_local;
            mb = test_mb_ptr;
        }
    }

    if (!btn || !mb) {
        Serial.println("move_btn_event_cb: btn or mb is NULL");
        return;
    }

    Serial.printf("move_btn_event_cb: code=%d, command=%s\n", (int)code,
                  cmd_to_str(mb->command));

    if (code == LV_EVENT_PRESSED) {
        Serial.printf("Move button pressed: %s\n", cmd_to_str(mb->command));
        int speed = 2; // Default to fast
        switch (mb->command) {
        case CMD_UP_SLOW:
            speed = 0;
            break;
        case CMD_UP_MEDIUM:
            speed = 1;
            break;
        case CMD_UP_FAST:
            speed = 2;
            break;
        case CMD_DOWN_SLOW:
            speed = 0;
            break;
        case CMD_DOWN_MEDIUM:
            speed = 1;
            break;
        case CMD_DOWN_FAST:
            speed = 2;
            break;
        default:
            speed = 2;
            break;
        }
        if (mb->command == CMD_UP_SLOW || mb->command == CMD_UP_MEDIUM ||
            mb->command == CMD_UP_FAST) {
            send_move_command(fsm::EventType::BTN_MOVE_UP, speed);
        } else if (mb->command == CMD_DOWN_SLOW ||
                   mb->command == CMD_DOWN_MEDIUM ||
                   mb->command == CMD_DOWN_FAST) {
            send_move_command(fsm::EventType::BTN_MOVE_DOWN, speed);
        } else if (mb->command == CMD_MOVE_TO) {
            // Always push FSM event for MOVE_TO so [SENT CMD] is guaranteed
            fsm::Event ev{};
            ev.type = fsm::EventType::BTN_MOVE_TO;
            ev.int_arg = current_stepper_position;
            g_fsm.push_event(ev);
        } else {
            send_move_command(fsm::EventType::BTN_STOP, 2);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        Serial.println("Move button released: STOP");
        send_move_command(fsm::EventType::BTN_STOP, 2);
    }
}

// ===== Radio Button Event Callbacks =====
void band_radio_button_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const char* label = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        if (last_band_btn && last_band_btn != btn) {
            lv_obj_clear_state(last_band_btn, LV_STATE_CHECKED);
        }
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        last_band_btn = btn;

        // Find band index
        int band_index = -1;
        for (int i = 0; i < sizeof(band_buttons) / sizeof(band_buttons[0]);
             i++) {
            if (strcmp(band_buttons[i].label, label) == 0) {
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

void mode_button_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const char* label = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        if (last_mode_btn && last_mode_btn != btn) {
            lv_obj_clear_state(last_mode_btn, LV_STATE_CHECKED);
        }
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        last_mode_btn = btn;

        // Find mode index
        int mode_index = -1;
        for (int i = 0; i < sizeof(mode_buttons) / sizeof(mode_buttons[0]);
             i++) {
            if (strcmp(mode_buttons[i].label, label) == 0) {
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
    positionArray[0][3] =
        random(0, 1600); // current_position (random for testing)

    // Initialize all band/mode positions with random values for testing
    Serial.println("Initializing positions with random test values:");
    for (int i = 1; i < POSITION_ROWS; i++) {
        for (int j = 0; j < POSITION_COLS; j++) {
            positionArray[i][j] = random(0, 1600);
            Serial.printf("  Band %d, Mode %d: %d\n", i - 1, j,
                          (int)positionArray[i][j]);
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

static bool save_single_position(int band_index, int mode_index,
                                 int32_t position) {
    if (band_index < 0 || band_index >= 6 || mode_index < 0 ||
        mode_index >= 4) {
        Serial.printf("Invalid band/mode index: %d/%d\n", band_index,
                      mode_index);
        return false;
    }

    preferences.begin(PREFS_NAMESPACE, false);

    // Save the specific band/mode position
    char key[16];
    snprintf(key, sizeof(key), "pos_%d_%d", band_index + 1, mode_index);
    preferences.putInt(key, position);

    // Also update the current position and state
    preferences.putInt("pos_0_0", band_index);          // last_band
    preferences.putInt("pos_0_1", mode_index);          // last_mode
    preferences.putInt("pos_0_2", autosave_on ? 1 : 0); // autosave
    preferences.putInt("pos_0_3", position);            // current_position

    preferences.end();

    Serial.printf("Position saved for %s/%s: %d\n",
                  band_buttons[band_index].label,
                  mode_buttons[mode_index].label, (int)position);
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

    // Load stepper speed pulse delay variables from preferences
    slow_speed_pulse_delay = preferences.getInt("spd_slow", 1000);
    medium_speed_pulse_delay = preferences.getInt("spd_med", 500);
    fast_speed_pulse_delay = preferences.getInt("spd_fast", 200);
    move_to_pulse_delay = preferences.getInt("spd_move", 800);

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
    positionArray[0][3] = current_stepper_position;

    Serial.println("Positions loaded from preferences:");
    Serial.printf(
        "  Header: band_index=%d, mode_index=%d, autosave=%d, position=%d\n",
        current_band_index, current_mode_index, (int)positionArray[0][2],
        (int)current_stepper_position);
    Serial.printf("  Pulse Delays: slow=%d, medium=%d, fast=%d, move_to=%d\n",
                  (int)slow_speed_pulse_delay, (int)medium_speed_pulse_delay,
                  (int)fast_speed_pulse_delay, (int)move_to_pulse_delay);

    // Validate loaded indices
    if (current_band_index < 0 || current_band_index >= 6) {
        Serial.printf("  WARNING: Invalid band index %d, resetting to 0\n",
                      current_band_index);
        current_band_index = 0;
    }
    if (current_mode_index < 0 || current_mode_index >= 4) {
        Serial.printf("  WARNING: Invalid mode index %d, resetting to 0\n",
                      current_mode_index);
        current_mode_index = 0;
    }

    Serial.printf(
        "  Final: Band=%s(%d), Mode=%s(%d), Position=%d, AutoSave=%s\n",
        band_buttons[current_band_index].label, current_band_index,
        mode_buttons[current_mode_index].label, current_mode_index,
        (int)current_stepper_position, autosave_on ? "On" : "Off");
    return true;
}

// Check stored preferences version and migrate defaults if needed.
static void check_and_migrate_prefs() {
    // Read stored prefs version first
    preferences.begin(PREFS_NAMESPACE, true);
    int stored_version = preferences.getInt("prefs_version", 0);
    preferences.end();

    // Always ensure the pulse-delay keys exist (non-destructive).
    // This prevents noisy NOT_FOUND logs on first boot after flashing and
    // ensures the app has sensible defaults available immediately.
    preferences.begin(PREFS_NAMESPACE, false);
    if (!preferences.isKey("spd_slow")) {
        preferences.putInt("spd_slow", (int)slow_speed_pulse_delay);
        Serial.println("Created missing key: spd_slow");
    }
    if (!preferences.isKey("spd_med")) {
        preferences.putInt("spd_med", (int)medium_speed_pulse_delay);
        Serial.println("Created missing key: spd_med");
    }
    if (!preferences.isKey("spd_fast")) {
        preferences.putInt("spd_fast", (int)fast_speed_pulse_delay);
        Serial.println("Created missing key: spd_fast");
    }
    if (!preferences.isKey("spd_move")) {
        preferences.putInt("spd_move", (int)move_to_pulse_delay);
        Serial.println("Created missing key: spd_move");
    }

    // If prefs version changed in code, update stored version (non-destructive)
    if (stored_version != PREFS_VERSION) {
        Serial.printf("Prefs version mismatch (stored=%d code=%d) - updating "
                      "prefs_version only\n",
                      stored_version, PREFS_VERSION);
        preferences.putInt("prefs_version", PREFS_VERSION);
    }
    preferences.end();
    Serial.println("Prefs check complete (missing keys created if any)");
}

// Removed: clamp_position and position limiting logic

static void update_current_position(int32_t position) {
    // Removed: clamp_position usage
    current_stepper_position = position;
    positionArray[0][3] = position;

    // Update position display on screen (only if UI is ready)
    update_position_display(position);

    // Also update the current band/mode position
    if (current_band_index >= 0 && current_band_index < 6 &&
        current_mode_index >= 0 && current_mode_index < 4) {
        positionArray[current_band_index + 1][current_mode_index] = position;

        // Always save current state for startup restoration (not just when
        // autosave is on)
        save_single_position(current_band_index, current_mode_index, position);

        // If autosave is on, also save the full position array
        if (autosave_on) {
            save_positions_to_file();
        }
    }
}

static void change_band_mode(int band_index, int mode_index) {
    // Save current position to the old band/mode before switching
    if (current_band_index >= 0 && current_band_index < 6 &&
        current_mode_index >= 0 && current_mode_index < 4) {
        positionArray[current_band_index + 1][current_mode_index] =
            current_stepper_position;
        Serial.printf("Saved current position %d to old %s/%s\n",
                      (int)current_stepper_position,
                      band_buttons[current_band_index].label,
                      mode_buttons[current_mode_index].label);

        // Always save position for startup restoration (not just when autosave
        // is on)
        save_single_position(current_band_index, current_mode_index,
                             current_stepper_position);
    }

    // Update indices to new band/mode
    current_band_index = band_index;
    current_mode_index = mode_index;

    // Update position array header
    positionArray[0][0] = band_index;
    positionArray[0][1] = mode_index;
    positionArray[0][2] = autosave_on ? 1 : 0;

    // Get stored position for the new band/mode combination
    if (band_index >= 0 && band_index < 6 && mode_index >= 0 &&
        mode_index < 4) {
        int32_t stored_position = positionArray[band_index + 1][mode_index];
        Serial.printf(
            "Band/Mode changed to %s/%s, moving to stored position: %d\n",
            band_buttons[band_index].label, mode_buttons[mode_index].label,
            (int)stored_position);

        // DEBUG: About to push FSM BTN_MOVE_TO event
        Serial.printf("[DEBUG] change_band_mode: pushing FSM BTN_MOVE_TO event "
                      "with position %d\n",
                      (int)stored_position);
        // Route MOVE_TO through FSM event for [SENT CMD] output
        fsm::Event ev{};
        ev.type = fsm::EventType::BTN_MOVE_TO;
        ev.int_arg = stored_position;
        g_fsm.push_event(ev);
        // DEBUG: send_message_to_controller called
        // Serial.printf("[DEBUG] send_message_to_controller: cmd=%s
        // param=%ld\n", cmd_to_str(cmd), (long)param);

        // Update current position immediately (will be confirmed when stepper responds)
        current_stepper_position = stored_position;
        positionArray[0][3] = stored_position;
        update_position_display(stored_position);

        // Always save the state change for startup restoration (regardless of
        // autosave setting)
        preferences.begin(PREFS_NAMESPACE, false);
        preferences.putInt("pos_0_0", band_index);          // last_band
        preferences.putInt("pos_0_1", mode_index);          // last_mode
        preferences.putInt("pos_0_2", autosave_on ? 1 : 0); // autosave
        preferences.putInt("pos_0_3", stored_position);     // current_position
        preferences.end();
        Serial.println(
            "Band/Mode state saved to preferences for startup restoration");

        // If autosave is on, also save the full position array
        if (autosave_on) {
            save_positions_to_file();
        }
    }
}

// ===== Position Display Functions =====
static void update_position_display(int32_t position) {
    // Safety check - only update if UI is initialized
    if (!g_position_label) {
        Serial.printf("Position display not initialized yet, position: %d\n",
                      (int)position);
        return;
    }

    char pos_text[32];
    snprintf(pos_text, sizeof(pos_text), "Position: %d", (int)position);
    lv_label_set_text(g_position_label, pos_text);
    lv_obj_set_style_text_font(g_position_label, &lv_font_montserrat_12, 0);
    lv_obj_center(g_position_label);

    // Debug: Verify text was set
    Serial.printf("Updated position text to: %s\n",
                  lv_label_get_text(g_position_label));

    // Force label to refresh
    lv_obj_invalidate(g_position_label);
}

static void update_signal_display(int8_t rssi) {
    // Safety check - only update if UI is initialized
    if (!g_signal_label) {
        Serial.printf("Signal display not initialized yet, RSSI: %d\n", rssi);
        return;
    }

    if (signal_forced_offline) {
        lv_label_set_text(g_signal_label, LV_SYMBOL_WIFI
                          "  Disconnected"); // Show message with icon
        lv_obj_set_style_bg_color(g_signal_label, lv_color_hex(0xFF0000),
                                  0); // Red background
        lv_obj_set_style_text_color(g_signal_label, lv_color_hex(0xFFFFFF),
                                    0); // White text
    } else if (rssi <= -100) {
        lv_label_set_text(g_signal_label, "Sig: -- dBm");
        lv_obj_set_style_bg_color(g_signal_label, lv_color_hex(0xFF8000),
                                  0); // Orange background
    } else {
        char signal_text[32];
        snprintf(signal_text, sizeof(signal_text), "Sig: %d dBm", rssi);
        lv_label_set_text(g_signal_label, signal_text);
        lv_obj_set_style_bg_color(g_signal_label, lv_color_hex(0xFF8000),
                                  0); // Orange background
    }
    lv_obj_center(g_signal_label);

    // Debug: Verify text was set
    Serial.printf("Updated signal text to: %s\n",
                  lv_label_get_text(g_signal_label));

    // Force label to refresh
    lv_obj_invalidate(g_signal_label);

    Serial.printf("Signal updated (RSSI: %d dBm)\n", rssi);
}

// Removed: update_limit_indicators function definition
// ===== Speed Pulse Delay Controls UI Event Callbacks =====

static void speed_dec_btn_event_cb(lv_event_t* e) {
    SpeedControlData* d = (SpeedControlData*)lv_event_get_user_data(e);
    if (!d || !d->value)
        return;
    if (*(d->value) > d->min) {
        *(d->value) -= 10;
        // update label
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)*(d->value));
        if (d->val_label)
            lv_label_set_text(d->val_label, buf);
        // persist immediately (no debounce) so next boot loads this value
        if (d->save_timer) {
            // cancel any existing debounce timer if present
            lv_timer_del(d->save_timer);
            d->save_timer = NULL;
        }
        if (d->pref_key) {
            preferences.begin(PREFS_NAMESPACE, false);
            preferences.putInt(d->pref_key, (int)*(d->value));
            int verify = preferences.getInt(d->pref_key, -999999);
            preferences.end();
            Serial.printf("Saved immediately: %s -> %d (verified=%d)\n",
                          d->pref_key, (int)*(d->value), verify);
        }
        // send correct command to stepper controller
        send_message_to_controller(d->controller_cmd, *(d->value));
        Serial.printf("Speed dec (Controller): %s -> %d\n",
                  d->pref_key ? d->pref_key : "?", (int)*(d->value));
    }
}

static void speed_inc_btn_event_cb(lv_event_t* e) {
    SpeedControlData* d = (SpeedControlData*)lv_event_get_user_data(e);
    if (!d || !d->value)
        return;
    if (*(d->value) < d->max) {
        *(d->value) += 10;
        // update label
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)*(d->value));
        if (d->val_label)
            lv_label_set_text(d->val_label, buf);
        // persist immediately (no debounce) so next boot loads this value
        if (d->save_timer) {
            lv_timer_del(d->save_timer);
            d->save_timer = NULL;
        }
        if (d->pref_key) {
            preferences.begin(PREFS_NAMESPACE, false);
            preferences.putInt(d->pref_key, (int)*(d->value));
            int verify = preferences.getInt(d->pref_key, -999999);
            preferences.end();
            Serial.printf("Saved immediately: %s -> %d (verified=%d)\n",
                          d->pref_key, (int)*(d->value), verify);
        }
        // send correct command to stepper controller
        send_message_to_controller(d->controller_cmd, *(d->value));
        Serial.printf("Speed inc (Controller): %s -> %d\n",
                  d->pref_key ? d->pref_key : "?", (int)*(d->value));
    }
}

// ===== Speed Pulse Delay Controls UI =====

void init_button_styles(void) {
    // Default (released) style
    lv_style_init(&style_move_default);
    lv_style_set_bg_opa(&style_move_default, LV_OPA_COVER);
    lv_style_set_bg_color(&style_move_default,
                          lv_color_hex(MOVE_BTN_COLOR_RELEASED));
    lv_style_set_radius(&style_move_default, 6);
    lv_style_set_pad_all(&style_move_default, 4);
    lv_style_set_border_width(&style_move_default, 0);
    lv_style_set_text_color(&style_move_default, lv_color_hex(0xFFFFFF));

    // Pressed style
    lv_style_init(&style_move_pressed);
    lv_style_set_bg_opa(&style_move_pressed, LV_OPA_COVER);
    lv_style_set_bg_color(&style_move_pressed,
                          lv_color_hex(MOVE_BTN_COLOR_PRESSED));
    lv_style_set_radius(&style_move_pressed, 6);
    lv_style_set_pad_all(&style_move_pressed, 4);
    lv_style_set_border_width(&style_move_pressed, 0);
    lv_style_set_text_color(&style_move_pressed, lv_color_hex(0xFFFFFF));

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
    lv_style_set_bg_color(&style_position_bar,
                          lv_color_hex(0x000080)); // Button blue for indicator
    lv_style_set_radius(&style_position_bar, 4);

    // Signal strength slider style
    lv_style_init(&style_signal_slider);
    lv_style_set_bg_opa(&style_signal_slider, LV_OPA_COVER);
    lv_style_set_bg_color(&style_signal_slider,
                          lv_color_hex(0xFF8000)); // Orange
    lv_style_set_radius(&style_signal_slider, 4);
}

void create_move_buttons() {
    const int numMoves = sizeof(move_buttons) / sizeof(move_buttons[0]);
    const int cols = 3;
    const int spacing_x = MOVE_BTN_SPACING_X, spacing_y = MOVE_BTN_SPACING_Y;
    const int btn_w = MOVE_BTN_WIDTH, btn_h = MOVE_BTN_HEIGHT;
    const int startX =
        (LV_HOR_RES - (cols * btn_w + (cols - 1) * spacing_x)) / 2;
    // Move buttons to 2 pixels from the bottom of the screen
    int screen_h = lv_disp_get_ver_res(NULL);
    // btn_h is already declared above, so remove redeclaration
    // spacing_y is already declared above, so remove redeclaration
    const int rows = (numMoves + cols - 1) / cols;
    const int total_height = rows * btn_h + (rows - 1) * spacing_y;
    const int startY = screen_h - total_height - 4;

    for (int i = 0; i < numMoves; i++) {
        int row = i / cols;
        int col = i % cols;
        int x = startX + col * (btn_w + spacing_x);
        int y = startY + row * (btn_h + spacing_y);

        lv_obj_t* btn = lv_btn_create(lv_scr_act());
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, x, y);

        lv_obj_add_style(btn, &style_move_default,
                         LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_style(btn, &style_move_pressed,
                         LV_PART_MAIN | LV_STATE_PRESSED);

        lv_obj_t* row_cont = lv_obj_create(btn);
        lv_obj_set_size(row_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row_cont, 2, 0);
        lv_obj_clear_flag(row_cont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(row_cont, LV_OPA_TRANSP, 0);
        lv_obj_center(row_cont);

        lv_obj_t* arrow_label = lv_label_create(row_cont);
        lv_label_set_text(arrow_label, move_buttons[i].arrow);
        lv_obj_set_style_text_font(arrow_label, &Arial_Arrows_14, 0);
        lv_obj_clear_flag(arrow_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(arrow_label, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(arrow_label, lv_color_hex(0xFFFFFF), 0);

        lv_obj_t* text_label = lv_label_create(row_cont);
        lv_label_set_text(text_label, move_buttons[i].text);
        lv_obj_set_style_text_font(text_label, LV_FONT_DEFAULT, 0);
        lv_obj_clear_flag(text_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(text_label, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(text_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(row_cont);

        move_btn_indices[i] = i;
        lv_obj_add_event_cb(btn, move_btn_event_cb, LV_EVENT_ALL,
                            &move_btn_indices[i]);
    }
}

void create_home_button() {
    // Create a Home button in the top left
    static lv_obj_t* home_btn = NULL;
    if (home_btn)
        return; // Only create once
    home_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(home_btn, 70, 28);
    lv_obj_set_pos(home_btn, 10, 8);
    lv_obj_set_style_bg_color(home_btn, lv_color_hex(0x0077FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(home_btn, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(home_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(home_btn, lv_color_hex(0x555555),
                                  LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(home_btn);
    lv_label_set_text(label, "Home");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(
        home_btn,
        [](lv_event_t* e) {
            lv_event_code_t code = lv_event_get_code(e);
            lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
            if (code == LV_EVENT_CLICKED) {
                Serial.println("[DEBUG] Home button LV_EVENT_CLICKED: pushing FSM MOVE_TO_HOME event");
                // Turn button red
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
                // Push FSM MOVE_TO_HOME event (triggers CMD_MOVE_TO_HOME)
                fsm::Event ev{};
                ev.type = fsm::EventType::MOVE_TO_HOME;
                ev.int_arg = current_stepper_position;
                Serial.printf("[DEBUG] Home button: MOVE_TO_HOME event int_arg (current_stepper_position) = %ld\n", (long)current_stepper_position);
                g_fsm.push_event(ev);
                Serial.println("[DEBUG] Home button: after push_event, calling g_fsm.tick()");
                // Immediately process FSM event so command is sent without delay
                g_fsm.tick();
                Serial.println("[DEBUG] Home button: after g_fsm.tick() call");
            } else if (code == LV_EVENT_RELEASED) {
                // Restore button color
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x0077FF), LV_PART_MAIN);
            }
        },
        LV_EVENT_ALL, NULL);
}
// Removed: create_limit_indicators function

// ...speed slider UI removed...

// ===== AutoSave & Power Buttons =====

static void autosave_toggle_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return;
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
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

static void power_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("Sending RESET event to FSM...");
        fsm::Event ev{};
        ev.type = fsm::EventType::BTN_RESET;
        g_fsm.push_event(ev);
        Serial.println("Reset event sent to FSM. Waiting for controller to "
                       "restart and send reset back...");
    } else if (code == LV_EVENT_LONG_PRESSED) {
        Serial.println("Power button long-press detected: performing hard "
                       "reset of pulse-delay defaults (persist)...");
        hard_reset_pulse_delays(true);
    }
}

// ===== Radio Button Creators =====
void create_band_radio_buttons() {
    int numBands = sizeof(band_buttons) / sizeof(band_buttons[0]);
    int spacing = BAND_BTN_SPACING;
    int totalWidth = numBands * bandBtn_width + (numBands - 1) * spacing;
    int startX = (LV_HOR_RES - totalWidth) / 2;
    // Move band buttons 5 pixels above the top row of move buttons
    int screen_h = lv_disp_get_ver_res(NULL);
    const int numMoves = sizeof(move_buttons) / sizeof(move_buttons[0]);
    const int cols = 3;
    const int spacing_y = MOVE_BTN_SPACING_Y;
    const int btn_h = MOVE_BTN_HEIGHT;
    const int rows = (numMoves + cols - 1) / cols;
    const int total_height = rows * btn_h + (rows - 1) * spacing_y;
    int top_row_y =
        screen_h - total_height - 4; // 2px from bottom, 4 for padding
    int y_offset = top_row_y - bandBtn_height - 5;

    for (int i = 0; i < numBands; i++) {
        lv_obj_t* btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(btn, bandBtn_width, bandBtn_height);
        lv_obj_set_pos(btn, startX + i * (bandBtn_width + spacing), y_offset);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_style(btn, &style_btn_checked, LV_STATE_CHECKED);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, band_buttons[i].label);
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, band_radio_button_event_cb, LV_EVENT_ALL,
                            (void*)band_buttons[i].label);

        // Restore last selected band from position system
        if (position_system_initialized && i == current_band_index) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            last_band_btn = btn;
            Serial.printf("Restored band button: %s (index %d)\n",
                          band_buttons[i].label, i);
        } else if (!position_system_initialized && i == 0) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            last_band_btn = btn;
            Serial.printf("Default band button: %s (index %d)\n",
                          band_buttons[i].label, i);
        }
    }
}

void create_mode_radio_buttons() {
    int numModes = sizeof(mode_buttons) / sizeof(mode_buttons[0]);
    int spacing = MODE_BTN_SPACING;
    int totalWidth = numModes * modeBtn_width + (numModes - 1) * spacing;
    int startX = (LV_HOR_RES - totalWidth) / 2;
    // Move mode buttons 5 pixels above the band buttons
    int screen_h = lv_disp_get_ver_res(NULL);
    const int numMoves = sizeof(move_buttons) / sizeof(move_buttons[0]);
    const int cols = 3;
    const int spacing_y = MOVE_BTN_SPACING_Y;
    const int btn_h = MOVE_BTN_HEIGHT;
    const int rows = (numMoves + cols - 1) / cols;
    const int total_height = rows * btn_h + (rows - 1) * spacing_y;
    int top_row_y =
        screen_h - total_height - 4; // 2px from bottom, 4 for padding
    int band_y_offset = top_row_y - bandBtn_height - 5;
    int y_offset = band_y_offset - modeBtn_height - 5;

    for (int i = 0; i < numModes; i++) {
        lv_obj_t* btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(btn, modeBtn_width, modeBtn_height);
        lv_obj_set_pos(btn, startX + i * (modeBtn_width + spacing), y_offset);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_style(btn, &style_btn_checked, LV_STATE_CHECKED);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, mode_buttons[i].label);
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, mode_button_event_cb, LV_EVENT_ALL,
                            (void*)mode_buttons[i].label);

        // Restore last selected mode from position system
        if (position_system_initialized && i == current_mode_index) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            last_mode_btn = btn;
            Serial.printf("Restored mode button: %s (index %d)\n",
                          mode_buttons[i].label, i);
        } else if (!position_system_initialized && i == 0) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            last_mode_btn = btn;
            Serial.printf("Default mode button: %s (index %d)\n",
                          mode_buttons[i].label, i);
        }
    }
}

// Create a small autosave toggle button in the top-right (separate from power)
void create_autosave_button() {
    lv_obj_t* autosave_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(autosave_btn, RESET_POWER_BTN_WIDTH,
                    RESET_POWER_BTN_HEIGHT);
    lv_obj_align(autosave_btn, LV_ALIGN_TOP_RIGHT, RESET_BTN_X, RESET_BTN_Y);
    lv_obj_add_style(autosave_btn, &style_toggle_off,
                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(autosave_btn, &style_toggle_on,
                     LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_event_cb(autosave_btn, autosave_toggle_cb, LV_EVENT_CLICKED,
                        NULL);
    g_autosave_btn = autosave_btn;

    if (position_system_initialized && autosave_on) {
        lv_obj_add_state(autosave_btn, LV_STATE_CHECKED);
    }

    lv_obj_t* ab_label = lv_label_create(autosave_btn);
    lv_label_set_text(ab_label, LV_SYMBOL_SAVE);
    lv_obj_center(ab_label);
}

void create_power_button() {
    lv_obj_t* power_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(power_btn, RESET_POWER_BTN_WIDTH, RESET_POWER_BTN_HEIGHT);
    if (g_autosave_btn) {
        lv_obj_align_to(power_btn, g_autosave_btn, LV_ALIGN_OUT_LEFT_MID,
                        POWER_BTN_GAP, 0);
    } else {
        lv_obj_align(power_btn, LV_ALIGN_TOP_RIGHT, POWER_BTN_FALLBACK_X,
                     RESET_BTN_Y);
    }
    lv_obj_add_style(power_btn, &style_toggle_on,
                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(power_btn, power_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* p_label = lv_label_create(power_btn);
    lv_label_set_text(p_label, LV_SYMBOL_POWER);
    lv_obj_center(p_label);
}
void create_message_boxes() {
    Serial.println("Creating message boxes using simple labels...");

    // TX Message Box (Data sent to StepperController) - Using simple label
    tx_message_box = lv_label_create(lv_scr_act());
    lv_obj_set_size(tx_message_box, MESSAGE_BOX_WIDTH,
                    52); // Fits 4 lines of text with minimal padding
    lv_obj_set_pos(tx_message_box, TX_MESSAGE_BOX_X, TX_MESSAGE_BOX_Y);
    lv_label_set_text(tx_message_box, "");
    lv_label_set_long_mode(tx_message_box, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(tx_message_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tx_message_box, lv_color_hex(0x000080), 0);
    lv_obj_set_style_text_color(tx_message_box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(tx_message_box, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_border_width(tx_message_box, 2, 0);
    lv_obj_set_style_pad_all(tx_message_box, 2, 0);
    lv_obj_set_style_text_font(tx_message_box, &lv_font_montserrat_10,
                               0); // Smaller font for compact display

    rx_message_box = lv_label_create(lv_scr_act());
    lv_obj_set_size(rx_message_box, MESSAGE_BOX_WIDTH,
                    52); // Fits 4 lines of text with minimal padding
    lv_obj_set_pos(rx_message_box, RX_MESSAGE_BOX_X, TX_MESSAGE_BOX_Y + 52 + 5);
    lv_label_set_text(rx_message_box, "");
    lv_label_set_long_mode(rx_message_box, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(rx_message_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rx_message_box, lv_color_hex(0x000080),
                              0); // Same as TX
    lv_obj_set_style_text_color(rx_message_box, lv_color_hex(0xFFFFFF),
                                0); // Same as TX
    lv_obj_set_style_border_color(rx_message_box, lv_color_hex(0x0000FF),
                                  0); // Same as TX
    lv_obj_set_style_border_width(rx_message_box, 2, 0);
    lv_obj_set_style_pad_all(rx_message_box, 2, 0);
    lv_obj_set_style_text_font(rx_message_box, &lv_font_montserrat_10, 0);

    Serial.println("Message boxes created successfully using labels");
}
void create_position_display() {
    Serial.println("Creating position display...");

    // Create a container for proper layering
    lv_obj_t* container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container, POSITION_BOX_WIDTH + 10,
                    POSITION_BOX_HEIGHT + 10);
    lv_obj_set_pos(container, POSITION_BOX_X_OFFSET - 5, POSITION_BOX_Y - 5);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP,
                            0); // Transparent container
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    // Create position text box inside container
    g_position_label = lv_label_create(container);
    if (!g_position_label) {
        Serial.println("Failed to create position label!");
        return;
    }
    Serial.printf("Position label created at %p\n", g_position_label);

    // Position and style the text box - make it more visible
    lv_obj_set_size(g_position_label, POSITION_BOX_WIDTH, POSITION_BOX_HEIGHT);
    lv_obj_center(g_position_label); // Center in container

    // Style as a text box with blue background like other UI elements
    lv_obj_set_style_bg_opa(g_position_label, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_position_label, lv_color_hex(0x1E3A8A),
                              0); // Blue background
    lv_obj_set_style_border_width(g_position_label, 2, 0);
    lv_obj_set_style_border_color(g_position_label, lv_color_hex(0xFFFFFF),
                                  0); // White border
    lv_obj_set_style_text_color(g_position_label, lv_color_hex(0xFFFFFF),
                                0); // White text
    lv_obj_set_style_text_font(g_position_label, &lv_font_montserrat_12,
                               0); // Readable font
    lv_obj_set_style_text_align(g_position_label, LV_TEXT_ALIGN_CENTER,
                                0); // Center text
    lv_obj_set_style_pad_all(g_position_label, 5,
                             0); // Match signal label padding for alignment
    lv_obj_set_style_radius(g_position_label, 4, 0); // Rounded corners

    lv_label_set_text(g_position_label, "Pos: 0");
    Serial.printf("Position text set to: %s\n",
                  lv_label_get_text(g_position_label));
    Serial.printf("Position display size: %dx%d at (%d,%d)\n",
                  POSITION_BOX_WIDTH, POSITION_BOX_HEIGHT,
                  POSITION_BOX_X_OFFSET, POSITION_BOX_Y);

    // Force to front
    lv_obj_move_foreground(container);
    lv_obj_move_foreground(g_position_label);

    Serial.println("Position display created successfully");

    // Initialize display with current position
    update_position_display(current_stepper_position);
}

void create_signal_display() {
    Serial.println("Creating signal strength display...");

    // Create a container for signal display
    lv_obj_t* signal_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(signal_container, SIGNAL_BOX_WIDTH + 10,
                    SIGNAL_BOX_HEIGHT + 10);
    lv_obj_set_pos(signal_container, SIGNAL_BOX_X_OFFSET - 5, SIGNAL_BOX_Y - 5);
    lv_obj_set_style_bg_opa(signal_container, LV_OPA_TRANSP,
                            0); // Transparent container
    lv_obj_set_style_border_width(signal_container, 0, 0);
    lv_obj_set_style_pad_all(signal_container, 0, 0);

    // Create signal text box inside container
    g_signal_label = lv_label_create(signal_container);
    if (!g_signal_label) {
        Serial.println("Failed to create signal label!");
        return;
    }

    lv_obj_set_size(g_signal_label, SIGNAL_BOX_WIDTH, SIGNAL_BOX_HEIGHT);
    lv_obj_center(g_signal_label);

    // Style as text box with orange background for visibility
    lv_obj_set_style_bg_opa(g_signal_label, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_signal_label, lv_color_hex(0xFF8000),
                              0); // Orange background
    lv_obj_set_style_border_width(g_signal_label, 2, 0);
    lv_obj_set_style_border_color(g_signal_label, lv_color_hex(0xFFFFFF),
                                  0); // White border
    lv_obj_set_style_text_color(g_signal_label, lv_color_hex(0x000000),
                                0); // Black text
    lv_obj_set_style_text_font(g_signal_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(g_signal_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(g_signal_label, 5, 0);
    lv_obj_set_style_radius(g_signal_label, 4, 0);

    lv_label_set_text(g_signal_label, "Sig: -- dBm");

    // Force to front
    lv_obj_move_foreground(signal_container);
    lv_obj_move_foreground(g_signal_label);

    Serial.println("Signal display created successfully");
}

// ===== UI Builder =====
void create_speed_delay_controls();
void build_ui() {
    init_button_styles();
    create_position_display();
    create_signal_display();
    create_move_buttons();
    create_band_radio_buttons();
    create_mode_radio_buttons();
    create_autosave_button();
    create_power_button();
    create_message_boxes();
    create_home_button();
    create_speed_delay_controls();
    // TODO: Restore correct usage for LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
    // LVGL_VERSION_PATCH, PORTRAIT_ROTATION if needed Example:
    // Serial.printf("LVGL v%d.%d.%d, rotation=%d\n", LVGL_VERSION_MAJOR,
    // LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, PORTRAIT_ROTATION);

    // Initialize position storage system
    Serial.println("Initializing position storage system...");
    if (!load_positions_from_file()) {
        Serial.println("Using default position values");
        initialize_position_array();
    }
    position_system_initialized = true;

    // Display startup state for debugging
    Serial.printf("Startup state - Band: %s (index %d), Mode: %s (index %d), "
                  "Position: %d\n",
                  band_buttons[current_band_index].label, current_band_index,
                  mode_buttons[current_mode_index].label, current_mode_index,
                  (int)current_stepper_position);

    // display_touch_init(); // Commented out undefined function
    Serial.println("Display and touch initialized");

    Serial.println("UI built successfully");

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        while (1)
            ;
    }

    Serial.print("My MAC address is ");
    Serial.println(WiFi.macAddress());

    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    // Initialize ESP-NOW peer info structure completely
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer_info.peer_addr, controller_mac, 6);
    peer_info.channel = 0;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        Serial.println("Failed to add peer");
    }

    // Move stepper to saved position on startup
    Serial.printf("Moving to startup position: %d for %s/%s\n",
                  (int)current_stepper_position,
                  band_buttons[current_band_index].label,
                  mode_buttons[current_mode_index].label);
    send_message_to_controller(CMD_MOVE_TO, current_stepper_position);

    Serial.println("Setup done");
    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    // Initialize ESP-NOW peer info structure completely
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer_info.peer_addr, controller_mac, 6);
    peer_info.channel = 0;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        Serial.println("Failed to add peer");
    }

    // Move stepper to saved position on startup
    Serial.printf("Moving to startup position: %d for %s/%s\n",
                  (int)current_stepper_position,
                  band_buttons[current_band_index].label,
                  mode_buttons[current_mode_index].label);
    send_message_to_controller(CMD_MOVE_TO, current_stepper_position);

    Serial.println("Setup done");
}
// ===== Display & Touch Initialization =====
void display_touch_init() {
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
        while (true) {
        }
    }

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    gfx->fillScreen(RGB565_BLACK);

    touch_controller.begin();
    Wire.setClock(40000);
    delay(10);

    touch_controller.setRotation(ROTATION_INVERTED);

    lv_init();

    screen_width = gfx->width();
    screen_height = gfx->height();

    buf_size_px = screen_width * 40;
    size_t buf_bytes = buf_size_px * sizeof(lv_color_t);
    disp_draw_buf = (lv_color_t*)heap_caps_malloc(
        buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        disp_draw_buf =
            (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_8BIT);
    }
    if (!disp_draw_buf) {
        Serial.println("LVGL disp_draw_buf allocate failed!");
        while (true) {
        }
    }

    disp = lv_display_create(screen_width, screen_height);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, buf_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
}

// FSM instance (single global, globally visible)
// ===== Arduino Setup & Loop =====

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("LVGL: %d.%d.%d  (PORTRAIT_ROTATION=%d)\n",
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                  PORTRAIT_ROTATION);

    // Initialize position storage system
    Serial.println("Initializing position storage system...");
    // Migrate or overwrite prefs with code defaults when PREFS_VERSION changes
    check_and_migrate_prefs();
    if (!load_positions_from_file()) {
        Serial.println("Using default position values");
        initialize_position_array();
    }
    position_system_initialized = true;

    // Display startup state for debugging
    Serial.printf("Startup state - Band: %s (index %d), Mode: %s (index %d), "
                  "Position: %d\n",
                  band_buttons[current_band_index].label, current_band_index,
                  mode_buttons[current_mode_index].label, current_mode_index,
                  (int)current_stepper_position);

    display_touch_init();
    Serial.println("Display and touch initialized");

    build_ui();
    Serial.println("UI built successfully");

    Serial.println("[DEBUG] Setting WiFi to STA mode...");
    WiFi.mode(WIFI_STA);
    Serial.println("[DEBUG] WiFi STA mode set");
    Serial.println("[DEBUG] Initializing ESP-NOW...");
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed!");
        while (1)
            ;
    }
    Serial.println("[DEBUG] ESP-NOW initialized");

    Serial.print("[DEBUG] My MAC address is ");
    Serial.println(WiFi.macAddress());

    Serial.println("[DEBUG] Registering ESP-NOW recv callback...");
    esp_now_register_recv_cb(on_data_recv);
    Serial.println("[DEBUG] ESP-NOW recv callback registered");
    Serial.println("[DEBUG] Registering ESP-NOW send callback...");
    esp_now_register_send_cb(on_data_sent);
    Serial.println("[DEBUG] ESP-NOW send callback registered");

    // Initialize ESP-NOW peer info structure completely
    Serial.println("[DEBUG] Initializing ESP-NOW peer info...");
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer_info.peer_addr, controller_mac, 6);
    peer_info.channel = 0;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = false;

    Serial.println("[DEBUG] Deleting existing ESP-NOW peer (if any)...");
    esp_now_del_peer(controller_mac); // Ignore result, just ensure clean state
    Serial.println("[DEBUG] Adding ESP-NOW peer...");
    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        Serial.println("[ERROR] Failed to add ESP-NOW peer!");
    } else {
        Serial.println("[DEBUG] ESP-NOW peer added");
    }

    // 1. Move to home position (StepperController should set stepper at home)
    Serial.println("[SETUP] Sending MOVE_TO_HOME (FSM)");
    fsm::Event move_home_ev{};
    move_home_ev.type = fsm::EventType::MOVE_TO_HOME;
    move_home_ev.int_arg = 0; // Home position is 0
    g_fsm.push_event(move_home_ev);

    // 2. Set position to 0 (update GUI and internal state)
    current_stepper_position = 0;
    update_position_display(current_stepper_position);
    Serial.println("[SETUP] Stepper position set to 0");

    // Do NOT move to last band/mode position yet; wait for HOME_COMPLETE
    Serial.println("Setup done");

    // Initialize FSM: provide send command and UI update callbacks
    g_fsm.begin(
        // send callback - always print [SENT CMD] and map all movement types
        [](const char* cmd, int32_t arg) {
            if (strcmp(cmd, "MOVE_TO") == 0) {
                send_message_to_controller(CMD_MOVE_TO, arg);
            } else if (strcmp(cmd, "MOVE_UP") == 0) {
                send_message_to_controller(CMD_UP_FAST);
            } else if (strcmp(cmd, "MOVE_DOWN") == 0) {
                send_message_to_controller(CMD_DOWN_FAST);
            } else if (strcmp(cmd, "STOP") == 0) {
                send_message_to_controller(CMD_STOP);
            } else if (strcmp(cmd, "MOVE_TO_HOME") == 0) {
                Serial.println("[DEBUG] FSM send callback: MOVE_TO_HOME received");
                send_message_to_controller(CMD_MOVE_TO_HOME, 0);
            } else if (strcmp(cmd, "RESET") == 0) {
                Serial.println("[DEBUG] FSM send callback: RESET received");
                send_message_to_controller(CMD_RESET, 0);
            } else {
                // fallback for unknown commands
                Serial.printf("[SENT CMD] Unknown FSM command: %s %ld\n", cmd,
                              (long)arg);
            }
        },
        // UI update callback - update UI based on FSM state
        [](fsm::State s) {
            Serial.printf("FSM state -> %d\n", (int)s);
            static lv_obj_t* home_btn = nullptr;
            if (!home_btn) {
                // Find the home button by its position/size (since it's static in create_home_button)
                lv_obj_t* scr = lv_scr_act();
                uint32_t child_cnt = lv_obj_get_child_cnt(scr);
                for (uint32_t i = 0; i < child_cnt; ++i) {
                    lv_obj_t* obj = lv_obj_get_child(scr, i);
                    if (lv_obj_get_x(obj) == 10 && lv_obj_get_y(obj) == 8 && lv_obj_get_width(obj) == 70 && lv_obj_get_height(obj) == 28) {
                        home_btn = obj;
                        break;
                    }
                }
            }
            switch (s) {
            case fsm::State::IDLE:
                update_position_display(current_stepper_position);
                if (home_btn) {
                    // Turn Home button green
                    lv_obj_set_style_bg_color(home_btn, lv_color_hex(0x00C800), LV_PART_MAIN);
                }
                break;
            case fsm::State::MOVING_UP:
            case fsm::State::MOVING_DOWN:
            case fsm::State::MOVING_TO:
                // Show moving indicator (customize as needed)
                break;
            case fsm::State::RESETTING:
                // Show resetting indicator (customize as needed)
                break;
            case fsm::State::ERROR:
                // Show error indicator (customize as needed)
                break;
            default:
                break;
            }
        });

    // Push a small test event so FSM is exercised at startup
    // Push a small test event so FSM is exercised at startup
    fsm::Event startup_ev{};
    startup_ev.type = fsm::EventType::BTN_MOVE_TO;
    startup_ev.int_arg = current_stepper_position; // target position
    g_fsm.push_event(startup_ev);
}

void loop() {
    // Feed watchdog timer first to prevent timeout
    esp_task_wdt_reset();

    // Process FSM events (guarantees MOVE_TO and all events are handled)
    g_fsm.tick();

    // Handle deferred position updates from ESP-NOW interrupt
    if (position_update_pending) {
        position_update_pending = false;
        Serial.printf(
            "*** DEFERRED UPDATE: Processing pending_position=%d ***\n",
            (int)pending_position);
        update_current_position(pending_position);
        Serial.printf(
            "*** DEFERRED UPDATE: Position display should now show: %d ***\n",
            (int)pending_position);
    }

    // Handle deferred signal strength updates
    if (signal_update_pending) {
        signal_update_pending = false;
        update_signal_display(last_rssi);
    }

    // Periodic signal strength evaluation and heartbeat management (every 1
    // second)
    uint32_t now = millis();

    // Handle deferred message box updates immediately (safe in main loop
    // context)
    if (tx_message_update_pending) {
        Serial.printf(
            "loop: tx_message_update_pending TRUE, tx_message_box=%p\n",
            tx_message_box);
        if (tx_message_box && lv_obj_is_valid(tx_message_box)) {
            tx_message_update_pending = false;
            static char tx_display[500];
            tx_display[0] = '\0';
            for (int i = 0; i < tx_message_count; i++) {
                strcat(tx_display, "TX: ");
                strcat(tx_display, tx_messages[i]);
                if (i < tx_message_count - 1)
                    strcat(tx_display, "\n");
            }
            lv_label_set_text(tx_message_box, tx_display);
            Serial.printf("loop: TX message box updated\n");
        } else {
            Serial.printf("loop: tx_message_box is NULL or invalid!\n");
        }
    }

    if (rx_message_update_pending) {
        Serial.printf(
            "loop: rx_message_update_pending TRUE, rx_message_box=%p\n",
            rx_message_box);
        if (rx_message_box && lv_obj_is_valid(rx_message_box)) {
            rx_message_update_pending = false;
            static char rx_display[500];
            rx_display[0] = '\0';
            for (int i = 0; i < rx_message_count; i++) {
                strcat(rx_display, "RX: ");
                strcat(rx_display, rx_messages[i]);
                if (i < rx_message_count - 1)
                    strcat(rx_display, "\n");
            }
            lv_label_set_text(rx_message_box, rx_display);
            Serial.printf("loop: RX message box updated\n");
        } else {
            Serial.printf("loop: rx_message_box is NULL or invalid!\n");
        }
    }

    // Update status and limit indicators (check every loop cycle for
    // responsiveness)
    static uint32_t last_status_update = 0;
    bool should_update = (now - last_status_update >= 200);
    if (should_update) {
        last_status_update = now;
        // Removed: update_limit_indicators()
    }
    if (now - last_signal_update >= 1000) {
        last_signal_update = now;
        // evaluate_signal_strength(); // Commented out undefined function
    }

    // Always run LVGL tick and timer handler for UI responsiveness
    lv_tick_inc(5);
    lv_timer_handler();
    delay(5);
}

// (Save/Reset buttons and dialog removed — persistence happens immediately and
// reset is available via power-button long-press which still calls
// hard_reset_pulse_delays(true)).