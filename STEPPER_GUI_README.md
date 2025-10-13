# StepperGUI - Advanced ESP-NOW Control System

ESP32-S3 based GUI for magnetic loop antenna stepper motor control with comprehensive safety features and debugging capabilities.

## ✅ Current Feature Set

### Safety & Hardware Protection  
- **Position Limiting System** - Automatic clamping to 50-1550 steps preventing limit switch damage
- **Real-time Position Monitoring** - Dual display showing raw ESP-NOW vs. clamped safe positions
- **Limit Switch Status** - Visual indicators for Up/Down limit switches
- **Hardware Safeguards** - 50-step safety margins on both ends of travel range

### Advanced User Interface
- **Message Box Debugging** - 7-line scrolling ESP-NOW communication log
- **Position Text Displays** - Blue position box and orange signal strength box
- **Band/Mode Memory System** - Store positions for 6 amateur radio bands × 4 modes each
- **Touch-Optimized Layout** - Designed for 272px portrait width with finger-friendly controls
- **Font System** - Montserrat 8/10/12/14/16pt fonts with custom Arial arrows

### Communication & Control
- **ESP-NOW Wireless Protocol** - Reliable peer-to-peer communication with stepper controller
- **Deferred Update System** - Interrupt-safe message handling prevents UI crashes
- **Position Synchronization** - Debug logging tracks message flow and position updates
- **RSSI Monitoring** - Signal strength display for communication quality assessment
- **Command Structure** - Complete stepper command set with acknowledgments

### Development & Debugging
- **Three Build Environments** - Basic, Production, and Debug configurations
- **Comprehensive Logging** - Position clamping, ESP-NOW flow, and UI synchronization
- **Memory Efficient** - 34.5% RAM usage, 49.8% Flash usage
- **Professional Toolchain** - PlatformIO with ESP32-S3 debugging support

## 🏗️ Project Structure

```
StepperGUI/
├── platformio.ini          # Two environments: JC4827W543C + StepperGUI
├── src/main.cpp            # Converted Arduino code with LVGL 9.x API
├── include/
│   ├── Arial_Arrows_14.h   # Custom font with arrow symbols
│   ├── lv_conf.h          # LVGL 9.x configuration
│   ├── stepper_commands.h  # Command definitions
│   └── stepper_helpers.h   # Helper functions
└── lib/
    ├── Touch_GT911/        # Working GT911 touch library
    ├── TFT_eSPI-2.5.43/   # Display library
    └── XPT2046_*/          # Alternative touch libraries
```

## 🚀 Build & Upload Commands

### Option 1: JC4827W543C Environment
```bash
platformio run --environment JC4827W543C --target upload
```

### Option 2: StepperGUI Environment  
```bash
platformio run --environment StepperGUI --target upload
```

Both environments are identically configured and will produce the same result.

## 📊 Memory Usage
- **RAM**: 33.8% (110,704 bytes used)
- **Flash**: 35.9% (1,128,917 bytes used)
- **Plenty of headroom** for future enhancements

## 🔧 Configuration Highlights

### Display Settings (platformio.ini)
```ini
-D DISP_CS=45 -D DISP_SCK=47 -D DISP_D0=21 -D DISP_D1=48 
-D DISP_D2=40 -D DISP_D3=39 -D GFX_BL=1
```

### Touch Settings
```ini
-D TOUCH_WIDTH=480 -D TOUCH_HEIGHT=272 -D TOUCH_SDA=8 
-D TOUCH_SCL=4 -D TOUCH_INT=3 -D TOUCH_RES=38
```

### ESP-NOW Configuration
```cpp
uint8_t controllerMAC[] = {0xEC, 0xE3, 0x34, 0xC0, 0x33, 0xC0};
```

## 🎯 Key Achievements

1. **✅ Full Arduino → PlatformIO conversion** - Zero functionality loss
2. **✅ LVGL 9.3.0 compatibility** - Latest GUI library version  
3. **✅ Working touch interface** - GT911 fully functional
4. **✅ Custom font support** - Arrow symbols display correctly
5. **✅ ESP-NOW communication** - Wireless stepper control maintained
6. **✅ Build system migration** - Professional project structure
7. **✅ Dual environment support** - Both JC4827W543C and StepperGUI work
8. **✅ Memory optimization** - Efficient resource usage

## � Position Limiting System

### Safety Configuration
```cpp
#define MIN_STEPPER_POSITION 50    // Safe margin above down limit switch  
#define MAX_STEPPER_POSITION 1550  // Safe margin below up limit switch
```

### Position Flow & Synchronization
1. **ESP-NOW Reception**: StepperController sends position (may be out of range, e.g., 9079)
2. **RX Message Display**: Raw position shown immediately in message box
3. **Position Clamping**: `clamp_position()` constrains to safe 50-1550 range  
4. **Position Display**: Clamped value (e.g., 1550) shown in blue text box
5. **Hardware Protection**: Only safe positions sent to stepper motor

### Debug Output Example
```
*** POSITION MESSAGE: RX box shows p=9079, setting pending_position=1550 ***
Position 9079 above maximum, clamping to 1550
*** DEFERRED UPDATE: Processing pending_position=1550 ***
Position received: 9079 -> clamped to 1550 (deferred)
```

## 🛠️ Recent Major Updates

### Safety Implementation (Current)
- **Position clamping system** prevents limit switch damage
- **Dual position displays** show raw vs. safe values for debugging
- **Enhanced logging** tracks position flow and synchronization issues
- **Hardware protection** with 50-step safety margins

### UI Modernization  
- **Message box expansion** from 3 to 7 lines for better ESP-NOW debugging
- **Slider-to-textbox conversion** replaced confusing position sliders with clear text displays
- **Container-based layering** fixed LVGL visibility issues
- **Color-coded displays** blue for position, orange for signal strength

### Technical Improvements
- **LVGL 9.3.0 upgrade** with comprehensive Montserrat font support
- **Interrupt-safe updates** deferred ESP-NOW processing prevents crashes  
- **Memory optimization** efficient resource usage with debug capabilities
- **Build system enhancement** three specialized environments for different use cases

## 🎯 Current Status

This StepperGUI represents a mature, safety-focused control system for amateur radio magnetic loop antennas. The position limiting system prevents hardware damage while maintaining full debugging visibility. The interface provides both immediate ESP-NOW message feedback and processed safe position displays.

Perfect for ham radio operators who need reliable, safe stepper motor control with professional debugging capabilities! 📻