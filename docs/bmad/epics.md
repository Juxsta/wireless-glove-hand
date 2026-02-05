# Wireless Glove Hand - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for the Wireless Glove Interface for Real-Time Robotic Hand Mimicry project, decomposing the requirements from the senior project proposal into implementable stories.

## Requirements Inventory

### Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-01 | System shall sense finger joint angles using flex sensors |
| FR-02 | System shall transmit sensor data wirelessly via Bluetooth LE |
| FR-03 | System shall control BLDC motors using Field Oriented Control |
| FR-04 | System shall map human finger positions to robotic hand positions |
| FR-05 | System shall provide real-time (<100ms latency) motion mimicry |
| FR-06 | System shall support independent control of each finger joint |
| FR-07 | System shall calibrate flex sensors for accurate angle measurement |
| FR-08 | System shall provide encoder feedback for motor position control |

### Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| NFR-01 | System shall operate on ESP32 microcontrollers |
| NFR-02 | System shall maintain <100ms end-to-end latency |
| NFR-03 | System shall be affordable for consumer use |
| NFR-04 | System shall be modular and repairable |
| NFR-05 | Motor control shall provide smooth, jitter-free operation |
| NFR-06 | System shall operate for minimum 2 hours on battery |

### Requirements Coverage Map

| Epic | Requirements Covered |
|------|---------------------|
| Epic 1: FOC Motor Control | FR-03, FR-06, FR-08, NFR-05 |
| Epic 2: Glove Sensor System | FR-01, FR-07, NFR-01 |
| Epic 3: Wireless Communication | FR-02, FR-05, NFR-01, NFR-02 |
| Epic 4: Robotic Hand Hardware | FR-04, FR-06, NFR-04 |
| Epic 5: System Integration | FR-04, FR-05, NFR-02, NFR-06 |
| Epic 6: Testing & Documentation | All |

---

## Epic List

1. **FOC Motor Control** — Implement Field Oriented Control for BLDC motors
2. **Glove Sensor System** — Build and calibrate the flex sensor glove
3. **Wireless Communication** — Establish BLE protocol between glove and hand
4. **Robotic Hand Hardware** — Design and assemble the mechanical hand
5. **System Integration** — Connect all subsystems for real-time operation
6. **Testing & Documentation** — Validate performance and document project

---

## Epic 1: FOC Motor Control

**Goal:** Implement Field Oriented Control (FOC) for smooth, precise BLDC motor control at each finger joint, enabling accurate position tracking and torque regulation.

**Owner:** Group (primarily Matthew)

### Story 1.1: FOC Library Selection

As a developer,
I want to evaluate and select a FOC library for ESP32,
So that we have a proven foundation for motor control.

**Acceptance Criteria:**

**Given** available FOC libraries (SimpleFOC, etc.)
**When** evaluating for ESP32 compatibility
**Then** selected library must support ESP32
**And** must support position control mode
**And** must support encoder feedback

---

### Story 1.2: Single Motor FOC Implementation

As a developer,
I want to implement FOC control for a single BLDC motor,
So that we can validate smooth, low-speed operation.

**Acceptance Criteria:**

**Given** a BLDC motor with encoder connected to ESP32
**When** sending position commands
**Then** motor reaches target position within 5% error
**And** movement is smooth without cogging
**And** motor can hold position under light load

---

### Story 1.3: Multi-Motor Control

As a developer,
I want to control multiple BLDC motors simultaneously,
So that we can actuate multiple finger joints.

**Acceptance Criteria:**

**Given** 4 BLDC motors connected to ESP32
**When** sending independent position commands
**Then** all motors respond independently
**And** control loop maintains <20ms update rate
**And** no interference between motor channels

---

### Story 1.4: Motor Position Calibration

As a developer,
I want an automatic calibration routine for motor encoders,
So that absolute position is known after power-on.

**Acceptance Criteria:**

**Given** motors at unknown positions
**When** running calibration routine
**Then** zero position is established
**And** full range of motion is mapped
**And** calibration completes in <10 seconds per motor

