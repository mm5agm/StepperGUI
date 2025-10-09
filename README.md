## StepperGUI Project

**Current Implementation**: ESP32-S3 based GUI controller for stepper motors using LVGL and ESP-NOW communication.

### Features
* ✅ Modern LVGL-based touch interface with custom controls
* ✅ ESP-NOW wireless communication with stepper controller  
* ✅ Multiple speed controls (Slow, Medium, Fast) for Up/Down movement
* ✅ Real-time slider controls with visual feedback
* ✅ Auto-save and reset functionality
* ✅ Debug configuration with USB CDC support for ESP32-S3
* ✅ Touch interface with custom arrow fonts

### Hardware Setup
* **Target**: JC4827W543C (ESP32-S3 with 4.3" capacitive touch display)
* **Display**: 480×272 TFT with GT911 touch controller
* **Communication**: ESP-NOW for wireless stepper motor control
* **Power**: USB-C programming and power

## device JC4827W543C

a.k.a. cheap-black-device (CBD) with capacitive touch

* display: 272 x 480
* performance: ~43 frames per second

## development environment
* Visual Code 1.103.2
* PlatformIO 6.1.18
