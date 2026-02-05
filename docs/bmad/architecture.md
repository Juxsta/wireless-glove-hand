---
stepsCompleted: [context, decisions-pending]
inputDocuments: [product-brief.md, prd.md, Senior Project Proposal]
workflowType: architecture
date: 2026-02-05
author: Eric Reyes
status: DRAFT - Decisions Needed
---

# Architecture Decision Document
## Wireless Glove Interface for Real-Time Robotic Hand Mimicry

**Version:** 0.1 (Draft)  
**Author:** Eric Reyes  
**Date:** February 5, 2026  
**Status:** Pending Technical Decisions

---

## 1. System Context

### 1.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         CONTROL GLOVE                                │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │ Flex Sensors│───▶│    ESP32    │───▶│  BLE Radio  │─────────┐   │
│  │ (20 total)  │    │  (ADC+App)  │    │ (Transmit)  │         │   │
│  └─────────────┘    └─────────────┘    └─────────────┘         │   │
│                                                                 │   │
│  ┌─────────────┐                                               │   │
│  │   Battery   │                                               │   │
│  │  (3.7V Li)  │                                               │   │
│  └─────────────┘                                               │   │
└─────────────────────────────────────────────────────────────────│───┘
                                                                  │
                              Bluetooth LE                        │
                              (~30Hz data)                        │
                                                                  │
┌─────────────────────────────────────────────────────────────────│───┐
│                         ROBOTIC HAND                            │   │
│                                                                 ▼   │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │  BLE Radio  │───▶│    ESP32    │───▶│ FOC Control │             │
│  │  (Receive)  │    │   (Main)    │    │   Loop      │             │
│  └─────────────┘    └─────────────┘    └──────┬──────┘             │
│                                               │                     │
│          ┌────────────────────────────────────┼────────────────┐   │
│          │                                    │                │   │
│          ▼                                    ▼                ▼   │
│  ┌───────────────┐                ┌───────────────┐    ┌──────────┐│
│  │ Motor Driver  │                │ Motor Driver  │    │  Motor   ││
│  │   (DRV8302)   │                │   (DRV8302)   │    │ Driver N ││
│  └───────┬───────┘                └───────┬───────┘    └────┬─────┘│
│          │                                │                 │      │
│          ▼                                ▼                 ▼      │
│  ┌───────────────┐                ┌───────────────┐    ┌──────────┐│
│  │ BLDC + Encoder│                │ BLDC + Encoder│    │  BLDC N  ││
│  │   (Joint 1)   │                │   (Joint 2)   │    │(Joint N) ││
│  └───────────────┘                └───────────────┘    └──────────┘│
│                                                                     │
│  ┌─────────────┐                                                   │
│  │   Battery   │                                                   │
│  │ (11.1V 3S)  │                                                   │
│  └─────────────┘                                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Data Flow

```
Flex Sensor → ADC (12-bit) → Angle Calculation → BLE Packet
    │                                                │
    │ 50Hz sample                                    │ 30Hz transmit
    ▼                                                ▼
[0-4095] → [0-90°] ────────────────────────▶ [packed bytes]
                                                     │
                                                     │ BLE notify
                                                     ▼
                                            BLE Receive → Unpack
                                                     │
                                                     │ 1kHz control
                                                     ▼
                                            Position Command → FOC
                                                     │
                                                     ▼
                                            Motor Position (encoder)
```

---

## 2. Architectural Decisions

### ADR-001: MCU Platform

| Attribute | Value |
|-----------|-------|
| **Status** | ✅ DECIDED |
| **Decision** | ESP32-WROOM-32 |
| **Rationale** | Dual-core (FOC + BLE), built-in WiFi/BLE, Arduino support, $4 cost |
| **Alternatives Considered** | STM32 (better ADC but no BLE), nRF52 (better BLE but less GPIO) |
| **Consequences** | Must work within ESP32 ADC limitations; 12-bit noisy |

---

### ADR-002: FOC Library

| Attribute | Value |
|-----------|-------|
| **Status** | 🟡 PROPOSED |
| **Options** | SimpleFOC, VESC firmware, Custom |
| **Recommendation** | **SimpleFOC** |
| **Rationale** | Active community, Arduino-compatible, well-documented, position control built-in |
| **Risks** | May need optimization for 4-motor control; RAM usage |
| **Action Needed** | Benchmark single motor, then scale test |

**Team Input Needed:**
- [ ] Has anyone used SimpleFOC before?
- [ ] Do we need torque control or just position?
- [ ] What's our target control loop rate? (1kHz minimum recommended)

