# StepperGUI FSM Automated Test Plan

This document describes the approach for automated testing of the StepperGUI FSM logic.

## Goals
- Ensure all movement, stop, and reset actions are routed through the FSM.
- Validate correct state transitions for all events.
- Confirm UI and hardware feedback are triggered only by FSM state changes.

## Approach
- Use a test harness in C++ to instantiate the FSM and simulate events.
- Mock the send command and UI update callbacks to capture outputs.
- Assert expected state transitions and callback invocations.

## Test Cases
1. **Idle to Moving Up**: Push BTN_MOVE_UP event, expect MOVING_UP state and send_cmd callback.
2. **Idle to Moving Down**: Push BTN_MOVE_DOWN event, expect MOVING_DOWN state and send_cmd callback.
3. **Idle to Move To**: Push BTN_MOVE_TO event, expect MOVING_TO state and send_cmd callback.
4. **Moving to Idle on Stop**: While MOVING_UP, push BTN_STOP, expect IDLE state and send_cmd callback.
5. **Resetting**: Push BTN_RESET event, expect RESETTING state and send_cmd callback.
6. **Error Handling**: Simulate error event, expect ERROR state and UI update.

## Implementation
- Place test code in `test/fsm_test.cpp`.
- Use asserts or a minimal test framework (e.g., doctest or Catch2 if available).
- Run tests on host (not embedded target) for speed.

---

You can implement the above plan in `test/fsm_test.cpp`.
