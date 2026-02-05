---
sprint: 2
name: "Single Finger Integration"
startDate: 2026-02-19
endDate: 2026-03-05
goal: "Assemble and demonstrate single finger PoC with end-to-end mimicry"
status: PLANNED
---

# Sprint 2: Single Finger Integration

**Duration:** Feb 19 - Mar 5, 2026 (2 weeks)  
**Goal:** Complete working single-finger demo: glove sensor → BLE → 4 motors → visible mimicry

**Prerequisites:** Sprint 1 validation complete (FOC works, BLE works)

---

## Sprint Objective

By end of Sprint 2, we should have:
1. ✅ 4 motors controlled independently with FOC
2. ✅ 3D printed finger assembly with motors mounted
3. ✅ End-to-end data flow: flex sensor → BLE → motor position
4. ✅ Latency measured and <100ms
5. ✅ 30-minute demo capability

---

## Stories

### S2.1: 4-Motor FOC Control
**Owner:** Matthew  
**Points:** 5  

**Tasks:**
- [ ] Wire all 4 DRV8302 drivers to ESP32
- [ ] Configure SimpleFOC for 4 motors
- [ ] Implement I2C mux encoder reading in control loop
- [ ] Tune each motor's PID independently
- [ ] Verify no timing conflicts at 4x ~500Hz loops

**Acceptance Criteria:**
- All 4 motors respond to independent position commands
- Control loop stable (no oscillation)
- CPU usage leaves headroom for BLE

---

### S2.2: Finger CAD Design
**Owner:** Antonio  
**Points:** 3  

**Tasks:**
- [ ] Design MCP joint with motor mount
- [ ] Design PIP joint with motor mount
- [ ] Design DIP joint with motor mount
- [ ] Design fingertip segment
- [ ] Design linkage system between joints
- [ ] Export STL files for printing

**Acceptance Criteria:**
- All parts printable without supports (or minimal)
- Motor fits snugly with magnet clearance for encoder
- Joint has 90° range of motion
- Assembly documented with photos

---

### S2.3: 3D Print & Assembly
**Owner:** Antonio  
**Points:** 3  

**Tasks:**
- [ ] Print all finger components (PLA or PETG)
- [ ] Test fit motors and encoders
- [ ] Iterate on tolerances if needed
- [ ] Assemble full finger with wiring
- [ ] Document assembly steps

**Acceptance Criteria:**
- Finger moves through full range without binding
- Motors securely mounted
- Wiring doesn't impede motion
- Can be disassembled for debugging

---

### S2.4: Glove Prototype
**Owner:** Raul  
**Points:** 2  

**Tasks:**
- [ ] Mount 2 flex sensors on glove finger
- [ ] Route wiring to ESP32 mounted on wrist
- [ ] Implement calibration gesture (flat/fist)
- [ ] Store calibration in EEPROM/flash
- [ ] Package for comfortable wear

**Acceptance Criteria:**
- Sensors respond to natural finger movement
- Calibration persists across power cycles
- Can wear for 30+ minutes comfortably

---

### S2.5: End-to-End Integration
**Owner:** Group  
**Points:** 5  

**Tasks:**
- [ ] Connect glove firmware to BLE transmit
- [ ] Connect hand firmware BLE receive to FOC control
- [ ] Implement angle mapping (sensor → motor)
- [ ] Add interpolation buffer for smooth motion
- [ ] Test latency with high-speed camera or LED sync

**Acceptance Criteria:**
- Bending glove finger → robotic finger mirrors
- Latency <100ms visually
- Smooth motion (no jerks or jumps)
- Works for 30 minutes continuously

---

### S2.6: Demo Preparation
**Owner:** Eric  
**Points:** 2  

**Tasks:**
- [ ] Create demo script (what to show, in what order)
- [ ] Document setup procedure
- [ ] Create backup plan for failures
- [ ] Record demo video for advisor/class
- [ ] Prepare 2-minute pitch explaining uniqueness

**Acceptance Criteria:**
- Demo runs reliably 3x in a row
- Setup takes <5 minutes
- Video captured and shareable

---

## Velocity Target

| Story | Points |
|-------|--------|
| S2.1 4-Motor FOC | 5 |
| S2.2 Finger CAD | 3 |
| S2.3 Print & Assembly | 3 |
| S2.4 Glove Prototype | 2 |
| S2.5 Integration | 5 |
| S2.6 Demo Prep | 2 |
| **TOTAL** | **20** |

---

## Sprint 2 Demo Day (Mar 5)

**Deliverable:** Working single-finger demo

**Show:**
1. Wear glove, power on both devices
2. Bend finger slowly → robotic finger follows
3. Quick movements → show responsiveness
4. Hold position → robotic finger holds
5. Explain: "This is FOC — smooth low-speed control that servos can't do"

---

## Post-PoC Decision Point

After Sprint 2, team decides:
- ✅ PoC validated → proceed to full hand (Sprints 3-6)
- ⚠️ Issues found → add Sprint 2.5 for fixes
- ❌ Fundamental problems → pivot approach (unlikely if Sprint 1 passed)
