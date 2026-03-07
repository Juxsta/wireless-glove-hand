---
date: 2026-02-05
author: Eric Reyes
status: Updated (Midterm Implementation)
---

# Architecture Decision Document
## Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**Version:** 1.1  
**Author:** Eric Reyes  
**Date:** February 5, 2026 (Updated March 2026)  
**Status:** ✅ Reflects Current Implementation

---

## 1. System Context

### 1.1 PoC Scope (Single Finger Demo)

For proof of concept, we're building a **single finger** (index finger) with joints to validate the core FOC approach before scaling to full hand.

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CONTROL GLOVE (PoC: 1 Finger)                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │  MLX90395   │───▶│   ESP32     │───▶│  ESP-NOW    │─────────┐   │
│  │  Magnetic   │    │ (SPI+Radio) │    │  Transmit   │         │   │
│  │  Sensors    │    │             │    │             │         │   │
│  │  (3/finger) │    │             │    │             │         │   │
│  └─────────────┘    └─────────────┘    └─────────────┘         │   │
└─────────────────────────────────────────────────────────────────│───┘
                                                                  │
                              ESP-NOW                             │
                            (~50Hz, <10ms)                        │
                                                                  │
┌─────────────────────────────────────────────────────────────────│───┐
│                    ROBOTIC HAND (PoC: 1 Finger)                 │   │
│                                                                 ▼   │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │  ESP-NOW    │───▶│   ESP32     │───▶│ FOC Control │             │
│  │  Receive    │    │   (Main)    │    │   Loop      │             │
│  └─────────────┘    └─────────────┘    └──────┬──────┘             │
│                                               │                     │
│     ┌─────────────┬─────────────┬─────────────┼─────────────┐      │
│     ▼             ▼             ▼             ▼             │      │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐           │      │
│  │DRV8300│    │DRV8300│    │DRV8300│    │DRV8300│           │      │
│  │Custom│     │Custom│     │Custom│     │Custom│           │      │
│  │ PCB  │     │ PCB  │     │ PCB  │     │ PCB  │           │      │
│  └──┬───┘     └──┬───┘     └──┬───┘     └──┬───┘           │      │
│     │            │            │            │               │      │
│  ┌──┴───┐     ┌──┴───┐     ┌──┴───┐     ┌──┴───┐          │      │
│  │ 2204 │     │ 2204 │     │ 2204 │     │ 2204 │          │      │
│  │ BLDC │     │ BLDC │     │ BLDC │     │ BLDC │          │      │
│  │ MCP  │     │ PIP  │     │ DIP  │     │ TIP  │          │      │
│  └──────┘     └──────┘     └──────┘     └──────┘          │      │
│                                                                     │
│  ┌─────────────┐                                                   │
│  │   Battery   │ 2S LiPo (7.4V) for PoC                           │
│  └─────────────┘                                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Data Flow

```
MLX90395 (SPI) → Raw Magnetometer (X,Y,Z) → atan2(Y,X) → Angle (°)
    │                                                        │
    │ 50Hz SPI burst read                                    │
    │                                                        │
    ▼                                                        ▼
Calibration (baseline subtraction) ──────────▶ ESP-NOW Packet (10 bytes)
                                                             │
                                                             │ ESP-NOW broadcast
                                                             ▼
                                                    ESP-NOW Receive → Unpack
                                                             │
                                                             │ 1kHz control loop
                                                             ▼
                                                    Position Command → FOC
                                                             │
                                                             ▼
                                                    Motor Position (AS5600)
```

---

## 2. Architectural Decisions (Implementation Status)

### ADR-001: MCU Platform

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ IMPLEMENTED |
| **Decision** | **ESP32-WROOM-32** (DevKit v1) |
| **Rationale** | Dual-core (FOC + wireless), built-in ESP-NOW support, Arduino/PlatformIO support, $4 cost |
| **Quantity** | 2 (one glove, one hand) |

---

