# Technical Specification
## Induction Furnace Cooling Controller

### Document Information
| Property | Value |
|----------|-------|
| Document ID | IAS-TS-2024-001 |
| Version | 1.0 |
| Date | 2024-01-XX |
| Status | Released |
| Author | Industrial Automation Systems |

---

## 1. System Overview

### 1.1 Purpose
The Induction Furnace Cooling Controller is an industrial control system designed to monitor cooling water flow in induction melting furnaces. It provides critical protection against cooling failure, preventing damage to expensive copper inductors (repair cost: $10,000+ per inductor, 1+ month downtime).

### 1.2 Scope
- 6 cooling circuit monitoring
- Real-time flow rate measurement (L/min)
- Fail-safe alarm and automatic shutdown
- User interface via LCD1602 and push buttons
- Industrial relay outputs (12V)

### 1.3 System Features
| Feature | Description |
|---------|-------------|
| Flow Monitoring | 6 channels, Hall effect sensors, 1s timeout |
| Display | LCD1602 with I2C backpack |
| Control | Start/Stop/Reset buttons |
| Outputs | Main contactor, Ready, Beacon, Power lamp |
| Safety | Watchdog timer, alarm latching, fail-safe |

---

## 2. Technical Specifications

### 2.1 Hardware Specifications

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| Processor | ESP32-C6 (RISC-V) | 160 MHz |
| Flash Memory | 4 MB | SPI flash |
| RAM | 512 KB | SRAM |
| Operating Voltage | 3.3V (Logic) / 5V (I2C) | Level shifter required |
| Operating Temperature | -20°C to +70°C | Industrial range |
| Storage Temperature | -40°C to +85°C | |
| Humidity | 5% to 95% RH | Non-condensing |
| Protection Rating | IP20 | Controller enclosure |

### 2.2 Power Requirements

| Parameter | Specification |
|-----------|---------------|
| Supply Voltage | 5V DC (logic) / 12V DC (relays) |
| Current Consumption | 500mA typical, 1A max |
| Power Consumption | 2.5W typical, 5W max |
| Power Supply Type | Regulated DC |

### 2.3 Inputs

| Input | Type | Quantity | Details |
|-------|------|----------|---------|
| Flow Sensors | Digital (Hall) | 6 | GPIO 10,11,20,21,22,23 |
| Start Button | Digital (LOW active) | 1 | PCF8575 pin 0 |
| Stop Button | Digital (LOW active) | 1 | PCF8575 pin 1 |
| Reset Button | Digital (LOW active) | 1 | PCF8575 pin 2 |

### 2.4 Outputs

| Output | Type | Quantity | Details |
|--------|------|----------|---------|
| Main Contactor | Relay (LOW active) | 1 | PCF8575 pin 0, 12V |
| Ready Indicator | Relay (LOW active) | 1 | PCF8575 pin 1, 12V |
| Beacon | Relay (LOW active) | 1 | PCF8575 pin 2, 12V |
| Power Lamp | Relay (LOW active) | 1 | PCF8575 pin 3, 12V |

### 2.5 Communication

| Interface | Details | Use |
|-----------|---------|-----|
| I2C | 50 kHz, 7-bit address | Peripherals |
| UART | 115200 baud, 8N1 | Debug logging |
| GPIO | Interrupt capable | Flow sensors |

---

## 3. Functional Specifications

