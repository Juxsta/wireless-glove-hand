# Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**EE198A/B Senior Project — San Jose State University**

## Team

- Antonio Rojas (014974063)
- Raul Hernandez-Solis (016319693)
- Matthew Men (016601806)
- Eric Reyes

**Advisor:** Junaid Anwar

## Overview

A wearable glove system with flex sensors at each finger joint that wirelessly controls a 3D-printed robotic hand using Field Oriented Control (FOC) with BLDC motors for precise, smooth movement.

## Key Features

- **Joint-level sensing** — 4 flex sensors per finger for high-resolution input
- **FOC motor control** — BLDC motors at each joint for smooth, lifelike motion
- **Wireless communication** — ESP32 with Bluetooth LE
- **Real-time mimicry** — Low-latency human-to-robot motion mapping

## Architecture

```
┌─────────────────┐     BLE      ┌──────────────────┐
│   Control Glove │ ─────────── │   Robotic Hand   │
│  (ESP32 + Flex  │              │  (ESP32 + BLDC   │
│    Sensors)     │              │  Motors + FOC)   │
└─────────────────┘              └──────────────────┘
```

## Project Structure

```
wireless-glove-hand/
├── .bmad-core/          # BMAD Method workflows & agents
├── docs/                # Project documentation
│   ├── bmad/            # BMAD artifacts (PRD, architecture, etc.)
│   └── hardware/        # Schematics, BOM, CAD files
├── firmware/            # ESP32 firmware
│   ├── glove/           # Control glove firmware
│   └── hand/            # Robotic hand firmware
├── hardware/            # Hardware design files
└── tests/               # Test files
```

## Timeline

| Phase | Timeline | Status |
|-------|----------|--------|
| Research | Aug–Dec 2025 | ✅ Complete |
| Code Prototyping & FOC | Jan 1 – Feb 3, 2026 | 🔄 In Progress |
| Motor Prototyping | Feb 4 – Mar 18, 2026 | ⏳ Upcoming |
| Integration Testing | Mar 19 – Apr 30, 2026 | ⏳ Upcoming |
| Final Report | May 1–15, 2026 | ⏳ Upcoming |

## Getting Started

### Prerequisites

- ESP-IDF or Arduino IDE
- PlatformIO (recommended)
- BMAD Method (included in `.bmad-core/`)

### Development

This project uses the BMAD Method for agile AI-driven development. Key workflows:

- `/product-brief` — Define problem and MVP scope
- `/create-prd` — Full requirements document
- `/create-architecture` — Technical decisions
- `/create-epics-and-stories` — Break work into stories
- `/sprint-planning` — Sprint tracking

## License

Academic project — San Jose State University, Department of Electrical Engineering

## References

See `docs/references.md` for full bibliography.
