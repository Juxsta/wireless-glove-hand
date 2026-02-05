# CLAUDE.md — Project Instructions

## Project Context

This is an EE198A/B Senior Project at San Jose State University: a wireless glove interface that controls a robotic hand in real-time using FOC (Field Oriented Control) and BLDC motors.

## BMAD Method

This project uses the BMAD (Breakthrough Method for Agile AI-Driven Development) framework. All planning artifacts live in `.bmad-core/` and `docs/bmad/`.

### Key Workflows

| Command | Purpose |
|---------|---------|
| `/bmad-help` | Get guidance on what to do next |
| `/product-brief` | Define problem, users, and MVP scope |
| `/create-prd` | Full requirements with personas and metrics |
| `/create-architecture` | Technical decisions and system design |
| `/create-epics-and-stories` | Break work into prioritized stories |
| `/sprint-planning` | Initialize sprint tracking |
| `/create-story` | Detail individual user stories |
| `/dev-story` | Implement a story |
| `/code-review` | Validate quality |

### Agents

Load agents for specialized guidance:

- **PM** (`pm.agent.yaml`) — Product management and requirements
- **Architect** (`architect.agent.yaml`) — System design and technical decisions
- **Dev** (`dev.agent.yaml`) — Implementation guidance
- **SM** (`sm.agent.yaml`) — Scrum Master for sprint management
- **QA** (`qa.agent.yaml`) — Testing and quality

## Project-Specific Notes

### Hardware Constraints

- ESP32 microcontrollers (limited RAM/flash)
- BLDC motors require FOC algorithms (computationally intensive)
- Flex sensors need calibration per finger
- Bluetooth LE for low-latency wireless

### Code Organization

- `firmware/glove/` — Sensor reading, BLE transmission
- `firmware/hand/` — FOC control, motor drivers, BLE reception
- Keep FOC math optimized for ESP32

### Team Assignments

- **Antonio** — Robotic hand hardware/3D printing
- **Raul** — Control glove hardware
- **Matthew** — Wireless code/protocol
- **Group** — FOC implementation, integration

## Quick Reference

**Current Phase:** Code Prototyping & FOC (Jan–Feb 2026)

**Immediate Goals:**
1. Set up FOC control codebase
2. Interface FOC with BLDC motors
3. Establish wireless control protocol

**Key Decisions Needed:**
- Motor driver: off-the-shelf vs custom?
- FOC library selection
- Communication protocol format
