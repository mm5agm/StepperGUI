#include "globals.hpp"

Button mode_buttons[4] = { {"CW"}, {"SSB"}, {"FT4"}, {"FT8"} };
Button band_buttons[6] = { {"10"}, {"12"}, {"15"}, {"17"}, {"20"}, {"30"} };
#include "globals.hpp"
#include <stdint.h>

int32_t positionArray[POSITION_ROWS][POSITION_COLS];
int current_band_index = 0;
int current_mode_index = 0;
int32_t current_stepper_position = 0;
bool position_system_initialized = false;
bool autosave_on = false;
bool position_update_pending = false;
int32_t pending_position = 0;
int8_t last_rssi = -100;
bool signal_update_pending = false;
bool signal_forced_offline = false;
bool tx_message_update_pending = false;
bool rx_message_update_pending = false;
char tx_messages[MAX_MESSAGES][60];
char rx_messages[MAX_MESSAGES][60];
int tx_message_count = 0;
int rx_message_count = 0;
uint32_t last_esp_now_msg_time = 0;
int consecutive_msgs = 0;
uint32_t last_signal_update = 0;
uint32_t last_heartbeat_received = 0;
int heartbeat_success_count = 0;
int heartbeat_total_count = 0;
