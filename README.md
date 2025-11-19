# StepperGUI


Use at your own risk. 

This version works for me, it might not work for you. 

I've stopped work on this project for the time being.


ESP32-S3 based GUI for controlling stepper motor systems using ESP-NOW wireless communication. Features comprehensive debugging capabilities.

## Hardware


## Key Features

### Safety & Control

### User Interface

### Communication & Debugging

## Hardware Setup

### JC4827W543C Device



## End-Stop Detection System

 THe home position is detected by hardware  using the TCRT5000 Infrared Reflective Photoelectric Switch IR Tracking Sensor Module. Since my magloop capacitor can move 360 degrees I have decided against having an end stop indicator. I may add that later. 

### Position Flow
1. ESP-NOW receives position from StepperController
2. Raw position shown in RX message box
3. Position is updated and shown in the GUI

## Project Structure


```
src/
├── main.cpp                # Main application logic
├── globals.cpp, globals.hpp # Global variables and constants
├── devices/
│   └── JC4827W543C.hpp     # Device-specific configuration
├── fsm/
│   ├── stepper_gui_fsm.cpp # Finite state machine logic
│   └── stepper_gui_fsm.hpp # FSM header

include/
├── Arial_Arrows_14.h       # Custom font for UI elements
├── globals.hpp             # Shared global declarations
├── lv_conf.h               # LVGL configuration

MagLoop_Common_Files/
├── circular_buffer.h       # Utility for buffering
├── stepper_commands.h      # ESP-NOW command definitions
├── stepper_helpers.h       # Utility functions

lib/
├── Touch_GT911/
│   ├── Touch_GT911.cpp     # Capacitive touch driver
│   └── Touch_GT911.h       # Touch driver header

etc/, data/, backups/, logs/ # Additional project folders
```

## Build Environments


### JC4827W543C build notes
For the `JC4827W543C` environment the build requires an extra include path and a few LVGL compile-time defines so that shared headers and Montserrat font objects are found during compilation and linking. If you edit `platformio.ini` manually, ensure the environment includes:


These flags are applied to the `[env:JC4827W543C]` build_flags in the project so the LVGL library builds the Montserrat font objects and enables logging. Without them you may see missing-header or undefined LVGL font / logging symbols at link time.

## Configuration

Key configuration constants in `main.cpp`:
```cpp

uint8_t controllerMAC[] = {0xEC, 0xE3, 0x34, 0xC0, 0x33, 0xC0};
```

Touch calibration and display pins are configured for JC4827W543C hardware.

## Recent Updates




## Batch Scripts for Syncing Code

- **download_latest_code.bat**: Pulls the latest code from both StepperGUI and MagLoop_Common_Files.
- **upload_code_if_changed.bat**: Checks for changes in both repositories and pushes updates only if changes are detected.

### Usage

1. Double-click or run `download_latest_code.bat` to update your local code from GitHub.
2. Double-click or run `upload_code_if_changed.bat` to push any local changes to GitHub. If there are no changes, the script will warn and do nothing.

These scripts automate the most common sync operations for development.

## development environment

---


All position limiting and limit switch logic is now archived for reference. If you need the last version with limit switches, use the `limit-switches-final` git tag or branch.

* PlatformIO 6.1.18
### End-Stop Detection

This version uses the TCRT5000 Infrared Reflective Photoelectric Switch IR Tracking Sensor Module for end-stop detection. The sensor is used to reliably detect the physical limits of the stepper motor travel, providing accurate and safe position limiting.

Wiring and integration details:
- Connect the TCRT5000 sensor output to the designated end-stop input pin on the ESP32-S3.
- The firmware reads the sensor state to determine when the end-stop is reached and halts or reverses the motor as needed.

Refer to the schematic and code comments for further integration details.