---

### ADR-003: Motor Driver

| Attribute | Value |
|-----------|-------|
| **Status** | 🔴 NEEDS DECISION |
| **Options** | |

#### Option A: SimpleFOC Shield v2.0.4
| Pros | Cons |
|------|------|
| Plug-and-play with SimpleFOC | $25 each (x4 = $100) |
| Inline current sensing | Only 1 motor per shield |
| 12-24V, 5A continuous | Large footprint |

#### Option B: DRV8302 Module
| Pros | Cons |
|------|------|
| $8-12 each | Requires wiring |
| 45V, 15A capable | More complex setup |
| Current sensing built-in | Less documentation |

#### Option C: Custom PCB
| Pros | Cons |
|------|------|
| Optimized for our needs | Design time + fab cost |
| Smaller form factor | Risk of errors |
| Lower unit cost at scale | Not practical for prototype |

**Recommendation:** Start with **SimpleFOC Shield** for 1-2 motors (proof of concept), then evaluate **DRV8302** for full hand if budget constrained.

**Team Input Needed:**
- [ ] What's our motor driver budget?
- [ ] Do we prioritize simplicity (Shield) or cost (DRV8302)?
- [ ] Timeline for custom PCB if needed?

---

### ADR-004: BLDC Motor Selection

| Attribute | Value |
|-----------|-------|
| **Status** | 🔴 NEEDS DECISION |
| **Constraints** | Small size (finger joint), sufficient torque, encoder compatible |

#### Candidates

| Motor | Size | Kv | Torque | Price | Notes |
|-------|------|-----|--------|-------|-------|
| GM2804 (iPower) | 28x10mm | 100 | ~0.1 Nm | $20 | Gimbal motor, smooth |
| GB2208 | 22x8mm | 80 | ~0.05 Nm | $15 | Smaller, less torque |
| 2804 Generic | 28x12mm | 140 | ~0.08 Nm | $8 | Cheaper, variable quality |
| N20 Geared DC | 12x10mm | N/A | 0.1 Nm | $3 | Not BLDC (backup option) |

**Recommendation:** **GM2804 (iPower)** for quality and smooth low-speed operation, OR **generic 2804** if budget-constrained.

**Team Input Needed:**
- [ ] What finger joint torque do we actually need? (need to measure/calculate)
- [ ] Can we test a single motor before bulk ordering?
- [ ] Supplier preference? (AliExpress lead time vs Amazon cost)

---

### ADR-005: Encoder Type

| Attribute | Value |
|-----------|-------|
| **Status** | 🔴 NEEDS DECISION |
| **Options** | |

#### Option A: AS5600 Magnetic Encoder
| Pros | Cons |
|------|------|
| 12-bit resolution | Requires magnet alignment |
| I2C interface (2 wires) | Only 1 per I2C bus without mux |
| $2-3 each | Sensitive to stray fields |

#### Option B: AS5048A Magnetic Encoder
| Pros | Cons |
|------|------|
| 14-bit resolution | $8-10 each |
| SPI interface (daisy-chain) | More wiring |
| PWM output option | Overkill for our accuracy needs? |

#### Option C: Optical Encoder (AMT102)
| Pros | Cons |
|------|------|
| High resolution (2048 PPR) | $20+ each |
| No magnetic interference | Larger size |
| Quadrature output | Requires interrupt handling |

**Recommendation:** **AS5600** for cost and simplicity. Use I2C multiplexer (TCA9548A) for multiple encoders.

**Team Input Needed:**
- [ ] Is 12-bit (4096 steps/rev) sufficient for our accuracy?
- [ ] Do we have I2C address conflicts with other devices?
- [ ] Experience with magnetic encoder alignment?

---

### ADR-006: Flex Sensor Configuration

| Attribute | Value |
|-----------|-------|
| **Status** | 🟡 PROPOSED |
| **Decision** | 4 flex sensors per finger (MCP, PIP, DIP, fingertip) |
| **Sensor Type** | Spectra Symbol 4.5" flex sensor |
| **Circuit** | Voltage divider with 10kΩ reference resistor |
| **ADC Channels** | 20 total (5 fingers × 4 joints) |

**ESP32 ADC Allocation:**
- ADC1 (8 channels): Fingers 1-2
- ADC2 (10 channels): Fingers 3-5 (⚠️ Note: ADC2 conflicts with WiFi, but we use BLE only)

**Alternative:** Use external ADC (ADS1115) for better resolution if noise is problematic.

---

### ADR-007: BLE Protocol Design

