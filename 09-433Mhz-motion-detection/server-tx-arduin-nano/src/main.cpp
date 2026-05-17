#include <Arduino.h>
#include <RCSwitch.h>
#include <Wire.h>

/*
MPU-9265  Arduino Nano
VCC       3.3V
GND       GND
SDA       A4
SCL       A5
*/

#define pinLed 4
#define pinTransmitter 2

RCSwitch mySwitch = RCSwitch();
bool isDetected = false;


// MPU-9265 I2C address
#define MPU_ADDR 0x68

// MPU-9265 registers
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_WHO_AM_I 0x75

// Threshold for movement detection (tune as needed)
const int threshold = 10000;

// Three-axis baseline values for resting position
int16_t baseX, baseY, baseZ;

/**
 * @brief Write a byte to a specific register on the MPU-9265
 *
 * @param reg
 * @param value
 */
void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

/**
 * @brief Read accelerometer data from the MPU-9265 and return it as three 16-bit integers
 *
 * @param x
 * @param y
 * @param z
 */
void readAccel(int16_t& x, int16_t& y, int16_t& z) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6);

    // MPU-9265 data is big-endian (high byte first)
    x = (Wire.read() << 8) | Wire.read();
    y = (Wire.read() << 8) | Wire.read();
    z = (Wire.read() << 8) | Wire.read();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting ...");

    pinMode(pinLed, OUTPUT);

    mySwitch.enableTransmit(pinTransmitter);

    Wire.begin();

    // Wake up MPU-9265 (clear sleep bit)
    writeRegister(REG_PWR_MGMT_1, 0x00);
    // Set accelerometer range to +/-2g
    writeRegister(REG_ACCEL_CONFIG, 0x00);

    delay(100);

    randomSeed(analogRead(A0));

    // Read baseline (resting position)
    readAccel(baseX, baseY, baseZ);
    Serial.print("Baseline - X: ");
    Serial.print(baseX);
    Serial.print(" Y: ");
    Serial.print(baseY);
    Serial.println(" Z: ");
    Serial.println(baseZ);

    Serial.println("Setup complete");
}

void loop() {
    int16_t x, y, z;
    readAccel(x, y, z);

    // Calculate difference from baseline
    int16_t dx = x - baseX;
    int16_t dy = y - baseY;
    int16_t dz = z - baseZ;

    // Use sum of absolute differences as movement magnitude
    int magnitude = abs(dx) + abs(dy) + abs(dz);

    if (magnitude > threshold && !isDetected) {
        isDetected = true;

        Serial.print("Movement detected! Magnitude: ");
        Serial.println(magnitude);
        digitalWrite(pinLed, HIGH);

        unsigned long code = random(0, 16777216UL); // random 24-bit value

        Serial.print("Sending code: ");
        Serial.println(code);

        mySwitch.send(code, 24);

    } else if (magnitude <= threshold && isDetected) {
        isDetected = false;

        Serial.println("No movement");
        digitalWrite(pinLed, LOW);
    }

    delay(50);
}