---

## Epic 2: Glove Sensor System

**Goal:** Build a wearable glove with flex sensors that accurately captures finger joint angles and prepares data for wireless transmission.

**Owner:** Raul

### Story 2.1: Flex Sensor Circuit Design

As a developer,
I want to design a voltage divider circuit for flex sensors,
So that joint angles can be read by ESP32 ADC.

**Acceptance Criteria:**

**Given** flex sensor resistance range (10k-40k typical)
**When** designing voltage divider
**Then** output voltage range fits ESP32 ADC (0-3.3V)
**And** provides good resolution across bend range
**And** circuit is documented with schematic

---

### Story 2.2: Multi-Sensor ADC Reading

As a developer,
I want to read multiple flex sensors using ESP32 ADC,
So that all finger joints can be monitored.

**Acceptance Criteria:**

**Given** 4 flex sensors per finger (20 total)
**When** reading ADC values
**Then** all sensors read within 50ms cycle
**And** readings are stable (±2% noise)
**And** ADC channels are properly multiplexed if needed

---

### Story 2.3: Sensor Calibration System

As a developer,
I want a calibration routine that maps flex sensor values to angles,
So that accurate joint angles are computed.

**Acceptance Criteria:**

**Given** raw ADC values from flex sensors
**When** user performs calibration gesture (flat hand, fist)
**Then** min/max values are captured per sensor
**And** linear mapping to 0-90° is established
**And** calibration data persists in flash

---

### Story 2.4: Glove Physical Assembly

As a developer,
I want to integrate flex sensors into a wearable glove,
So that sensors accurately track finger movement.

**Acceptance Criteria:**

**Given** flex sensors and glove material
**When** assembling sensor glove
**Then** sensors are positioned at each finger joint
**And** wiring does not impede hand movement
**And** glove fits comfortably for extended use

---

## Epic 3: Wireless Communication

**Goal:** Establish reliable, low-latency Bluetooth LE communication between the glove (sender) and robotic hand (receiver).

**Owner:** Matthew

### Story 3.1: BLE Service Definition

As a developer,
I want to define a BLE service and characteristics for joint data,
So that glove and hand can communicate efficiently.

**Acceptance Criteria:**

**Given** joint angle data (20 floats or encoded values)
**When** defining BLE characteristics
**Then** data fits in minimal BLE packets
**And** characteristic supports notifications
**And** service UUID is documented

---

### Story 3.2: BLE Transmitter (Glove)

As a developer,
I want the glove ESP32 to advertise and transmit sensor data,
So that the robotic hand can receive position commands.

**Acceptance Criteria:**

**Given** calibrated sensor data
**When** BLE connection established
**Then** data transmits at ≥30Hz
**And** packet loss <1%
**And** reconnects automatically if connection lost

---

### Story 3.3: BLE Receiver (Hand)

As a developer,
I want the hand ESP32 to receive and parse joint data,
So that motors can be commanded to matching positions.

**Acceptance Criteria:**

**Given** incoming BLE notifications
**When** parsing joint data
**Then** data is correctly decoded to joint angles
**And** values are validated (0-90° range)
**And** stale data is detected and handled

---

### Story 3.4: Latency Optimization

As a developer,
I want to minimize end-to-end wireless latency,
So that hand movement feels responsive.

**Acceptance Criteria:**

**Given** sensor reading to motor command path
**When** measuring total latency
**Then** end-to-end latency <50ms
**And** jitter <10ms
**And** BLE connection interval optimized

---

## Epic 4: Robotic Hand Hardware

**Goal:** Design and assemble a 3D-printed robotic hand with BLDC motors at each joint for precise, independent actuation.

**Owner:** Antonio

### Story 4.1: Hand CAD Design

As a developer,
I want to design a 3D-printable hand structure,
So that motors and joints can be properly housed.

**Acceptance Criteria:**

**Given** BLDC motor dimensions and finger geometry
**When** creating CAD model
**Then** design accommodates 4 motors per finger
**And** joints have realistic range of motion (0-90°)
**And** design is printable without supports where possible

