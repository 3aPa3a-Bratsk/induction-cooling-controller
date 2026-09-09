# Induction Furnace Cooling Controller

[![PlatformIO](https://img.shields.io/badge/platformio-ESP32C6-orange)](https://platformio.org/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1-blue)](https://www.espressif.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## 📖 Overview

Industrial cooling controller for induction melting furnaces. Monitors 6 cooling circuits with Hall effect flow sensors, provides real-time flow rate display, and ensures fail-safe operation to prevent damage to copper inductors.

**Critical protection system** for foundry applications where cooling failure can lead to:
- Copper inductor melting (repair cost: $10,000+)
- 1+ month downtime
- Significant production losses

## 🚀 Features

### Core Functionality
- **6-channel flow monitoring** with 1-second timeout
- **Real-time flow rate display** in L/min (with ZJ-B1 sensor calibration)
- **LCD1602 display** with I2C interface
- **Relay outputs**: Main contactor, Ready indication, Beacon, Power lamp
- **Button interface**: Start, Stop, Reset alarm
- **Watchdog timer** for system reliability

### Safety Features
- **1-second response time** to flow loss
- **Automatic main contactor shutdown** on alarm
- **Alarm latching** until manual reset
- **Start prevention** if any channel has no flow
- **Critical condition monitoring** in all states (including standby)
- **Visual alarm indication**: Beacon + LCD

### Industrial Grade
- ESP32-C6 RISC-V processor
- 4MB Flash memory
- Industrial I2C expanders (PCF8574/PCF8575)
- 6x GPIO inputs for Hall sensors
- 12V relay driver outputs

## 📊 Display Interface

### Standby Mode (All OK)
System ready.
Press start.

### Standby Mode (Flow Error)
! System NOT ready !
ERR: 3,5

### Running Mode (Normal)
1:1.5 2:2.3 3:1.8
4:2.1 5:1.9 6:2.5

### Running Mode (Flow Loss)
1:1.5 2:!!! 3:1.8
4:2.1 5:1.9 6:2.5

### Alarm State
!! ALARM !!
ERR: 2

## 🔧 Hardware Requirements

### Components
| Component          |       Specification      |        Quantity |
|--------------------|--------------------------|-----------------|
| MCU                | ESP32-C6 DevKitM-1       |               1 |
| Display            | LCD1602 with PCF8574 I2C |               1 |
| Relay Module       | 4x Relay with PCF8575    |               1 |
| Button Module      | 6x Button with PCF8575   |               1 |
| Flow Sensors       | ZJ-B1 (1/2") Hall effect |               6 |
| Level Shifter      | SCU7810 (3.3V ↔ 5V)      |               1 |
| Power Supply       | 5V / 12V DC              |               1 |

### Pin Configuration
|       Function | GPIO |    Description |
|----------------|------|----------------|
| I2C SDA        |  6   |  I2C Data line |
| I2C SCL        |  7   | I2C Clock line |
| Flow Sensor 1  | 10   |      Channel 1 |
| Flow Sensor 2  | 11   |      Channel 2 |
| Flow Sensor 3  | 20   |      Channel 3 |
| Flow Sensor 4  | 21   |      Channel 4 |
| Flow Sensor 5  | 22   |      Channel 5 |
| Flow Sensor 6  | 23   |      Channel 6 |

**I2C Addresses:**
- LCD: 0x27
- Relay Expander: 0x23
- Button Expander: 0x25

## 📦 Installation

### 1. Clone Repository
```bash
git clone https://github.com/YOUR_USERNAME/induction-cooling-controller.git
cd induction-cooling-controller
2. Install PlatformIO
bash
# Install PlatformIO Core
pip install platformio

# Or use VS Code extension
# Search for "PlatformIO IDE" in extensions
3. Build and Upload
bash
# Build firmware
pio run

# Upload to ESP32-C6
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
4. Production Build
bash
pio run -e production -t upload
🏗️ Project Structure
text
induction-cooling-controller/
├── include/
│   ├── config.h           # System configuration
│   ├── version.h           # Version information
│   ├── system_state.h      # State machine definitions
│   ├── lcd1602.h          # LCD interface
│   ├── relay.h            # Relay control
│   ├── buttons.h          # Button interface
│   ├── flow_sensors.h     # Flow sensor management
│   └── watchdog.h         # Watchdog timer
├── src/
│   ├── main.cpp           # Main application
│   ├── system_state.cpp   # State machine implementation
│   ├── lcd1602.cpp        # LCD driver
│   ├── relay.cpp          # Relay driver
│   ├── buttons.cpp        # Button driver
│   ├── flow_sensors.cpp   # Flow sensor driver
│   └── watchdog.cpp       # Watchdog implementation
├── docs/
│   ├── technical_specification.md
│   ├── user_manual.md
│   └── certification/
├── platformio.ini         # PlatformIO configuration
├── CMakeLists.txt         # Build configuration
└── README.md             # This file

🔬 Calibration

Flow Sensor Calibration
For ZJ-B1 sensors (1/2"):

cpp
#define FLOW_PULSES_PER_LITER 450.0f

To calibrate for your specific sensor:

Measure 1 liter of water
Count pulses from sensor
Update FLOW_PULSES_PER_LITER with counted value

I2C Timing
For level shifter compatibility, I2C speed is set to:

cpp
#define I2C_MASTER_FREQ_HZ 50000  // 50kHz

🛡️ Safety Features
Flow Loss Detection
Timeout: 1 second

Action: Immediate main contactor shutdown

Indication: Beacon ON, LCD alarm, Relay state change

System Startup
All flows must be present for system start

Continuous monitoring even in standby

Visual indication of faulty channels

Watchdog
Timeout: 10 seconds

Action: System reset

Monitoring: All critical tasks

📊 Logging

Monitor Output

📊 FLOW: 1:OK 2:OK 3:OK 4:OK 5:OK 6:OK
💧 RATE (L/min): 1:1.5 2:2.3 3:1.8 4:2.1 5:1.9 6:2.5
✅ SYSTEM STARTED
🚨 CRITICAL: ERR: 3,5
🔧 Troubleshooting

Problem	Cause	    Solution
LCD shows garbage	I2C speed too high	Reduce to 10kHz
No flow detection	Missing pull-up resistor	Add 4.7kΩ pull-up
Relay chattering	State updates too frequent	Check RELAY_HOLD_TIME_MS
False flow alarms	Sensor calibration	Check FLOW_PULSES_PER_LITER

📋 Certification Documentation

The system is designed with functional safety principles:

IEC 61508 compliant architecture
SIL 1 ready design
Fail-safe operation

Latching alarms require manual reset

🤝 Contributing

Fork the repository
Create feature branch (git checkout -b feature/AmazingFeature)
Commit changes (git commit -m 'Add AmazingFeature')
Push to branch (git push origin feature/AmazingFeature)
Open Pull Request

📄 License

MIT License - see LICENSE file for details

📞 Contact

Bratsk-Service Ltd.

Email: dr.ibolit2020@gmail.com
Website: www.bratsk-service.ru

⚠️ Disclaimer

This software is designed for industrial use. The user assumes full responsibility for:

Proper installation
Regular maintenance
Safety procedures
Compliance with local regulations.
