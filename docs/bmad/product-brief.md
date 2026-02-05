---
stepsCompleted: [vision, users, metrics, scope]
inputDocuments: [Senior Project Proposal EE198A]
date: 2026-02-05
author: Eric Reyes
---

# Product Brief: Wireless Glove Interface for Real-Time Robotic Hand Mimicry

## 1. Vision & Problem Statement

### The Problem

The relationship between humans and machines continues to evolve rapidly. Fields like prosthetics, telecommunication, robotics, and virtual reality have made huge strides in functionality and widespread availability. However, **highly articulate, human-like, real-time wireless interfaces remain unattainable at a consumer level**.

Current solutions fall into two extremes:
- **Consumer-grade** (VR controllers): Trade affordability for quality and accuracy
- **Commercial robotic hands**: Exceptional performance but too complex and expensive for average users

### The Gap

Many consumer products focus on gesture controls rather than joint-level motion. Systems with joint control either:
- Are too simple and not accurate enough, OR
- Are too complex and advanced for consumer needs

### Our Vision

Create a **functioning control system for a robotic hand** using remote sensing of the human hand that:
- Provides improved precision and control
- Maintains affordability and accessibility
- Uses joint-level flex sensing for higher-resolution input
- Employs FOC-driven BLDC actuators for smooth, lifelike output
- All within a low-cost, modular design achievable with student-level resources

---

## 2. Target Users

### Primary User: Research & Education

| Attribute | Description |
|-----------|-------------|
| **Who** | Engineering students, robotics researchers, hobbyists |
| **Context** | Learning motor control, studying human-robot interaction |
| **Pain Points** | Expensive commercial systems, complex tendon-driven DIY projects |
| **Success Metric** | Can replicate finger movements with visible accuracy |

### Secondary User: Prosthetics Prototyping

| Attribute | Description |
|-----------|-------------|
| **Who** | Prosthetics researchers, rehabilitation engineers |
| **Context** | Rapid prototyping of control interfaces |
| **Pain Points** | High barrier to entry for testing control schemes |
| **Success Metric** | Platform enables quick iteration on control algorithms |

### Tertiary User: Telepresence / Remote Operation

| Attribute | Description |
|-----------|-------------|
| **Who** | Remote operators, hazardous environment workers |
| **Context** | Operating robotic hands at a distance |
| **Pain Points** | Latency, lack of precision in existing solutions |
| **Success Metric** | Natural feeling control with minimal training |

---

## 3. Success Metrics

### Must-Have (MVP)

| Metric | Target | Rationale |
|--------|--------|-----------|
| **End-to-end latency** | <100ms | Feels responsive to user |
| **Position accuracy** | ±5° | Visibly accurate mimicry |
| **Joints controlled** | 4 per finger (1 finger min) | Demonstrates joint-level control |
| **Wireless range** | >2m | Practical desktop use |
| **Runtime** | >30 min | Usable demo duration |

### Should-Have (Full Demo)

| Metric | Target | Rationale |
|--------|--------|-----------|
| **Fingers controlled** | 5 (full hand) | Complete hand mimicry |
| **Latency** | <50ms | Near-imperceptible delay |
| **Runtime** | >2 hours | Extended operation |
| **Reconnection** | <5 seconds | Seamless recovery |

### Nice-to-Have (Stretch)

| Metric | Target | Rationale |
|--------|--------|-----------|
| **Force feedback** | Basic haptics | Bidirectional interaction |
| **Grip detection** | Contact sensing | Object manipulation awareness |

---

## 4. Scope Definition

### In Scope (MVP)

- ✅ Wearable glove with flex sensors at finger joints
- ✅ Wireless data transmission via Bluetooth LE
- ✅ 3D-printed robotic hand structure
- ✅ BLDC motors with FOC control at each joint
- ✅ Real-time position mimicry
- ✅ Calibration routines for sensors and motors
- ✅ Basic documentation and demo

### In Scope (Full Project)

- ✅ All five fingers with joint-level control
- ✅ Encoder feedback for closed-loop control
- ✅ Battery power system
- ✅ Performance test suite
- ✅ Final report and presentation

### Out of Scope

- ❌ Force/torque sensing and feedback
- ❌ Tactile/haptic feedback to user
- ❌ Mobile app interface
- ❌ Cloud connectivity
- ❌ Machine learning for gesture recognition
- ❌ Production-ready enclosure
- ❌ Multi-hand synchronization

---

## 5. Key Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| FOC complexity on ESP32 | Medium | High | Use proven SimpleFOC library; start with single motor |
| Motor heat under load | Medium | Medium | Limit duty cycle; add thermal monitoring |
| BLE latency spikes | Low | High | Optimize connection interval; implement interpolation |
| 3D print tolerances | Medium | Medium | Iterative design; test fit early |
| Flex sensor drift | Medium | Low | Implement runtime recalibration |
| Integration timeline | High | High | Parallel workstreams; weekly integration tests |

---

## 6. Timeline Alignment

| Phase | Dates | Deliverable |
|-------|-------|-------------|
| Research | Aug–Dec 2025 | ✅ Proposal, literature review |
| Code Prototyping | Jan 1 – Feb 3, 2026 | FOC codebase, sensor reading |
| Hardware Prototyping | Feb 4 – Mar 18, 2026 | Working motor + glove prototypes |
| Integration | Mar 19 – Apr 30, 2026 | Full system demonstration |
| Documentation | May 1–15, 2026 | Final report + poster |

---

## 7. Open Questions for PRD

1. **Motor selection**: Which specific BLDC motor model? (size, torque, cost)
2. **Driver board**: Off-the-shelf (SimpleFOC Shield) vs custom PCB?
3. **Communication protocol**: Raw angle values or encoded/compressed?
4. **Calibration UX**: Automatic vs guided user calibration?
5. **Power budget**: Separate battery for motors vs shared?

---

## Approval

| Role | Name | Date |
|------|------|------|
| Product Owner | Eric Reyes | 2026-02-05 |
| Technical Lead | TBD | |
| Advisor | Junaid Anwar | |