### 3.1 Flow Monitoring

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sensor Type | Hall effect | ZJ-B1 (1/2") |
| Calibration | 450 pulses/L | For ZJ-B1 |
| Measurement Range | 0-30 L/min | Sensor dependent |
| Update Rate | 1 second | Calculation interval |
| Timeout | 1 second | Flow loss detection |
| Display Format | X.X L/min | 1 decimal place |

### 3.2 State Machine

| State | Description | Entry Conditions | Exit Conditions |
|-------|-------------|------------------|-----------------|
| READY | System ready | Power on, Reset | Press START |
| RUNNING | Normal operation | All flows OK, START | STOP or ALARM |
| ALARM | Flow loss detected | Any flow lost | Press RESET |
| ERROR | Fatal error | Watchdog, hardware fail | System reset |

### 3.3 Display Format

#### READY State (All OK)
System ready.
Press start.

text

#### READY State (Flow Error)
! System NOT ready !
ERR: 3,5

text

#### RUNNING State (Normal)
1:1.5 2:2.3 3:1.8
4:2.1 5:1.9 6:2.5

text

#### RUNNING State (Flow Lost)
1:1.5 2:!!! 3:1.8
4:2.1 5:1.9 6:2.5

text

#### ALARM State
!! ALARM !!
ERR: 2

text

### 3.4 Relay Control Logic

| Relay | READY State | RUNNING State | ALARM State |
|-------|-------------|---------------|-------------|
| Main Contactor | OFF | ON | OFF |
| Ready Indicator | ON | OFF | ON |
| Beacon | OFF | ON | ON |
| Power Lamp | ON | ON | ON |

### 3.5 Safety Functions

| Function | Description | Response Time |
|----------|-------------|---------------|
| Flow Loss Detection | Monitors all 6 channels | <1s |
| Main Contactor Shutdown | Removes power to furnace | <100ms |
| Alarm Indication | Beacon + LCD display | <100ms |
| Startup Prevention | No start without all flows | Real-time |
| Watchdog Reset | System reset on hang | 10s timeout |
| Alarm Latching | Requires manual reset | Until RESET pressed |

---

## 4. Pin Configuration

### 4.1 ESP32-C6 Pinout

| GPIO | Function | Description | Direction |
|------|----------|-------------|-----------|
| 6 | I2C SDA | I2C Data line | Bidirectional |
| 7 | I2C SCL | I2C Clock line | Output |
| 10 | FLOW_1 | Channel 1 flow sensor | Input |
| 11 | FLOW_2 | Channel 2 flow sensor | Input |
| 20 | FLOW_3 | Channel 3 flow sensor | Input |
| 21 | FLOW_4 | Channel 4 flow sensor | Input |
| 22 | FLOW_5 | Channel 5 flow sensor | Input |
| 23 | FLOW_6 | Channel 6 flow sensor | Input |

### 4.2 I2C Device Addresses

| Device | Address | Protocol | Description |
|--------|---------|----------|-------------|
| LCD | 0x27 | 8-bit | PCF8574 I2C backpack |
| Relays | 0x23 | 16-bit | PCF8575 I/O expander |
| Buttons | 0x25 | 16-bit | PCF8575 I/O expander |

### 4.3 PCF8574 LCD Pin Mapping

| PCF8574 Pin | LCD Connection | Function |
|-------------|----------------|----------|
| P0 | RS | Register Select |
| P1 | RW | Read/Write (GND) |
| P2 | E | Enable |
| P3 | Backlight | Backlight control |
| P4 | D4 | Data bit 4 |
| P5 | D5 | Data bit 5 |
| P6 | D6 | Data bit 6 |
| P7 | D7 | Data bit 7 |

### 4.4 PCF8575 Relay Pin Mapping

| Pin | Relay | Function | Active State |
|-----|-------|----------|--------------|
| P00 | Relay 0 | Main Contactor | LOW |
| P01 | Relay 1 | Ready Indicator | LOW |
| P02 | Relay 2 | Beacon | LOW |
| P03 | Relay 3 | Power Lamp | LOW |
| P04-P15 | Reserved | Future expansion | - |

### 4.5 PCF8575 Button Pin Mapping

| Pin | Button | Function | Active State |
|-----|--------|----------|--------------|
| P00 | Button 0 | Start | LOW |
| P01 | Button 1 | Stop | LOW |
| P02 | Button 2 | Reset | LOW |
| P03-P15 | Reserved | Future expansion | - |

---

## 5. Software Specifications

### 5.1 Framework and Tools

| Component | Specification | Version |
|-----------|---------------|---------|
| Platform | PlatformIO | Latest |
| Framework | ESP-IDF | v5.1+ |
| Language | C++ | C++17 |
| RTOS | FreeRTOS | Included |
| Build System | CMake | 3.16+ |

### 5.2 Tasks and Priorities

| Task | Priority | Stack Size | Description |
|------|----------|------------|-------------|
| Control | 5 (highest) | 8192 bytes | State machine, relay control |
| Buttons | 4 | 4096 bytes | Button polling, debounce |
| Flow | 3 | 4096 bytes | Sensor monitoring |
| Display | 2 | 4096 bytes | LCD update |
| Watchdog | 1 (lowest) | 2048 bytes | System monitoring |

### 5.3 Event System

| Event | Trigger | Action |
|-------|---------|--------|
| BUTTON_START | Start button pressed | Start system |
| BUTTON_STOP | Stop button pressed | Stop system |
| BUTTON_RESET | Reset button pressed | Reset alarm |
| FLOW_ALARM | Flow lost on any channel | Trigger alarm |
| UPDATE_DISPLAY | State changed | Update LCD |

### 5.4 Memory Usage

| Section | Size | Notes |
|---------|------|-------|
| Code (Flash) | ~300KB | With optimizations |
| Data (RAM) | ~50KB | Static + heap |
| Stack | ~20KB | All tasks combined |
| Total | ~370KB | Within 4MB limit |

---

## 6. Environmental Specifications

| Parameter | Specification |
|-----------|---------------|
| Operating Temperature | -20°C to +70°C |
| Storage Temperature | -40°C to +85°C |
| Humidity | 5% to 95% RH (non-condensing) |
| Vibration | 10-500 Hz, 2g |
| Shock | 15g, 11ms |
| Protection Rating | IP20 (controller) |
| Pollution Degree | 2 |
| Altitude | Up to 2000m |

---

## 7. EMC Specifications (Designed For)

| Test | Standard | Level |
|------|----------|-------|
| ESD | IEC 61000-4-2 | ±6kV contact, ±8kV air |
| RF | IEC 61000-4-3 | 10V/m, 80MHz-1GHz |
| Burst | IEC 61000-4-4 | ±2kV power, ±1kV signal |
| Surge | IEC 61000-4-5 | ±1kV line-to-line |
| Conducted RF | IEC 61000-4-6 | 10Vrms, 150kHz-80MHz |
| Magnetic Field | IEC 61000-4-8 | 30A/m, 50Hz |

---

## 8. Safety Certifications (Planned)

| Standard | Description | Status |
|----------|-------------|--------|
| CE Mark | European compliance | Planned |
| UKCA | UK compliance | Planned |
| IEC 61000-6-2 | EMC Immunity | Designed for |
| IEC 61000-6-4 | EMC Emission | Designed for |
| IEC 61508 | Functional Safety | SIL 1 ready |
| EN 60204-1 | Machinery Safety | Designed for |

---

## 9. Calibration

### 9.1 Flow Sensor Calibration

```cpp
#define FLOW_PULSES_PER_LITER 450.0f  // Default for ZJ-B1
9.2 Calibration Procedure
Step	Action	Description
1	Prepare	Measure exactly 1 liter of water
2	Count	Count pulses from sensor during flow
3	Calculate	Update FLOW_PULSES_PER_LITER
4	Verify	Rebuild and upload firmware
5	Validate	Test with known flow rate
9.3 Calibration Frequency
Component	Frequency
Flow sensors	Annually
System validation	After hardware changes
Factory reset	As needed
10. Testing
10.1 Factory Tests
Test	Description	Pass Criteria
Power On	System boot	Display shows "System ready."
I2C Test	Device communication	All devices detected
Flow Test	All channels	OK status on all channels
Relay Test	Relay switching	Correct state change
Button Test	All buttons	Correct response
10.2 Functional Tests
Test	Procedure	Expected Result
Start System	All flows OK → START	System runs
Stop System	RUNNING → STOP	System stops
Flow Alarm	Stop any flow	Alarm within 1s
Reset Alarm	ALARM → RESET	System ready
10.3 Safety Tests
Test	Procedure	Expected Result
Power Loss	Remove power	System safe state
Sensor Failure	Disconnect sensor	Alarm, stop
Watchdog	Inject hang	System reset
11. Maintenance
11.1 Preventive Maintenance Schedule
Frequency	Task
Daily	Visual inspection, check display status
Weekly	Verify all flow sensors show OK
Monthly	Test relay contacts, inspect wiring
Quarterly	Sensor calibration check
Annually	Full system validation, recalibration
11.2 Troubleshooting Guide
Problem	Likely Cause	Solution
LCD garbage	I2C speed too high	Reduce to 10kHz
No backlight	Power issue	Check 5V supply
No flow detection	Missing pull-up	Add 4.7kΩ pull-up
Relay chattering	Too frequent updates	Increase HOLD time
False alarms	Sensor calibration	Check pulses/L
System won't start	Flow error	Check all sensors
12. Version History
Version	Date	Changes	Author
1.0	2024-01-XX	Initial release	IAS
13. Appendices
Appendix A: Wiring Diagram Reference
Component	Connection	Notes
ESP32-C6	5V/GND	Via USB or regulator
LCD1602	I2C (pins 6,7)	5V power
Relays	I2C (pins 6,7)	12V external power
Buttons	I2C (pins 6,7)	Pull-up to 3.3V
Flow Sensors	GPIO 10,11,20,21,22,23	5V power
Appendix B: Bill of Materials (BOM)
Component	Part Number	Quantity	Supplier
ESP32-C6	DevKitM-1	1	Espressif
LCD1602	with PCF8574	1	Various
Relay Module	4-ch with PCF8575	1	Various
Button Module	with PCF8575	1	Various
Flow Sensor	ZJ-B1 (1/2")	6	Various
Level Shifter	SCU7810	1	Various
Appendix C: Revision Control
Section	Revision	Date	Changes
All	1.0	2024-01-XX	Initial release
Document Owner: Industrial Automation Systems
Review Frequency: Annually
Next Review: 2025-01-XX