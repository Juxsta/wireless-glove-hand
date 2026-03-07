/**
 * MLX90395 Hall-Effect Magnetic Sensor Driver
 * 
 * 3-axis magnetometer via SPI for finger joint angle tracking.
 * Original implementation by Antonio Rojas (EE198B).
 * Extracted into reusable driver by Eric Reyes.
 * 
 * Usage:
 *   MLX90395 sensor(CS_PIN);
 *   sensor.init();
 *   sensor.startBurst();
 *   
 *   loop() {
 *     RawMag mag = sensor.readMeasurement();
 *     if (mag.valid) {
 *       float angle = sensor.calculateAngle(mag);
 *     }
 *   }
 * 
 * Hardware: ESP32 VSPI bus (SCK=18, MISO=19, MOSI=23)
 */

#ifndef MLX90395_H
#define MLX90395_H

#include <Arduino.h>
#include <SPI.h>

// =============================================================================
// MLX90395 Commands
// =============================================================================

#define MLX_CMD_START_BURST 0x1E  // Start burst mode (continuous measurement)
#define MLX_CMD_READ_MEAS   0x4E  // Read measurement (X, Y, Z)
#define MLX_CMD_EXIT        0x80  // Exit mode
#define MLX_CMD_NOP         0x00  // No operation

// =============================================================================
// Data Structures
// =============================================================================

struct RawMag {
    int16_t x;      // X-axis raw magnetometer reading
    int16_t y;      // Y-axis raw magnetometer reading
    int16_t z;      // Z-axis raw magnetometer reading
    bool valid;     // True if read was successful
};

struct MagCalibration {
    int16_t x_offset;
    int16_t y_offset;
    int16_t z_offset;
};

// =============================================================================
// MLX90395 Driver Class
// =============================================================================

class MLX90395 {
public:
    /**
     * Constructor
     * @param csPin Chip select GPIO pin
     */
    MLX90395(uint8_t csPin) : _csPin(csPin) {
        _cal.x_offset = 0;
        _cal.y_offset = 0;
        _cal.z_offset = 0;
    }

    /**
     * Initialize SPI and sensor
     * @param spiFreq SPI clock frequency (default 1 MHz)
     */
    void init(uint32_t spiFreq = 1000000) {
        pinMode(_csPin, OUTPUT);
        digitalWrite(_csPin, HIGH);
        
        // Initialize SPI (VSPI on ESP32)
        SPI.begin(18, 19, 23, _csPin);  // SCK, MISO, MOSI, CS
        
        _spiSettings = SPISettings(spiFreq, MSBFIRST, SPI_MODE0);
        
        delay(10);  // Allow sensor to power up
        
        // Exit any existing mode
        sendCommand(MLX_CMD_EXIT);
        delay(5);
    }

    /**
     * Start burst mode (continuous measurement)
     * Must be called before readMeasurement()
     */
    bool startBurst() {
        uint8_t status = sendCommand(MLX_CMD_START_BURST);
        delay(10);  // Allow burst mode to stabilize
        return (status == 0x00);  // Check if command succeeded
    }

    /**
     * Exit burst mode
     */
    void exitBurst() {
        sendCommand(MLX_CMD_EXIT);
        delay(5);
    }

    /**
     * Read magnetometer measurement (X, Y, Z)
     * Sensor must be in burst mode (call startBurst() first)
     * @return RawMag struct with x, y, z readings and validity flag
     */
    RawMag readMeasurement() {
        RawMag mag;
        mag.valid = false;

        SPI.beginTransaction(_spiSettings);
        digitalWrite(_csPin, LOW);
        
        // Send read command
        SPI.transfer(MLX_CMD_READ_MEAS);
        
        // Read 6 bytes (2 bytes per axis: X, Y, Z)
        uint8_t data[6];
        for (int i = 0; i < 6; i++) {
            data[i] = SPI.transfer(0x00);
        }
        
        digitalWrite(_csPin, HIGH);
        SPI.endTransaction();

        // Parse 16-bit signed integers (big-endian)
        mag.x = (int16_t)((data[0] << 8) | data[1]);
        mag.y = (int16_t)((data[2] << 8) | data[3]);
        mag.z = (int16_t)((data[4] << 8) | data[5]);
        mag.valid = true;

        return mag;
    }

    /**
     * Calculate angle from magnetometer reading using atan2
     * @param mag Raw magnetometer reading
     * @param applyCalibration If true, subtract calibration offsets
     * @return Angle in degrees (0-360)
     */
    float calculateAngle(const RawMag& mag, bool applyCalibration = true) {
        if (!mag.valid) return 0.0f;

        // Apply calibration (baseline subtraction)
        int16_t x = mag.x;
        int16_t y = mag.y;
        
        if (applyCalibration) {
            x -= _cal.x_offset;
            y -= _cal.y_offset;
        }

        // Calculate angle using atan2 (returns -PI to PI)
        float angle_rad = atan2(y, x);
        
        // Convert to degrees (0-360)
        float angle_deg = angle_rad * 180.0f / PI;
        if (angle_deg < 0) {
            angle_deg += 360.0f;
        }

        return angle_deg;
    }

    /**
     * Calibrate sensor (record baseline magnetic field)
     * @param numSamples Number of samples to average (default 20)
     */
    void calibrate(int numSamples = 20) {
        Serial.println("MLX90395: Starting calibration...");
        Serial.println("Hold sensor in baseline position (e.g., hand flat)");
        
        long sumX = 0, sumY = 0, sumZ = 0;
        int validSamples = 0;

        for (int i = 0; i < numSamples; i++) {
            RawMag mag = readMeasurement();
            if (mag.valid) {
                sumX += mag.x;
                sumY += mag.y;
                sumZ += mag.z;
                validSamples++;
            }
            delay(50);  // 20 Hz sampling
        }

        if (validSamples > 0) {
            _cal.x_offset = sumX / validSamples;
            _cal.y_offset = sumY / validSamples;
            _cal.z_offset = sumZ / validSamples;
            
            Serial.printf("MLX90395: Calibration complete - Offsets: X=%d, Y=%d, Z=%d\n",
                _cal.x_offset, _cal.y_offset, _cal.z_offset);
        } else {
            Serial.println("MLX90395: Calibration failed - no valid samples");
        }
    }

    /**
     * Set calibration offsets manually
     */
    void setCalibration(int16_t x_offset, int16_t y_offset, int16_t z_offset) {
        _cal.x_offset = x_offset;
        _cal.y_offset = y_offset;
        _cal.z_offset = z_offset;
    }

    /**
     * Get current calibration
     */
    MagCalibration getCalibration() const {
        return _cal;
    }

private:
    uint8_t _csPin;
    SPISettings _spiSettings;
    MagCalibration _cal;

    /**
     * Send a command to the MLX90395
     * @param cmd Command byte
     * @return Status byte from sensor
     */
    uint8_t sendCommand(uint8_t cmd) {
        SPI.beginTransaction(_spiSettings);
        digitalWrite(_csPin, LOW);
        
        uint8_t status = SPI.transfer(cmd);
        
        digitalWrite(_csPin, HIGH);
        SPI.endTransaction();
        
        return status;
    }
};

#endif // MLX90395_H