---

### Story 4.2: Motor Mount Design

As a developer,
I want motor mounts that position motors at each joint,
So that rotation directly actuates the joint.

**Acceptance Criteria:**

**Given** BLDC motor specifications
**When** designing mounts
**Then** motors are securely held
**And** shaft aligns with joint axis
**And** mounting allows encoder cable routing

---

### Story 4.3: Hand 3D Printing

As a developer,
I want to print and assemble the robotic hand,
So that motors can be installed and tested.

**Acceptance Criteria:**

**Given** CAD designs for hand components
**When** 3D printing
**Then** parts print successfully
**And** parts fit together as designed
**And** assembly is documented with photos

---

### Story 4.4: Motor Installation

As a developer,
I want to install BLDC motors in the hand assembly,
So that joints can be actuated.

**Acceptance Criteria:**

**Given** printed hand and BLDC motors
**When** installing motors
**Then** all 4 motors (for one finger) are installed
**And** wiring is routed cleanly
**And** joint can move through full range without binding

---

## Epic 5: System Integration

**Goal:** Connect all subsystems (glove, wireless, hand) for complete real-time robotic hand mimicry.

**Owner:** Group

### Story 5.1: Sensor-to-Motor Mapping

As a developer,
I want to map glove sensor values to motor positions,
So that human hand movement is replicated.

**Acceptance Criteria:**

**Given** calibrated sensor angles and motor positions
**When** mapping sensor to motor
**Then** 1:1 angle correspondence is achieved
**And** mapping is configurable per joint
**And** inverse mapping (hand to glove feedback) documented

---

### Story 5.2: Full System Test

As a developer,
I want to test the complete system end-to-end,
So that real-time mimicry is validated.

**Acceptance Criteria:**

**Given** all subsystems connected
**When** moving human hand
**Then** robotic hand mirrors movement
**And** latency is perceptible but acceptable (<100ms)
**And** all 4 joints of at least one finger work

---

### Story 5.3: Performance Tuning

As a developer,
I want to tune PID/FOC parameters for optimal response,
So that movement is smooth and accurate.

**Acceptance Criteria:**

**Given** working integrated system
**When** tuning control parameters
**Then** overshoot <10%
**And** settling time <200ms
**And** no oscillation at steady state

---

### Story 5.4: Power System Integration

As a developer,
I want to design a battery power system,
So that the system is portable.

**Acceptance Criteria:**

**Given** power requirements for motors and ESP32
**When** selecting batteries
**Then** system runs ≥2 hours on charge
**And** voltage regulators handle motor demands
**And** low battery warning implemented

---

## Epic 6: Testing & Documentation

**Goal:** Validate system performance against requirements and create comprehensive documentation for the senior project submission.

**Owner:** Group

### Story 6.1: Performance Test Suite

As a developer,
I want automated tests for key performance metrics,
So that system quality is verified.

**Acceptance Criteria:**

**Given** integrated system
**When** running test suite
**Then** latency measurements are recorded
**And** position accuracy is measured
**And** results are exportable for report

---

### Story 6.2: Final Written Report

As a developer,
I want to complete the final project report,
So that the project is properly documented.

**Acceptance Criteria:**

**Given** completed project
**When** writing report
**Then** follows EE198B template
**And** includes methodology, results, conclusions
**And** references all prior work

---

### Story 6.3: Presentation Poster

As a developer,
I want to create a project poster,
So that the project can be presented at the expo.

**Acceptance Criteria:**

**Given** completed project
**When** creating poster
**Then** summarizes problem, solution, results
**And** includes system diagram and photos
**And** follows department formatting guidelines

---

### Story 6.4: Demo Preparation

As a developer,
I want to prepare a reliable demo,
So that the system can be shown working.

**Acceptance Criteria:**

**Given** completed system
**When** preparing demo
**Then** system reliably starts and operates
**And** demo script is documented
**And** backup plan exists for failures
