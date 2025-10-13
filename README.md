# StepperGUI

ESP32-S3 based GUI for controlling stepper motor systems using ESP-NOW wireless communication. Features position limiting for safety and comprehensive debugging capabilities.

## Hardware

- **Display**: JC4827W543C - 4.3" ESP32-S3 with 480x272 capacitive touchscreen
- **Communication**: ESP-NOW wireless protocol
- **GUI Framework**: LVGL 9.3.0 with Montserrat fonts
- **Target**: Magnetic loop antenna stepper motor control

## Key Features

### Safety & Control
- **Position Limiting**: Automatic clamping to safe range (50-1550 steps) to prevent limit switch triggers
- **Real-time Position Monitoring**: Dual display showing raw ESP-NOW messages and clamped positions
- **Band/Mode Memory System**: Store and recall positions for different amateur radio bands and modes
- **Auto-save Functionality**: Persistent storage of positions and settings

### User Interface
- **Touch-optimized Interface**: Designed for 272px portrait width with finger-friendly controls
- **Message Box Debugging**: 7-line scrolling display for ESP-NOW communication monitoring
- **Position Text Boxes**: Clear blue/orange displays for position and signal strength
- **Limit Switch Indicators**: Visual feedback for Up/Down limit switch states

### Communication & Debugging
- **ESP-NOW Wireless**: Reliable peer-to-peer communication with stepper controller
- **Deferred Update System**: Interrupt-safe ESP-NOW message handling
- **Comprehensive Logging**: Debug output for position synchronization and message flow
- **RSSI Monitoring**: Signal strength display for communication quality

## Hardware Setup

### JC4827W543C Device
- **Display**: 272 x 480 pixels (portrait orientation)
- **Performance**: ~43 frames per second
- **Touch**: GT911 capacitive touch controller
- **Communication**: ESP-NOW for wireless stepper motor control
- **Power**: USB-C programming and power

## Position Limiting System

The GUI implements safety limits to prevent hardware damage:

- **Minimum Position**: 50 steps (safe margin above down limit switch)
- **Maximum Position**: 1550 steps (safe margin below up limit switch)  
- **Usable Range**: 1500 steps for antenna tuning
- **Automatic Clamping**: Invalid positions are automatically constrained to safe limits
- **Debug Logging**: Clamping operations are logged for troubleshooting

### Position Flow
1. ESP-NOW receives position from StepperController
2. Raw position shown in RX message box (may be outside safe range)
3. Position clamped to 50-1550 range using `clamp_position()`
4. Clamped position shown in blue position text box
5. Safe position sent to stepper motor

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

- **`JC4827W543C`**: Basic hardware configuration
- **`StepperGUI`**: Production build with ESP-NOW communication
- **`StepperGUI_debug`**: Debug build with enhanced logging and position tracking

## Configuration

Key configuration constants in `main.cpp`:
```cpp
#define MIN_STEPPER_POSITION 50    // Safe margin above down limit
#define MAX_STEPPER_POSITION 1550  // Safe margin below up limit
uint8_t controllerMAC[] = {0xEC, 0xE3, 0x34, 0xC0, 0x33, 0xC0};
```

Touch calibration and display pins are configured for JC4827W543C hardware.

## Recent Updates

- Implemented position limiting system for hardware safety
- Added comprehensive ESP-NOW debugging with message boxes
- Replaced confusing position sliders with clear text displays
- Enhanced logging for position synchronization troubleshooting
- Optimized UI layout for 272px portrait orientation
- Fixed LVGL font configuration issues

## development environment
* Visual Code 1.103.2
* PlatformIO 6.1.18
