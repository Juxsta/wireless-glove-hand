# Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**EE198A/B Senior Project — San Jose State University**  
**Department of Electrical Engineering**  
**Spring 2026**

## Team

| Name | Student ID | Role |
|------|-----------|------|
| Antonio Rojas | 014974063 | Robotic hand hardware / 3D printing |
| Raul Hernandez-Solis | 016319693 | Control glove hardware |
| Matthew Men | 016601806 | Wireless communication / protocol |
| Eric Reyes | 011351067 | 3D printing / CAD / firmware / project lead |

**Advisor:** Junaid Anwar

## Abstract

This project implements a wearable glove system with flex sensors at each finger joint that wirelessly controls a 3D-printed robotic hand. The system uses Field Oriented Control (FOC) with brushless DC (BLDC) motors to achieve precise, smooth, real-time mimicry of human hand movements. Communication between the glove and hand is handled via Bluetooth Low Energy (BLE) on ESP32 microcontrollers.

The goal is to demonstrate that joint-level robotic hand control can be achieved affordably using consumer-grade components, providing a platform suitable for education, research prototyping, and telepresence applications.

## System Architecture

```
┌─────────────────────┐        BLE         ┌──────────────────────┐
│    Control Glove     │  ──────────────▶  │     Robotic Hand      │
│                      │    ~30 Hz data     │                      │
│  • Flex sensors (4/  │                    │  • BLDC motors (FOC)  │
│    finger joint)     │                    │  • DRV8302 drivers    │
│  • ESP32 (ADC + BLE) │                    │  • AS5600 encoders    │
│  • Calibration       │                    │  • ESP32 (control)    │
│    routines          │                    │  • 2S LiPo battery    │
└─────────────────────┘                    └──────────────────────┘
```

### Data Flow

```
Flex Sensor → ADC (12-bit) → Angle Mapping (0–90°) → BLE Packet (4 bytes)
                                                            │
                                                     BLE notification
                                                            │
                                                            ▼
                                               BLE Receive → Unpack
                                                            │
                                                     1 kHz FOC loop
                                                            │
                                                            ▼
                                               Motor Position (AS5600)
```

## Key Specifications

| Parameter | Target | Notes |
|-----------|--------|-------|
| End-to-end latency | < 100 ms | Sensor-to-motor response |
| Position accuracy | ± 5° | Compared to goniometer reference |
| Sensor sample rate | ≥ 50 Hz | Per-finger ADC polling |
| BLE update rate | ≥ 30 Hz | Characteristic notifications |
| FOC control loop | ≥ 1 kHz | Timer interrupt driven |
| Wireless range | > 2 m | Practical desktop use |
| Continuous runtime | > 30 min | Demo duration |
| BLE packet loss | < 1% | Sequence number tracking |

## Project Structure

```
wireless-glove-hand/
├── docs/                    # Project documentation
│   ├── proposal/            # EE198A proposal and Gantt chart
│   ├── hardware/            # Schematics, BOM, CAD exports
│   └── references.md        # Bibliography
├── firmware/                # ESP32 firmware (PlatformIO)
│   ├── glove/               # Control glove firmware
│   │   └── src/main.cpp
│   └── hand/                # Robotic hand firmware
│       └── src/main.cpp
├── hardware/                # KiCad schematics, 3D print STLs
└── tests/                   # Test scripts and results
```

## Hardware Components

### Control Glove
- **MCU:** ESP32-WROOM-32 (dual-core, WiFi/BLE)
- **Sensors:** Flex sensors (1 per joint, 4 per finger)
- **ADC:** 12-bit native ESP32 ADC with calibration
- **Power:** USB or LiPo

### Robotic Hand
- **MCU:** ESP32-WROOM-32
- **Motors:** BLDC motors (one per joint) with FOC control
- **Drivers:** DRV8302 motor driver boards
- **Encoders:** AS5600 magnetic rotary encoders
- **Structure:** 3D-printed (PLA/PETG)
- **Power:** 2S LiPo (7.4V)

## Development Timeline

| Phase | Dates | Deliverables |
|-------|-------|--------------|
| Research & Proposal | Aug – Dec 2025 | Literature review, EE198A proposal, oral presentation |
| Firmware Prototyping & FOC | Jan 1 – Feb 3, 2026 | Sensor reading, BLE pairing, basic FOC |
| Motor & Mechanical Integration | Feb 4 – Mar 18, 2026 | 3D-printed hand, motor mounting, driver wiring |
| System Integration & Testing | Mar 19 – Apr 30, 2026 | End-to-end testing, latency measurement, accuracy validation |
| Final Report & Presentation | May 1 – 15, 2026 | EE198B written report, oral presentation, demo |

## Getting Started

### Prerequisites
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- ESP-IDF (for advanced FOC tuning)
- 3D printer access (for hand structure)

### Building Firmware

```bash
# Glove firmware
cd firmware/glove
pio run --target upload

# Hand firmware
cd firmware/hand
pio run --target upload
```

### Calibration
1. Flash both ESP32s
2. Power on glove — run sensor calibration (flat hand → full fist)
3. Power on hand — run motor zero-position calibration
4. BLE pairing is automatic on boot

## Testing

See `tests/` for test scripts. Key validation tests:
- **Latency test:** High-speed camera sync between glove movement and motor response
- **Accuracy test:** Goniometer comparison at 0°, 30°, 60°, 90° per joint
- **Endurance test:** 30-minute continuous operation
- **Packet loss test:** BLE sequence number tracking

## References

See [`docs/references.md`](docs/references.md) for full bibliography.

## License

Academic project — San Jose State University, Department of Electrical Engineering, EE198A/B.