### ADR-002: FOC Library

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ IMPLEMENTED |
| **Decision** | **SimpleFOC v2.3.x** |
| **Rationale** | Arduino-compatible, extensive documentation, active community, proven on ESP32 |
| **Control Mode** | Position control (angle mode) — no torque sensing needed for PoC |
| **Loop Rate** | Target 1kHz per motor, may reduce to 500Hz for 4 motors |

**Implementation Notes:**
- Use `motor.controller = MotionControlType::angle`
- PID tuning per motor: start with P=20, I=0, D=0.5
- Sensor: Magnetic encoder (AS5600) via I2C

---

### ADR-003: Motor Driver

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ CUSTOM PCB DESIGNED (Raul) |
| **Decision** | **DRV8300 Custom PCB** |
| **Components** | DRV8300 gate driver + BSZ063N04LS6 MOSFETs + ACS37042 current sensing |
| **Cost** | ~$11.77/board (OSHPark fabrication + DigiKey components) |
| **Quantity** | 4 for PoC (1 per joint) |
| **Specs** | 3-phase BLDC control, inline current monitoring |

**Rationale:**
- Cost-effective custom solution vs commercial modules ($25+ each)
- Integrated current sensing (ACS37042) for future torque control
- Designed by Raul — optimized for our motor specs
- OSHPark fabrication ensures quality

**Wiring:**
- Gate driver outputs → MOSFET half-bridges → motor phases
- PWM inputs from ESP32 (6 pins per driver, 24 total for 4 motors)
- ENABLE → ESP32 GPIO
- Current sense outputs → ESP32 ADC (future feature)

**Pin Allocation (4 motors):**
| Motor | PWM A | PWM B | PWM C | Enable | Current Sense |
|-------|-------|-------|-------|--------|---------------|
| MCP   | 25    | 26    | 27    | 14     | ADC1_CH0 (future) |
| PIP   | 16    | 17    | 18    | 15     | ADC1_CH3 (future) |
| DIP   | 19    | 21    | 22    | 23     | ADC1_CH4 (future) |
| TIP   | 32    | 33    | 13    | 12     | ADC1_CH5 (future) |

**Note:** Pin mappings subject to change based on final PCB layout. Firmware contains TODO comments for DRV8300 PCB integration.

---

### ADR-004: BLDC Motor Selection

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ MOTORS SOURCED (Matthew) |
| **Decision** | **2204 BLDC Motor** |
| **Specs** | 22mm diameter, Kv TBD (needs verification from Matthew) |
| **Torque** | ~0.08 Nm (estimated) |
| **Quantity** | 5 (4 + 1 spare for testing) |
| **Supplier** | Sourced by Matthew |

**Pole Pair Count:** TBD — **NEEDS VERIFICATION** from motor datasheet.
- Typical 2204 motors: 7-14 pole pairs
- Must update `BLDCMotor(POLE_PAIRS)` in firmware once confirmed
- Current firmware uses `#define MOTOR_POLE_PAIRS 7  // TODO: Verify for 2204 motors`

**Rationale:** 
- Appropriate size for finger joints
- Proven torque for hand actuation
- Sourced by Matthew — confirmed availability

**Previous Version (Wrong):** Architecture originally specified 2804 motors. Updated to reflect actual sourced components.

---

### ADR-005: Encoder Type

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ IMPLEMENTED |
| **Decision** | **AS5600 Magnetic Encoder** |
| **Interface** | I2C (with TCA9548A multiplexer for 4 encoders) |
| **Resolution** | 12-bit (4096 positions/rev) — sufficient for ±1° accuracy |
| **Price** | ~$2-3/ea |
| **Quantity** | 5 (4 + 1 spare) |

**I2C Architecture:**
```
ESP32 I2C (GPIO 21 SDA, GPIO 22 SCL)
    │
    └── TCA9548A Multiplexer (addr 0x70)
            ├── Channel 0 → AS5600 (MCP joint)
            ├── Channel 1 → AS5600 (PIP joint)
            ├── Channel 2 → AS5600 (DIP joint)
            └── Channel 3 → AS5600 (TIP joint)
```

**Magnet:** 6x3mm diametric magnet mounted to motor shaft.

