# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2024-01-XX

### Added
- **Core System**
  - ESP32-C6 implementation with ESP-IDF framework
  - 6-channel flow monitoring with Hall sensors
  - 1-second timeout for critical flow loss detection
  
- **Display Interface**
  - LCD1602 with PCF8574 I2C backpack
  - Real-time flow rate display (L/min)
  - Channel status display (OK/!!!)
  - Standby error indication
  - Alarm state display

- **Control Interface**
  - Start/Stop/Reset button control
  - Relay outputs: Main contactor, Ready, Beacon, Power lamp
  - Active LOW relay control

- **Safety Features**
  - Watchdog timer with 10s timeout
  - Continuous flow monitoring in all states
  - Startup prevention on flow loss
  - Automatic shutdown on alarm
  - Latching alarm requiring manual reset

- **Diagnostics**
  - Real-time flow rate logging
  - Sensor level diagnostics
  - Alarm channel identification
  - Serial monitor output

### Technical
- ESP-IDF v5.1+ support
- I2C driver with 50kHz speed (level shifter compatible)
- FreeRTOS task management
- Event-driven architecture
- Atomic operations for ISR safety

### Documentation
- Complete README.md
- Technical specification
- User manual
- Certification documentation
- API documentation

### Known Issues
- None reported

## [Unreleased]
### Planned
- Network connectivity (Wi-Fi/Ethernet)
- MQTT integration for remote monitoring
- OTA firmware updates
- Data logging to SD card
- Web dashboard
- Mobile app support
- Cloud integration
- Additional safety certification