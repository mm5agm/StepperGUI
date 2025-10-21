// Minimal state machine for StepperGUI
#pragma once

#include <cstdint>
#include <functional>

namespace fsm {

enum class State : uint8_t {
    IDLE,
    MOVING_UP,
    MOVING_DOWN,
    MOVING_TO,
    RESETTING,
    ERROR,
};


enum class EventType : uint8_t {
    BTN_MOVE_UP,
    BTN_MOVE_DOWN,
    BTN_MOVE_TO,
    BTN_STOP,
    BTN_RESET,
    ESP_POS_UPDATE,
    TIMEOUT,
    ERROR,
    BTN_HOME,
    HOME_COMPLETE,
    HOME_FAILED,
    SENSOR_STATUS,
    SET_HOME_POSITION,
    MOVE_TO_HOME,
    SENSOR_ERROR,
};

struct Event {
    EventType type;
    int32_t int_arg{0};
    int32_t pos{0};
};

class StepperGuiFsm {
public:
    using SendCommandCb = std::function<void(const char *cmd, int32_t arg)>;
    using UpdateUiCb = std::function<void(State)>;

    StepperGuiFsm();

    void begin(SendCommandCb send_cb, UpdateUiCb ui_cb);
    void push_event(const Event &ev);
    void tick(); // call from main loop

    State state() const { return state_; }

    // Added for GUI movement command dispatch
    void send_move_command(EventType type, int speed);

private:
    State state_{State::IDLE};
    SendCommandCb send_cmd_{nullptr};
    UpdateUiCb update_ui_{nullptr};

    // Simple internal helpers
    void transition_to(State s);
    void handle_event(const Event &ev);
};

} // namespace fsm
