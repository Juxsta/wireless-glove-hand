# Prototyping

Early-stage test code and proof-of-concept experiments. These are not production firmware — they exist to validate individual subsystems before integration.

## Contents

### esp-now-test/
ESP-NOW wireless communication validation between two ESP32 units.
- **Author:** Matthew Men
- **Status:** ✅ Validated — bidirectional communication confirmed

### sensor-test/
MLX90395 Hall-effect sensor SPI communication and angle reading.
- **Author:** Antonio Rojas
- **Status:** Awaiting sensor delivery

### motor-test/
BLDC motor + AS5600 encoder basic spin test.
- **Author:** Matthew Men
- **Status:** Awaiting DRV8300 PCB delivery