| Attribute | Value |
|-----------|-------|
| **Status** | 🟡 PROPOSED |
| **Service UUID** | Custom (to be generated) |
| **Characteristic** | Single notify characteristic for all joint data |
| **Data Format** | See below |

**Proposed Packet Format (20 bytes):**
```
Byte 0:     Packet sequence number (0-255)
Byte 1:     Finger bitmap (which fingers have data)
Bytes 2-5:  Finger 1 joints (4 × 8-bit angles, 0-180 mapped to 0-255)
Bytes 6-9:  Finger 2 joints
Bytes 10-13: Finger 3 joints
Bytes 14-17: Finger 4 joints
Bytes 18-20: Finger 5 joints (3 bytes, 4th in next packet or compressed)
```

**Alternative:** Use 16-bit angles for more precision (doubles packet size).

**Team Input Needed:**
- [ ] Is 8-bit angle resolution (0.7° steps) sufficient?
- [ ] Do we need timestamps in packets?

---

### ADR-008: Power Architecture

| Attribute | Value |
|-----------|-------|
| **Status** | 🟡 PROPOSED |

**Glove Power:**
- Single 3.7V 1000mAh LiPo
- ESP32 + sensors draw ~100mA
- Runtime: ~10 hours

**Hand Power:**
- 3S LiPo (11.1V) for motors
- 5V regulator for ESP32
- Separate motor and logic grounds (star topology)
- Estimated motor current: 0.5A per motor × 4 = 2A average, 5A peak

**Concerns:**
- Motor PWM noise may affect ADC/BLE on same board
- Need decoupling capacitors on motor drivers
- Consider separate ESP32 for motor control if interference issues

---

## 3. Component Summary (BOM Draft)

| Component | Qty | Unit Cost | Total | Status |
|-----------|-----|-----------|-------|--------|
| ESP32-WROOM-32 | 2 | $4 | $8 | ✅ Available |
| Flex Sensor 4.5" | 20 | $8 | $160 | ⚠️ Expensive |
| BLDC Motor (2804) | 4+ | $15 | $60+ | 🔴 Need selection |
| Motor Driver | 4+ | $12-25 | $48-100 | 🔴 Need selection |
| AS5600 Encoder | 4+ | $3 | $12+ | 🟡 Proposed |
| 3S LiPo Battery | 1 | $20 | $20 | ✅ Available |
| 1S LiPo Battery | 1 | $10 | $10 | ✅ Available |
| 3D Print Filament | 1kg | $25 | $25 | ✅ Available |
| Misc (wires, PCB, etc.) | - | - | $50 | Estimate |
| **TOTAL** | | | **~$400-450** | Over budget ⚠️ |

**Cost Reduction Options:**
1. Fewer flex sensors (2 per finger instead of 4) — saves $80
2. Generic motors from AliExpress — saves $40
3. DRV8302 instead of SimpleFOC Shield — saves $50
4. DIY flex sensors (velostat) — experimental, saves $100+

---

## 4. Risk Analysis

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| FOC doesn't run smoothly on ESP32 | Low | High | Test early with single motor |
| Motor drivers overheat | Medium | Medium | Add heatsinks, limit duty cycle |
| BLE interference from motors | Medium | Medium | Shielding, separate power |
| 3D printed parts too weak | Medium | Low | Iterate design, add reinforcement |
| Encoder alignment issues | Medium | Medium | Design alignment jig |
| Over budget | High | Medium | Prioritize MVP (1 finger first) |

---

## 5. Open Questions for Team

### Motor & Control
1. What specific BLDC motor should we order for testing?
2. SimpleFOC Shield ($25) or DRV8302 ($12) for drivers?
3. Do we need current sensing / torque control?

### Sensors
4. 4 flex sensors per finger or reduce to 2-3?
5. Commercial flex sensors or DIY alternative?

### Architecture
6. Single ESP32 for motors, or one per finger pair?
7. Wired or wireless between glove halves (left/right)?

### Schedule
8. Can we order components by end of week?
9. Who's taking lead on each subsystem?

---

## 6. Next Steps

1. **Immediate (This Week)**
   - [ ] Team reviews this doc and provides input
   - [ ] Finalize motor and driver selection
   - [ ] Place component orders

2. **Week 2**
   - [ ] Single motor + driver + encoder test
   - [ ] BLE communication prototype
   - [ ] Initial CAD for finger joint

3. **Week 3-4**
   - [ ] FOC tuning and characterization
   - [ ] Flex sensor circuit validation
   - [ ] Integration of sensor → BLE → motor

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-05 | Eric Reyes | Initial architecture draft |
