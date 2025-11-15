#include <atomic>


// Store the last requested band/mode position (legacy, for BTN_HOME)
static std::atomic<int32_t> g_last_band_mode_position{0};
// Store the position before MOVE_TO_HOME (for Home button logic)
static std::atomic<int32_t> g_pre_home_position{0};


#include <Arduino.h>
#include "globals.hpp"
#include "fsm/stepper_gui_fsm.hpp"

#include <queue>
#include <mutex>
#include <functional>
#include <algorithm>

using namespace fsm;

StepperGuiFsm::StepperGuiFsm() = default;

void StepperGuiFsm::begin(SendCommandCb send_cb, UpdateUiCb ui_cb) {
    send_cmd_ = send_cb;
    update_ui_ = ui_cb;
    // Update UI to initial state
    if (update_ui_) update_ui_(state_);
}

// Very small thread-safe in-process queue using std::queue + mutex
static std::queue<Event> g_event_queue;
static std::mutex g_event_mutex;

void StepperGuiFsm::push_event(const Event &ev) {
    std::lock_guard<std::mutex> lk(g_event_mutex);
        Serial.printf("[DEBUG] FSM: push_event called, type=%d\n", (int)ev.type);
        g_event_queue.push(ev);
        Serial.printf("[DEBUG] FSM: event queue size after push: %zu\n", g_event_queue.size());
}

void StepperGuiFsm::tick() {
    // Drain at most N events per tick to avoid starvation
    const int max_per_tick = 8;
    int processed = 0;
        if (!g_event_queue.empty()) {
            Serial.printf("[DEBUG] FSM: tick() called, queue size=%zu\n", g_event_queue.size());
        }
        while (processed < max_per_tick) {
        Event ev;
        {
            std::lock_guard<std::mutex> lk(g_event_mutex);
            if (g_event_queue.empty()) break;
            ev = g_event_queue.front();
                Serial.printf("[DEBUG] FSM: popping event type=%d\n", (int)ev.type);
            g_event_queue.pop();
        }
        handle_event(ev);
        processed++;
    }
}

void StepperGuiFsm::transition_to(State s) {
    if (state_ == s) return;
    state_ = s;
    if (update_ui_) update_ui_(state_);
}

