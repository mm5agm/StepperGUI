# StepperGUI

ESP32-S3 based GUI for controlling stepper motor systems using ESP-NOW wireless communication. Features comprehensive debugging capabilities.

## Hardware


## Key Features

### Safety & Control

### User Interface

### Communication & Debugging

## Hardware Setup

### JC4827W543C Device



## End-Stop Detection System

This version does not clamp position values in software. Instead, safety limits are enforced by hardware end-stop detection using the TCRT5000 Infrared Reflective Photoelectric Switch IR Tracking Sensor Module. The sensor reliably detects the physical limits of the stepper motor travel, and the firmware halts or reverses the motor when an end-stop is reached.

### Position Flow
1. ESP-NOW receives position from StepperController
2. Raw position shown in RX message box
3. End-stop sensor state is monitored to prevent movement beyond physical limits
4. Position is updated and shown in the GUI
5. Motor is stopped or reversed if an end-stop is triggered

## Project Structure

```
src/
├── main.cpp              # Main application with position limiting and ESP-NOW
├── application/          # Application framework
└── devices/             # Device-specific configurations (JC4827W543C)

include/
├── lv_conf.h            # LVGL 9.3.0 configuration with Montserrat fonts
├── stepper_commands.h   # ESP-NOW command definitions
├── stepper_helpers.h    # Utility functions
└── Arial_Arrows_14.h    # Custom font for UI elements

lib/
├── TFT_eSPI-2.5.43/     # Display library
├── Touch_GT911/         # Capacitive touch driver
└── XPT2046_*/           # Alternative touch drivers (unused)
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
**NOTE: This is the final version of StepperGUI using mechanical limit switches and position limiting. Future development will use the TCRT5000 Infrared Reflective Photoelectric Switch IR Tracking Sensor Module for end-stop detection, and position limiting code will be removed.**

All position limiting and limit switch logic is now archived for reference. If you need the last version with limit switches, use the `limit-switches-final` git tag or branch.

* PlatformIO 6.1.18
### End-Stop Detection

This version uses the TCRT5000 Infrared Reflective Photoelectric Switch IR Tracking Sensor Module for end-stop detection. The sensor is used to reliably detect the physical limits of the stepper motor travel, providing accurate and safe position limiting.

Wiring and integration details:
- Connect the TCRT5000 sensor output to the designated end-stop input pin on the ESP32-S3.
- The firmware reads the sensor state to determine when the end-stop is reached and halts or reverses the motor as needed.

Refer to the schematic and code comments for further integration details.
