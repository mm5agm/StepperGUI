// Minimal FSM test harness for StepperGUI
// To build/run: g++ -std=c++17 -o fsm_test fsm_test.cpp
#include "../src/fsm/stepper_gui_fsm.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace fsm;

struct CallbackLog {
    std::vector<std::string> commands;
    std::vector<State> states;
};

void test_idle_to_moving_up() {
    CallbackLog log;
    StepperGuiFsm fsm;
    fsm.begin(
        [&](const char* cmd, int32_t) { log.commands.push_back(cmd); },
        [&](State s) { log.states.push_back(s); }
    );
    Event ev{EventType::BTN_MOVE_UP};
    fsm.push_event(ev);
    fsm.tick();
    assert(fsm.state() == State::MOVING_UP);
    assert(!log.commands.empty() && log.commands.back() == "MOVE_UP");
    std::cout << "test_idle_to_moving_up passed\n";
}

void test_idle_to_moving_down() {
    CallbackLog log;
    StepperGuiFsm fsm;
    fsm.begin(
        [&](const char* cmd, int32_t) { log.commands.push_back(cmd); },
        [&](State s) { log.states.push_back(s); }
    );
    Event ev{EventType::BTN_MOVE_DOWN};
    fsm.push_event(ev);
    fsm.tick();
    assert(fsm.state() == State::MOVING_DOWN);
    assert(!log.commands.empty() && log.commands.back() == "MOVE_DOWN");
    std::cout << "test_idle_to_moving_down passed\n";
}

void test_idle_to_move_to() {
    CallbackLog log;
    StepperGuiFsm fsm;
    fsm.begin(
        [&](const char* cmd, int32_t arg) { log.commands.push_back(cmd); },
        [&](State s) { log.states.push_back(s); }
    );
    Event ev{EventType::BTN_MOVE_TO, 123};
    fsm.push_event(ev);
    fsm.tick();
    assert(fsm.state() == State::MOVING_TO);
    assert(!log.commands.empty() && log.commands.back() == "MOVE_TO");
    std::cout << "test_idle_to_move_to passed\n";
}

void test_moving_to_idle_on_stop() {
    CallbackLog log;
    StepperGuiFsm fsm;
    fsm.begin(
        [&](const char* cmd, int32_t arg) { log.commands.push_back(cmd); },
        [&](State s) { log.states.push_back(s); }
    );
    // Go to MOVING_UP
    fsm.push_event({EventType::BTN_MOVE_UP});
    fsm.tick();
    // Now stop
    fsm.push_event({EventType::BTN_STOP});
    fsm.tick();
    assert(fsm.state() == State::IDLE);
    std::cout << "test_moving_to_idle_on_stop passed\n";
}

int main() {
    test_idle_to_moving_up();
    test_idle_to_moving_down();
    test_idle_to_move_to();
    test_moving_to_idle_on_stop();
    std::cout << "All FSM tests passed!\n";
    return 0;
}