void StepperGuiFsm::handle_event(const Event &ev) {
    Serial.printf("[DEBUG] FSM: handle_event called, state=%d, event type=%d\n", (int)state_, (int)ev.type);
    switch (state_) {
        case State::IDLE:
            switch (ev.type) {
            case EventType::MOVE_TO_HOME:
                Serial.println("[DEBUG] FSM: handle_event MOVE_TO_HOME entered");
                Serial.println("[DEBUG] FSM: about to call send_cmd_ for MOVE_TO_HOME");
                g_pre_home_position = ev.int_arg; // Save current position before homing
                Serial.printf("[DEBUG] FSM: g_pre_home_position set to %ld\n", (long)ev.int_arg);
                if (send_cmd_) send_cmd_("MOVE_TO_HOME", 0);
                transition_to(State::MOVING_TO);
                break;
                case EventType::BTN_MOVE_UP:
                    if (send_cmd_) send_cmd_("MOVE_UP", 0);
                    transition_to(State::MOVING_UP);
                    break;
                case EventType::BTN_MOVE_DOWN:
                    if (send_cmd_) send_cmd_("MOVE_DOWN", 0);
                    transition_to(State::MOVING_DOWN);
                    break;
                case EventType::BTN_MOVE_TO:
                    // Always call send_cmd_ for MOVE_TO to guarantee [SENT CMD] output
                    if (send_cmd_) send_cmd_("MOVE_TO", ev.int_arg);
                    transition_to(State::MOVING_TO);
                    break;
                case EventType::ESP_POS_UPDATE:
                    // stay idle, update UI only
                    break;
                case EventType::BTN_HOME:
                    // Store the current band/mode position before homing
                    if (ev.int_arg != 0) {
                        g_last_band_mode_position = ev.int_arg;
                    }
                    if (send_cmd_) send_cmd_("HOME", 0);
                    transition_to(State::RESETTING);
                    break;
                case EventType::HOME_COMPLETE:
                    // Home complete, update UI/state
                    if (update_ui_) update_ui_(State::IDLE);
                    Serial.printf("[DEBUG] FSM: HOME_COMPLETE (MOVING_TO) handler, g_pre_home_position=%ld\n", (long)g_pre_home_position.load());
                    if (send_cmd_ && g_pre_home_position != 0) {
                        Serial.printf("[DEBUG] FSM: After HOME_COMPLETE (MOVING_TO), returning to pre-home position %ld\n", (long)g_pre_home_position.load());
                        send_cmd_("MOVE_TO", g_pre_home_position);
                    } else {
                        Serial.println("[DEBUG] FSM: HOME_COMPLETE (MOVING_TO) - g_pre_home_position is 0, not sending MOVE_TO");
                    }
                    transition_to(State::IDLE);
                    break;
                case EventType::HOME_FAILED:
                    transition_to(State::ERROR);
                    break;
                case EventType::SENSOR_STATUS:
                    // Handle sensor status event
                    break;
                case EventType::SET_HOME_POSITION:
                    if (send_cmd_) send_cmd_("SET_HOME_POSITION", ev.int_arg);
                    break;
                case EventType::SENSOR_ERROR:
                    transition_to(State::ERROR);
                    break;
                default:
                    break;
            }
            break;

        case State::MOVING_TO:
            switch (ev.type) {
                case EventType::MOVE_TO_HOME:
                    Serial.println("[DEBUG] FSM: handle_event MOVE_TO_HOME (MOVING_TO) entered");
                    Serial.println("[DEBUG] FSM: about to call send_cmd_ for MOVE_TO_HOME (MOVING_TO)");
                    g_pre_home_position = ev.int_arg; // Save current position before homing
                    Serial.printf("[DEBUG] FSM: g_pre_home_position set to %ld (MOVING_TO)\n", (long)ev.int_arg);
                    if (send_cmd_) send_cmd_("MOVE_TO_HOME", 0);
                    transition_to(State::MOVING_TO);
                    break;
                case EventType::HOME_COMPLETE:
                    // Home complete, update UI/state
                    if (update_ui_) update_ui_(State::IDLE);
                    static bool startup_move_sent = false;
                    if (!startup_move_sent) {
                        int32_t last_band_mode_position = positionArray[current_band_index + 1][current_mode_index];
                        Serial.printf("[FSM] After HOME_COMPLETE (startup), moving to last band/mode position %d for %s/%s\n",
                            (int)last_band_mode_position,
                            band_buttons[current_band_index].label,
                            mode_buttons[current_mode_index].label);
                        if (send_cmd_) send_cmd_("MOVE_TO", last_band_mode_position);
                        startup_move_sent = true;
                    } else {
                        Serial.printf("[DEBUG] FSM: HOME_COMPLETE (MOVING_TO) handler, g_pre_home_position=%ld\n", (long)g_pre_home_position.load());
                        if (send_cmd_ && g_pre_home_position != 0) {
                            Serial.printf("[DEBUG] FSM: After HOME_COMPLETE (MOVING_TO), returning to pre-home position %ld\n", (long)g_pre_home_position.load());
                            send_cmd_("MOVE_TO", g_pre_home_position);
                        } else {
                            Serial.println("[DEBUG] FSM: HOME_COMPLETE (MOVING_TO) - g_pre_home_position is 0, not sending MOVE_TO");
                        }
                    }
                    transition_to(State::IDLE);
                    break;
                case EventType::BTN_MOVE_TO:
                    if (send_cmd_) send_cmd_("MOVE_TO", ev.int_arg);
                    // Optionally stay in MOVING_TO or reset state
                    break;
                case EventType::ESP_POS_UPDATE:
                    if (ev.pos == ev.int_arg) {
                        // reached target
                        transition_to(State::IDLE);
                    }
                    break;
                case EventType::BTN_STOP:
                    if (send_cmd_) send_cmd_("STOP", 0);
                    transition_to(State::IDLE);
                    break;
                default:
                    break;
            }
            break;

        case State::MOVING_UP:
        case State::MOVING_DOWN:
            switch (ev.type) {
                case EventType::MOVE_TO_HOME:
                    Serial.println("[DEBUG] FSM: handle_event MOVE_TO_HOME (MOVING_UP/DOWN) entered");
                    Serial.println("[DEBUG] FSM: about to call send_cmd_ for MOVE_TO_HOME (MOVING_UP/DOWN)");
                    g_pre_home_position = ev.int_arg; // Save current position before homing
                    Serial.printf("[DEBUG] FSM: g_pre_home_position set to %ld (MOVING_UP/DOWN)\n", (long)ev.int_arg);
                    if (send_cmd_) send_cmd_("MOVE_TO_HOME", 0);
                    transition_to(State::MOVING_TO);
                    break;
                case EventType::BTN_MOVE_TO:
                    if (send_cmd_) send_cmd_("MOVE_TO", ev.int_arg);
                    transition_to(State::MOVING_TO);
                    break;
                case EventType::ESP_POS_UPDATE:
                    // Update pos in UI (callback)
                    break;
                // Removed: LIMIT_UP and LIMIT_DOWN event types
                case EventType::BTN_STOP:
                    if (send_cmd_) send_cmd_("STOP", 0);
                    transition_to(State::IDLE);
                    break;
                default:
                    break;
            }
            break;



        case State::RESETTING:
            // For now, treat as busy state
            break;
        case State::ERROR:
            // require manual reset
            break;
    }
}
