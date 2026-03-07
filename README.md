# Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**EE198A/B Senior Project — San Jose State University**  
**Department of Electrical Engineering**  
**Spring 2026**

## Team

| Name | Student ID | Role |
|------|-----------|------|
| Antonio Rojas | 014974063 | Robotic hand hardware / 3D printing |
| Raul Hernandez-Solis | 016319693 | Control glove hardware / motor driver PCB design |
| Matthew Men | 016601806 | Wireless communication / motor sourcing |
| Eric Reyes | 011351067 | 3D printing / CAD / firmware |

**Advisor:** Dr. Birsen Sirkeci

## Abstract

This project implements a wearable glove system with MLX90395 Hall-effect magnetic sensors at each finger joint that wirelessly controls a 3D-printed robotic hand. The system uses Field Oriented Control (FOC) with brushless DC (BLDC) motors driven by custom DRV8300-based motor drivers to achieve precise, smooth, real-time mimicry of human hand movements. Communication between the glove and hand is currently handled via ESP-NOW on ESP32 microcontrollers.

The goal is to demonstrate that joint-level robotic hand control can be achieved affordably using consumer-grade components, providing a platform suitable for education, research prototyping, and telepresence applications.

## System Architecture

```
┌─────────────────────┐     ESP-NOW      ┌──────────────────────┐
│    Control Glove     │  ──────────────▶ │     Robotic Hand     │
│                      │    ~50 Hz data   │                      │
│  • MLX90395 Hall-    │                  │  • 2204 BLDC motors  │
│    effect sensors    │                  │    (FOC control)     │
│    (SPI, 3/finger)   │                  │  • DRV8300 custom    │
│  • ESP32 (SPI +      │                  │    motor drivers     │
│    ESP-NOW)          │                  │  • AS5600 encoders   │
│  • Calibration       │                  │  • ESP32 (control)   │
│    routines          │                  │  • 2S LiPo battery   │
└─────────────────────┘                   └──────────────────────┘
```

### Data Flow

```
MLX90395 (SPI) → Angle Calculation (atan2) → ESP-NOW Packet (10 bytes)
                                                       │
                                                ESP-NOW broadcast
                                                  (~50Hz, <10ms)
                                                       │
                                                       ▼
                                          ESP-NOW Receive → Unpack
                                                       │
                                                  1 kHz FOC loop
                                                       │
                                                       ▼
                                          Motor Position (AS5600)
```

## Key Specifications

| Parameter | Target | Notes |
|-----------|--------|-------|
| End-to-end latency | < 50 ms | Sensor-to-motor response (ESP-NOW) |
| Position accuracy | ± 5° | Compared to goniometer reference |
| Sensor sample rate | ≥ 50 Hz | Per-finger SPI polling |
| ESP-NOW update rate | ≥ 50 Hz | Broadcast packets |
| FOC control loop | ≥ 1 kHz | Timer interrupt driven |
| Wireless range | > 2 m | Practical desktop use |
| Continuous runtime | > 30 min | Demo duration |
| Packet loss | < 1% | Sequence number tracking |

## Project Structure

```
wireless-glove-hand/
├── docs/                    # Project documentation
│   ├── proposal/            # EE198A proposal and Gantt chart
│   ├── design/              # Architecture and ADRs
│   ├── hardware/            # Schematics, BOM, CAD exports
│   └── references.md        # Bibliography
├── firmware/                # ESP32 firmware (PlatformIO)
│   ├── glove/               # Control glove firmware
│   │   └── src/
│   │       ├── main.cpp     # ESP-NOW transmitter
│   │       └── mlx90395.h   # MLX90395 SPI driver
│   └── hand/                # Robotic hand firmware
│       └── src/
│           └── main.cpp     # ESP-NOW receiver + FOC
├── hardware/                # KiCad schematics, 3D print STLs
└── tests/                   # Test scripts and results
```

## Hardware Components

### Control Glove
- **MCU:** ESP32-WROOM-32 (dual-core, WiFi/ESP-NOW)
- **Sensors:** MLX90395 Hall-effect magnetic sensors (3 per finger: MCP, PIP, DIP)
  - **Interface:** SPI bus (SCK=18, MISO=19, MOSI=23)
  - **Chip selects:** Configurable GPIO per sensor
  - **Measurement:** 3-axis magnetic field → atan2 angle calculation
  - **Backup:** Flex sensors available as fallback
- **Power:** USB or LiPo

### Robotic Hand
- **MCU:** ESP32-WROOM-32
- **Motors:** 2204 BLDC motors (one per joint) with FOC control
  - **Pole pairs:** TBD (requires verification from motor spec)
- **Drivers:** **DRV8300 custom PCB** designed by Raul
  - **Components:** DRV8300 gate driver + BSZ063N04LS6 MOSFETs + ACS37042 current sensing
  - **Cost:** ~$11.77/board (OSHPark fabrication + DigiKey components)
  - **Features:** 3-phase BLDC control + inline current monitoring
- **Encoders:** AS5600 magnetic rotary encoders (I2C via TCA9548A multiplexer)
- **Structure:** 3D-printed (PLA/PETG)
- **Power:** 2S LiPo (7.4V)

### Wireless Communication
- **Current:** ESP-NOW (low-latency peer-to-peer)
- **Note:** BLE vs ESP-NOW is under team discussion for future iterations

## Development Timeline

| Phase | Dates | Deliverables |
|-------|-------|--------------|
| Research & Proposal | Aug – Dec 2025 | Literature review, EE198A proposal, oral presentation |
| Design & Component Selection | Jan – Feb 2026 | Schematic design, BOM finalization, FOC algorithm research |
| Firmware Development | Feb – Mar 2026 | MLX90395 SPI driver, ESP-NOW communication, FOC motor control |
| Mechanical Build & Integration | Mar – Apr 2026 | 3D-printed hand, motor mounting, DRV8300 PCB assembly, system integration |
| Testing & Validation | Apr – May 2026 | Latency measurement, accuracy validation, endurance testing |
| Final Report & Presentation | May 2026 | EE198B written report, oral presentation, live demo |

## Getting Started

### Prerequisites
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- ESP-IDF (for advanced FOC tuning)
- 3D printer access (for hand structure)

### Building Firmware

```bash
# Glove firmware (ESP-NOW + MLX90395)
cd firmware/glove
pio run --target upload

# Hand firmware (ESP-NOW + SimpleFOC)
cd firmware/hand
pio run --target upload
```

### Calibration

**Glove:**
1. Flash ESP32
2. Hold hand flat → record baseline magnetic field readings (20 samples)
3. Calibration stored in EEPROM for subsequent power cycles

**Hand:**
1. Flash ESP32
2. Run motor zero-position calibration
3. Verify encoder alignment per joint
4. ESP-NOW pairing is automatic on boot

## Testing

See `tests/` for test scripts. Key validation tests:
- **Latency test:** High-speed camera sync between glove movement and motor response
- **Accuracy test:** Goniometer comparison at 0°, 30°, 60°, 90° per joint
- **Endurance test:** 30-minute continuous operation
- **Packet loss test:** ESP-NOW sequence number tracking

## References

See [`docs/references.md`](docs/references.md) for full bibliography.

## License

Academic project — San Jose State University, Department of Electrical Engineering, EE198A/B.
