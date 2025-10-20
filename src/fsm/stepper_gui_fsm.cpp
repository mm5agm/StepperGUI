
#include <Arduino.h>
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
    g_event_queue.push(ev);
}

void StepperGuiFsm::tick() {
    // Drain at most N events per tick to avoid starvation
    const int max_per_tick = 8;
    int processed = 0;
    while (processed < max_per_tick) {
        Event ev;
        {
            std::lock_guard<std::mutex> lk(g_event_mutex);
            if (g_event_queue.empty()) break;
            ev = g_event_queue.front();
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
    switch (state_) {
        case State::IDLE:
            switch (ev.type) {
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
                default:
                    break;
            }
            break;

        case State::MOVING_TO:
            switch (ev.type) {
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
                case EventType::BTN_MOVE_TO:
                    if (send_cmd_) send_cmd_("MOVE_TO", ev.int_arg);
                    transition_to(State::MOVING_TO);
                    break;
                case EventType::ESP_POS_UPDATE:
                    // Update pos in UI (callback)
                    break;
                case EventType::LIMIT_UP:
                case EventType::LIMIT_DOWN:
                case EventType::BTN_STOP:
                    if (send_cmd_) send_cmd_("STOP", 0);
                    transition_to(State::IDLE);
                    break;
                default:
                    break;
            }
            break;



        case State::RESETTING:
            // For now, treat as busy state; accept limit events
            if (ev.type == EventType::LIMIT_DOWN || ev.type == EventType::LIMIT_UP) {
                transition_to(State::IDLE);
            }
            break;

        case State::MOVE_TO_DOWN_LIMIT:
            // Handle MOVE_TO_DOWN_LIMIT state if needed (currently no-op)
            // Add logic here if special handling is required
            break;
        case State::ERROR:
            // require manual reset
            break;
    }
}
