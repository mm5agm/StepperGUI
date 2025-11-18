typedef struct {
	const char* label;
} Button;

extern Button mode_buttons[4];
extern Button band_buttons[6];
#pragma once
#include <stdint.h>
//#include "config.hpp"
 
#ifndef POSITION_ROWS
#define POSITION_ROWS 7
#endif
#ifndef POSITION_COLS
#define POSITION_COLS 4
#endif

#ifndef MAX_MESSAGES
#define MAX_MESSAGES 4
#endif
 
extern int32_t positionArray[POSITION_ROWS][POSITION_COLS];
extern int current_band_index;
extern int current_mode_index;
extern int32_t g_pre_home_position;
extern int32_t g_last_band_mode_position;
extern int32_t current_stepper_position;
extern bool position_system_initialized;
extern bool autosave_on;
extern bool position_update_pending;
extern int32_t pending_position;
extern int8_t last_rssi;
extern bool signal_update_pending;
extern bool signal_forced_offline;
extern bool tx_message_update_pending;
extern bool rx_message_update_pending;
extern char tx_messages[MAX_MESSAGES][60];
extern char rx_messages[MAX_MESSAGES][60];
extern int tx_message_count;
extern int rx_message_count;
extern uint32_t last_esp_now_msg_time;
extern int consecutive_msgs;
extern uint32_t last_signal_update;
extern uint32_t last_heartbeat_received;
extern int heartbeat_success_count;
extern int heartbeat_total_count;