---

### ADR-006: Sensor Configuration

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ IMPLEMENTED (Antonio's code) |
| **Decision** | **MLX90395 Hall-effect Magnetic Sensors** |
| **Interface** | SPI bus (SCK=18, MISO=19, MOSI=23) |
| **Quantity** | 3 per finger (MCP, PIP, DIP) for PoC |
| **Resolution** | 16-bit per axis (X, Y, Z) |
| **Backup** | Flex sensors available as fallback |

**Rationale:**
- More accurate and reliable than resistive flex sensors
- SPI interface allows high-speed polling (>100Hz)
- Antonio implemented working driver code
- Magnetic angle calculation via atan2(Y, X)

**Circuit:**
```
ESP32 SPI Bus (VSPI):
    SCK  = GPIO 18
    MISO = GPIO 19
    MOSI = GPIO 23
    
Chip Selects (one per sensor):
    CS0 = GPIO 5  (MCP sensor)
    CS1 = GPIO 17 (PIP sensor)
    CS2 = GPIO 16 (DIP sensor)
```

**MLX90395 Commands:**
- `START_BURST = 0x1E` — Begin continuous measurement mode
- `READ_MEAS = 0x4E` — Read X, Y, Z magnetometer data
- `EXIT = 0x80` — Exit burst mode

**Calibration:**
- 20-sample baseline measurement (hand flat)
- Baseline subtraction per sensor
- Stored in EEPROM/Preferences for power cycles

**Previous Version (Wrong):** Architecture originally specified flex sensors. Updated to reflect MLX90395 implementation.

---

### ADR-007: Wireless Protocol

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ IMPLEMENTED (Antonio's code) |
| **Current Implementation** | **ESP-NOW** |
| **Future Discussion** | BLE vs ESP-NOW under team evaluation |

**ESP-NOW Specifications:**
- **Latency:** <10ms typical (vs 50-100ms for BLE)
- **Range:** ~200m line-of-sight (vs ~10m for BLE in practice)
- **Pairing:** MAC address based (configured at compile time)
- **Rate:** ~50Hz packet broadcast
- **MTU:** 250 bytes (we use 10 bytes)

**Packet Format (10 bytes):**
```
Byte 0:    Sequence number (0-255, wraps)
Byte 1:    MCP angle high byte
Byte 2:    MCP angle low byte  (16-bit int16_t in degrees * 10)
Byte 3:    PIP angle high byte
Byte 4:    PIP angle low byte
Byte 5:    DIP angle high byte
Byte 6:    DIP angle low byte
Byte 7-8:  Reserved (TIP angle, future)
Byte 9:    Checksum (XOR of bytes 0-8)
```

**Receiver MAC Address:**
- Configured in glove firmware: `#define RECEIVER_MAC {0xXX, 0xXX, ...}`
- ESP-NOW broadcasts to specific peer (not broadcast MAC)

**Implementation Notes:**
- Antonio's working code uses ESP-NOW
- Team discussing BLE for future iterations (lower power, phone integration)
- Firmware architecture allows protocol swap without major changes

**Previous Version (Wrong):** Architecture originally specified BLE. Updated to reflect ESP-NOW implementation.

---

### ADR-008: Power Architecture (PoC)

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ IMPLEMENTED |

**Glove (Transmitter):**
- USB power from laptop for PoC development
- Future: 1S LiPo (3.7V 500mAh)

**Hand (Receiver + Motors):**
- **2S LiPo (7.4V 1000mAh)** for PoC
- Step-down to 5V for ESP32 (use AMS1117-5.0 or buck converter)
- Motors run directly from 7.4V (DRV8300 supports wide input range)
- Estimated runtime: ~30-45 minutes under light load

**Why 2S instead of 3S:**
- Cheaper battery
- Lower risk of motor overheating
- Sufficient for demo torque
- Upgrade to 3S for full hand if needed

---

## 3. PoC Bill of Materials (Updated)

| Component | Qty | Unit Cost | Total | Source | Status |
|-----------|-----|-----------|-------|--------|--------|
| ESP32 DevKit v1 | 2 | $4 | $8 | Amazon | ✅ |
| DRV8300 Custom PCB | 4 | $11.77 | $47 | OSHPark/DigiKey | 🔄 Ordered |
| 2204 BLDC Motor | 5 | $8-12 | $50 | (Matthew) | ✅ Sourced |
| AS5600 Encoder | 5 | $3 | $15 | Amazon | ✅ |
| TCA9548A I2C Mux | 1 | $3 | $3 | Amazon | ✅ |
| MLX90395 Sensor | 4 | $5-8 | $28 | DigiKey/Adafruit | ✅ |
| 6x3mm Diametric Magnet | 10 | $0.50 | $5 | Amazon | ✅ |
| 2S LiPo 1000mAh | 1 | $15 | $15 | Amazon | ✅ |
| AMS1117-5.0 Regulator | 2 | $1 | $2 | Amazon | ✅ |
| Prototype PCB/Wires | - | - | $15 | - | - |
| 3D Print Filament | 0.5kg | $12 | $12 | Available | - |
| **TOTAL** | | | **~$200** | | |

**Budget Status:** ✅ Within $200 target for PoC

---

## 4. Risk Analysis (Updated)

| Risk | Prob | Impact | Status | Mitigation |
|------|------|--------|--------|------------|
| Motor pole pairs unknown | High | Med | ⚠️ **ACTION NEEDED** | Matthew to verify from datasheet/supplier |
| DRV8300 PCB fabrication delay | Med | Med | 🔄 Monitoring | Ordered from OSHPark (2-week lead time) |
| MLX90395 SPI noise/interference | Low | Med | ✅ Addressed | Antonio's code includes filtering |
| ESP-NOW range in practice | Low | Low | ✅ Tested | >2m confirmed in lab |
| 3D print tolerances | Med | Low | 🔄 Iterating | Antonio testing fits |
| Motor lead time | Low | High | ✅ Resolved | Matthew sourced motors |

---

## 5. Firmware Directory Structure

```
wireless-glove-hand/
├── firmware/
│   ├── glove/
│   │   ├── platformio.ini
│   │   └── src/
│   │       ├── main.cpp          # ESP-NOW transmit + MLX90395 read
│   │       └── mlx90395.h        # MLX90395 SPI driver (Antonio's code)
│   └── hand/
│       ├── platformio.ini
│       └── src/
│           ├── main.cpp          # ESP-NOW receive + SimpleFOC
│           ├── foc_controller.h  # SimpleFOC wrapper
│           └── encoder_mux.h     # AS5600 + TCA9548A I2C mux
```

---

## 6. Next Steps

### Immediate Actions
- [ ] **Matthew:** Confirm 2204 motor pole pair count from datasheet
- [ ] **Raul:** Confirm DRV8300 PCB pin mappings for firmware
- [ ] **Antonio:** Extract MLX90395 driver into header file
- [ ] **Eric:** Update firmware with DRV8300 pin mappings (once confirmed)

### Integration Testing
- [ ] Single motor + SimpleFOC + AS5600 + DRV8300 PCB test
- [ ] Full 4-motor ESP-NOW control test
- [ ] Latency measurement (ESP-NOW vs BLE comparison)

### Mechanical
- [ ] CAD updates for 2204 motor mounts
- [ ] Print and test single finger assembly
- [ ] Verify encoder magnet alignment

---

## Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Firmware | Eric Reyes | 2026-03-06 | ✅ |
| Hardware Lead | Antonio Rojas | | |
| Motor Driver PCB | Raul Hernandez-Solis | | |
| Wireless/Motors | Matthew Men | | |
| Advisor | Dr. Birsen Sirkeci | | |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-05 | Eric Reyes | Initial architecture draft |
| 1.0 | 2026-02-05 | Eric Reyes | Finalized all decisions for PoC, added BOM |
| 1.1 | 2026-03-06 | Eric Reyes | **Updated to reflect actual implementation:** MLX90395 sensors, ESP-NOW, DRV8300 PCB, 2204 motors |
