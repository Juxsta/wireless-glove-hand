# Wireless Glove Control System — Project Context

## Project Pivot (April 29, 2026)

**Original scope:** Custom robotic hand + glove + wireless control (BLDC/FOC/ESP-NOW)
**New scope:** Wireless glove control system applied to any open-source robotic arm

### Why the pivot
The custom robotic hand (BLDC motors, DRV8300 drivers, FOC control) proved too complex to build in the remaining time. The core innovation is the wireless sensing + control pipeline, not the specific robotic hand hardware.

### What we're delivering now
1. Glove with flex sensors -- reads finger/joint positions (switched from MLX90395 Hall-effect to flex sensors)
2. ESP-NOW wireless link -- transmits joint angles in real-time (~50Hz)
3. Control system on ESP32 -- receives sensor data, maps to motor commands
4. Open-source robotic arm -- demo platform (servo-based, sourced from open-source projects)

### Key change: Sensors
- Before: MLX90395 Hall-effect magnetic sensors (SPI, 3 per finger)
- Now: Flex sensors -- simpler, analog read, one per finger joint

## Team
- Antonio Rojas: Robotic arm hardware / 3D printing
- Raul Hernandez-Solis: Motor driver / PCB design
- Matthew Men: Wireless communication / motor sourcing
- Eric Reyes: 3D printing / CAD / firmware

Advisor: Dr. Birsen Sirkeci

## Upcoming Deadlines
- Symposium Poster: Due May 4 (submit PPTX before noon). Must use provided template, Spartan colors. Printed on 36x48 tri-fold board. Advisor must approve.
- Final Report: Due May 12
- Meeting Log + Student Survey: Due May 12
- Symposium Presentation: Date TBD (confirm with Dr. Sirkeci)

## TODO for this work session
- [ ] Update glove firmware for flex sensor input (analog read instead of SPI/MLX90395)
- [ ] Update hand/arm firmware for servo control (instead of FOC/BLDC)
- [ ] Find and integrate an open-source robotic arm design
- [ ] Update README to reflect pivot
- [ ] Start poster content
