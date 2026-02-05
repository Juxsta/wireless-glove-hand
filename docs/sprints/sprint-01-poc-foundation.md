---
sprint: 1
name: "PoC Foundation"
startDate: 2026-02-05
endDate: 2026-02-19
goal: "Validate FOC motor control + BLE communication as independent subsystems"
status: IN_PROGRESS
---

# Sprint 1: PoC Foundation

**Duration:** Feb 5 - Feb 19, 2026 (2 weeks)  
**Goal:** Validate that SimpleFOC runs on ESP32 with AS5600 encoder, and BLE communication works between two ESP32s at 30Hz+

---

## Sprint Objective

By end of Sprint 1, we should have:
1. ✅ Single BLDC motor spinning smoothly with FOC position control
2. ✅ AS5600 encoder reading position accurately
3. ✅ BLE link transmitting mock sensor data at 30Hz
4. ✅ Components ordered and tracking confirmed

**This sprint is VALIDATION — if FOC doesn't work on ESP32, we need to know now.**

---

## Stories

### S1.1: Component Procurement
**Owner:** Eric  
**Points:** 2  
**Status:** 🔄 IN PROGRESS

**Tasks:**
- [ ] Order 5x 2804 BLDC motors (AliExpress) — **DO TODAY**
- [ ] Order 4x DRV8302 drivers (Amazon)
- [ ] Order 5x AS5600 encoders (Amazon)
- [ ] Order 1x TCA9548A I2C mux (Amazon)
- [ ] Order 3x flex sensors (Amazon/SparkFun)
- [ ] Order magnets, wires, proto boards
- [ ] Create tracking spreadsheet with order numbers + ETAs

**Acceptance Criteria:**
- All orders placed with tracking numbers
- ETA spreadsheet shared with team

---

### S1.2: Dev Environment Setup
**Owner:** Matthew  
**Points:** 1  
**Status:** ⏳ TODO

**Tasks:**
- [ ] Initialize PlatformIO project for `firmware/glove`
- [ ] Initialize PlatformIO project for `firmware/hand`
- [ ] Add SimpleFOC library to hand project
- [ ] Add ESP32 BLE libraries to both projects
- [ ] Verify builds compile (even if empty main)

**Acceptance Criteria:**
- `pio run` succeeds for both projects
- README updated with build instructions

---

### S1.3: Single Motor FOC Test
**Owner:** Matthew + Antonio  
**Points:** 5  
**Status:** ⏳ TODO (blocked on motor availability)

**Tasks:**
- [ ] Wire DRV8302 to ESP32 (pins per architecture doc)
- [ ] Wire AS5600 encoder to ESP32 I2C
- [ ] Attach magnet to motor shaft
- [ ] Implement SimpleFOC position control example
- [ ] Tune PID for smooth motion (no oscillation)
- [ ] Document working configuration

**Acceptance Criteria:**
- Motor tracks commanded position within 5°
- Smooth motion at <10 RPM (no cogging)
- Can hold position under light finger pressure

**Notes:**
- If we don't have motors yet, borrow a gimbal motor from drone club or use any available BLDC
- This is the highest-risk story — prioritize validation

---

### S1.4: AS5600 Encoder Validation
**Owner:** Matthew  
**Points:** 2  
**Status:** ⏳ TODO

**Tasks:**
- [ ] Wire AS5600 to ESP32 via I2C
- [ ] Read raw angle values (0-4095)
- [ ] Verify smooth rotation tracking (no jumps)
- [ ] Test magnet distance sensitivity
- [ ] Document alignment requirements

**Acceptance Criteria:**
- Angle reads correctly through full rotation
- No discontinuities or noise spikes
- Works at 1kHz polling rate

---

### S1.5: I2C Multiplexer Test
**Owner:** Matthew  
**Points:** 2  
**Status:** ⏳ TODO (after S1.4)

**Tasks:**
- [ ] Wire TCA9548A to ESP32
- [ ] Connect 2 AS5600 encoders on different channels
- [ ] Implement channel switching code
- [ ] Verify both encoders read independently
- [ ] Measure switching overhead

**Acceptance Criteria:**
- Both encoders read correctly
- Channel switch takes <1ms
- No cross-talk between channels

---

### S1.6: BLE Communication Prototype
**Owner:** Matthew  
**Points:** 3  
**Status:** ⏳ TODO

**Tasks:**
- [ ] Implement BLE GATT server on "glove" ESP32
- [ ] Implement BLE GATT client on "hand" ESP32
- [ ] Define service/characteristic UUIDs per architecture
- [ ] Transmit 6-byte packets at 30Hz
- [ ] Measure actual throughput and latency

**Acceptance Criteria:**
- Stable connection maintained for 10+ minutes
- Packet rate ≥30Hz measured
- Latency <50ms (BLE only, not motor response)
- Auto-reconnect after disconnect

---

### S1.7: Flex Sensor Circuit
**Owner:** Raul  
**Points:** 2  
**Status:** ⏳ TODO

**Tasks:**
- [ ] Build voltage divider circuit on breadboard
- [ ] Connect to ESP32 ADC (GPIO 34)
- [ ] Read values across bend range
- [ ] Implement basic calibration (store min/max)
- [ ] Map to angle (0-90°)

**Acceptance Criteria:**
- Clear correlation between bend and ADC value
- Noise <5% of range
- Calibration routine works

---

## Definition of Done

A story is DONE when:
- [ ] All tasks completed
- [ ] Code committed to repo
- [ ] Basic tests pass
- [ ] Documentation updated
- [ ] Demo shown to at least one team member

---

## Risks & Blockers

| Risk | Status | Action |
|------|--------|--------|
| Motor lead time (AliExpress) | 🔴 | Order TODAY, look for Amazon backup |
| No motors to test FOC | 🟡 | Borrow gimbal motor from drone club |
| ESP32 PWM conflicts | 🟢 | Pin mapping validated in architecture |

---

## Daily Sync

**Format:** 15-min async check-in on Discord  
**Questions:**
1. What did you complete?
2. What are you working on?
3. Any blockers?

---

## Sprint Review (Feb 19)

**Demo Checklist:**
- [ ] Single motor responds to serial commands
- [ ] BLE packets flow between two ESP32s
- [ ] Show encoder angle on serial monitor
- [ ] Flex sensor changes motor position (if integration done)

---

## Velocity Tracking

| Story | Points | Status |
|-------|--------|--------|
| S1.1 Component Procurement | 2 | 🔄 |
| S1.2 Dev Environment | 1 | ⏳ |
| S1.3 FOC Test | 5 | ⏳ |
| S1.4 Encoder Test | 2 | ⏳ |
| S1.5 I2C Mux Test | 2 | ⏳ |
| S1.6 BLE Prototype | 3 | ⏳ |
| S1.7 Flex Sensor | 2 | ⏳ |
| **TOTAL** | **17** | |

---

## Notes

- Focus on **validation over polish** — we need to know if FOC works before building the hand
- If motors are delayed, do S1.4-S1.6 first (encoders + BLE don't need motors)
- Weekly advisor check-in: schedule with Junaid
