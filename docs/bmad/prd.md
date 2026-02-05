---
stepsCompleted: [discovery, success, journeys, domain, functional, nonfunctional]
inputDocuments: [product-brief.md, Senior Project Proposal]
workflowType: prd
date: 2026-02-05
author: Eric Reyes
---

# Product Requirements Document
## Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**Version:** 1.0  
**Author:** Eric Reyes  
**Date:** February 5, 2026  
**Status:** Draft

---

## 1. Executive Summary

This PRD defines the requirements for a wireless glove-controlled robotic hand system that provides real-time, joint-level mimicry of human finger movements using Field Oriented Control (FOC) for BLDC motors. The system targets educational and research applications where affordability and precision must be balanced.

---

## 2. User Personas

### Persona 1: Alex — Robotics Student

| Attribute | Value |
|-----------|-------|
| **Age** | 22 |
| **Role** | Senior EE student |
| **Technical Level** | Intermediate (Arduino, basic motor control) |
| **Goals** | Learn FOC, build portfolio project |
| **Frustrations** | Commercial systems too expensive; DIY tutorials oversimplified |
| **Quote** | "I want to understand how it really works, not just copy code." |

### Persona 2: Dr. Chen — Rehabilitation Researcher

| Attribute | Value |
|-----------|-------|
| **Age** | 45 |
| **Role** | University professor |
| **Technical Level** | High (controls theory, some embedded) |
| **Goals** | Prototype prosthetic control interfaces quickly |
| **Frustrations** | Lab budget constraints; long lead times for custom hardware |
| **Quote** | "I need a platform I can modify without starting from scratch." |

### Persona 3: Jordan — Maker/Hobbyist

| Attribute | Value |
|-----------|-------|
| **Age** | 35 |
| **Role** | Software engineer (day job), robotics hobbyist |
| **Technical Level** | High software, medium hardware |
| **Goals** | Build cool projects; learn new domains |
| **Frustrations** | Incomplete documentation; projects that "almost work" |
| **Quote** | "Show me it works end-to-end, then I'll dig into the details." |

---

## 3. User Journeys

### Journey 1: First-Time Setup (Alex)

```
1. Unbox/print components
2. Flash firmware to ESP32s (glove + hand)
3. Power on both devices
4. Run sensor calibration (flat hand → fist)
5. Run motor calibration (zero position)
6. Pair via BLE
7. Move hand → see robotic hand mirror movement
8. Success! 🎉
```

**Success Criteria:** Complete setup in <2 hours with documentation

### Journey 2: Research Iteration (Dr. Chen)

```
1. Clone repository
2. Modify control algorithm (e.g., add filtering)
3. Rebuild and flash
4. Test with standard movement sequence
5. Measure latency and accuracy
6. Compare against baseline
7. Iterate
```

**Success Criteria:** Code change → test cycle in <15 minutes

### Journey 3: Demo Day (Jordan)

```
1. Power on system
2. Wear glove
3. Perform demo movements (wave, grip, point)
4. Audience sees responsive mimicry
5. Answer questions about implementation
6. Share GitHub link
```

**Success Criteria:** System runs 30+ minutes without intervention

---

## 4. Functional Requirements

### 4.1 Glove Subsystem (FR-GLV)

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| FR-GLV-01 | System shall read flex sensor resistance values | Must | ADC reads 4 sensors per finger |
| FR-GLV-02 | System shall convert resistance to joint angle (0-90°) | Must | Angle accuracy ±5° vs goniometer |
| FR-GLV-03 | System shall support sensor calibration | Must | User can recalibrate via button/command |
| FR-GLV-04 | System shall store calibration in non-volatile memory | Should | Calibration persists across power cycles |
| FR-GLV-05 | System shall sample sensors at ≥50Hz | Must | Verified via logic analyzer |
| FR-GLV-06 | System shall filter sensor noise | Should | Moving average or low-pass filter |

### 4.2 Wireless Communication (FR-COM)

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| FR-COM-01 | System shall transmit joint data via BLE | Must | Data received by hand ESP32 |
| FR-COM-02 | System shall achieve ≥30Hz update rate | Must | Measured packet rate |
| FR-COM-03 | System shall reconnect automatically after disconnect | Must | Reconnects within 5 seconds |
| FR-COM-04 | System shall indicate connection status | Should | LED or serial output |
| FR-COM-05 | System shall use notifications (not polling) | Must | BLE characteristic notifications enabled |
| FR-COM-06 | System shall minimize packet size | Should | Encoded angles <40 bytes |

### 4.3 Motor Control (FR-MOT)

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| FR-MOT-01 | System shall implement FOC for BLDC motors | Must | Smooth rotation at <10 RPM |
| FR-MOT-02 | System shall support position control mode | Must | Motor holds commanded position |
| FR-MOT-03 | System shall read encoder feedback | Must | Position updated in control loop |
| FR-MOT-04 | System shall control 4 motors independently | Must | No cross-talk between channels |
| FR-MOT-05 | System shall support motor calibration | Must | Electrical zero detected |
| FR-MOT-06 | System shall limit current for safety | Must | Configurable current limit |
| FR-MOT-07 | System shall detect stall/fault conditions | Should | Fault flag raised on stall |

### 4.4 System Integration (FR-SYS)

| ID | Requirement | Priority | Acceptance Criteria |
|----|-------------|----------|---------------------|
| FR-SYS-01 | System shall map sensor angles to motor positions | Must | 1:1 correspondence verified |
| FR-SYS-02 | System shall achieve <100ms end-to-end latency | Must | Measured with high-speed camera |
| FR-SYS-03 | System shall provide startup self-test | Should | Reports sensor/motor status |
| FR-SYS-04 | System shall support configuration via serial | Should | Baud rate, limits adjustable |

---

## 5. Non-Functional Requirements

### 5.1 Performance (NFR-PERF)

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-PERF-01 | End-to-end latency | <100ms | High-speed camera sync |
| NFR-PERF-02 | Control loop rate | ≥1kHz | Timer interrupt measurement |
| NFR-PERF-03 | BLE throughput | ≥30 packets/sec | Packet counter |
| NFR-PERF-04 | Position accuracy | ±5° | Comparison to reference |

### 5.2 Reliability (NFR-REL)

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-REL-01 | Mean time between failures | >4 hours | Continuous operation test |
| NFR-REL-02 | BLE packet loss | <1% | Sequence number tracking |
| NFR-REL-03 | Motor thermal protection | No burnout | 30-min stress test |

### 5.3 Usability (NFR-USE)

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-USE-01 | Setup time (new user) | <2 hours | User study |
| NFR-USE-02 | Calibration time | <2 minutes | Timed procedure |
| NFR-USE-03 | Documentation completeness | All steps covered | Checklist review |

### 5.4 Hardware Constraints (NFR-HW)

| ID | Requirement | Target | Rationale |
|----|-------------|--------|-----------|
| NFR-HW-01 | MCU platform | ESP32 | WiFi/BLE, dual-core, affordable |
| NFR-HW-02 | Motor type | BLDC with encoder | FOC requirement |
| NFR-HW-03 | Sensor type | Flex resistive | Cost, simplicity |
| NFR-HW-04 | Fabrication | 3D printed (FDM) | Accessible, modifiable |
| NFR-HW-05 | Budget | <$300 total | Student affordability |

### 5.5 Power (NFR-PWR)

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-PWR-01 | Glove battery life | >8 hours | Continuous use test |
| NFR-PWR-02 | Hand runtime | >2 hours | Motor load test |
| NFR-PWR-03 | Idle power | <100mW | Multimeter |

---

## 6. Technical Constraints

### 6.1 ESP32 Limitations

- **ADC**: 12-bit, but noisy above 3.1V — use voltage dividers carefully
- **PWM**: 16 channels available, need 3 per motor (12 for 4 motors)
- **RAM**: 320KB — FOC library + BLE stack must fit
- **Flash**: 4MB — sufficient for firmware + calibration data

### 6.2 FOC Constraints

- Requires encoder with ≥12-bit resolution for smooth low-speed operation
- Clarke/Park transforms need ~10μs per iteration
- Current sensing needed for torque control (optional for position-only)

### 6.3 BLE Constraints

- MTU typically 23 bytes (can negotiate higher)
- Connection interval minimum 7.5ms (133Hz theoretical max)
- Notification latency adds 1-2 connection intervals

---

## 7. Dependencies

| Dependency | Type | Risk | Mitigation |
|------------|------|------|------------|
| SimpleFOC Library | Software | Low | Well-maintained, active community |
| ESP32 Arduino Core | Software | Low | Stable, widely used |
| BLDC Motors (sourcing) | Hardware | Medium | Identify multiple suppliers |
| Flex Sensors (sourcing) | Hardware | Low | Multiple vendors available |
| 3D Printer Access | Hardware | Low | School facilities available |
| Encoder Magnets | Hardware | Medium | Order early, test compatibility |

---

## 8. Acceptance Criteria Summary

### MVP (Minimum Viable Product)

- [ ] Single finger (4 joints) with working mimicry
- [ ] Latency <100ms measured
- [ ] 30-minute continuous operation
- [ ] Documented setup procedure
- [ ] Code compiles and runs from fresh clone

### Full Demo

- [ ] All 5 fingers operational
- [ ] Latency <50ms
- [ ] 2-hour runtime
- [ ] Calibration persists across restarts
- [ ] Performance test results documented

---

## 9. Open Technical Decisions

These decisions should be made during architecture phase:

| Decision | Options | Considerations |
|----------|---------|----------------|
| **FOC Library** | SimpleFOC, custom | SimpleFOC proven but may need optimization |
| **Motor Driver** | SimpleFOC Shield, DRV8302, custom | Cost vs features vs size |
| **Encoder Type** | Magnetic (AS5600), optical | Magnetic simpler, optical more precise |
| **Data Encoding** | Raw floats, fixed-point, delta | Bandwidth vs precision tradeoff |
| **Power Architecture** | Single battery, split | Motor current spikes may affect MCU |

---

## 10. References

1. Senior Project Proposal (EE198A, Dec 2025)
2. SimpleFOC Documentation: https://simplefoc.com
3. ESP32 Technical Reference Manual
4. BLE Specification v5.0

---

## Appendix A: Requirements Traceability

| PRD Requirement | Epic | Stories |
|-----------------|------|---------|
| FR-GLV-* | Epic 2 | #5, #6, #7, #8 |
| FR-COM-* | Epic 3 | #9, #10, #11, #12 |
| FR-MOT-* | Epic 1 | #1, #2, #3, #4 |
| FR-SYS-* | Epic 5 | #17, #18, #19, #20 |
| NFR-* | All | Cross-cutting |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-05 | Eric Reyes | Initial PRD from proposal |
