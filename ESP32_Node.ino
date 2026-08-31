/*
 * ESP32_Node.ino
 * --------------
 * Drives the relay and measures voltage/current on the switched load.
 *
 *  Gateway --LoRa--> [this board] -> relay ON/OFF
 *  Gateway <--LoRa-- [this board] <- voltage & current readings
 *
 * Libraries required:
 *   - "LoRa" by sandeepmistry
 *   - "ACS712" by Rob Tillaart
 *   - "ZMPT101B"
 */

#include <SPI.h>
#include <LoRa.h>
#include <ZMPT101B.h>
#include <ACS712.h>

#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   14
#define LORA_DIO0  26
#define LORA_BAND  433E6      // must match the gateway
#define LORA_SYNC_WORD 0xF3   // must match on both boards

// ---- Relay ----
// GPIO0 is a boot-strapping pin on ESP32 (pulled low = flash-mode at boot).
#define RELAY_PIN 5
#define RELAY_ON  LOW     // most relay boards are active-LOW - flip if yours isn't
#define RELAY_OFF HIGH

// ---- Sensors ----
#define VOLTAGE_SENSOR_PIN 2
#define CURRENT_SENSOR_PIN 4
#define MAINS_FREQUENCY_HZ 50.0f
#define VOLTAGE_SENSITIVITY 500.0f 
#define VOLTAGE_CALIBRATION 1.49f    // e.g. 230 / measured155 = 1.49
#define CURRENT_OFFSET_mA 0.0f       // subtract any DC/zero-current offset

ZMPT101B voltageSensor(VOLTAGE_SENSOR_PIN, MAINS_FREQUENCY_HZ);
ACS712 currentSensor(CURRENT_SENSOR_PIN, 3.3, 4096, 66); // pin, Vref, ADC res, mV/A

unsigned long lastReportMs = 0;
const unsigned long REPORT_INTERVAL_MS = 2000;

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF); 

  voltageSensor.setSensitivity(VOLTAGE_SENSITIVITY);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa init failed - check wiring/frequency.");
    while (true) delay(1000);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  Serial.println("LoRa ready.");
}

void loop() {
  handleCommandsFromGateway();

  if (millis() - lastReportMs >= REPORT_INTERVAL_MS) {
    lastReportMs = millis();
    reportTelemetry();
  }
}

// Gateway -> relay
void handleCommandsFromGateway() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  String received;
  while (LoRa.available()) received += (char)LoRa.read();

  if (!received.startsWith("CMD:")) return;

  char c = received.charAt(4);
  if (c == '1') {
    digitalWrite(RELAY_PIN, RELAY_ON);
    Serial.println("Relay ON");
  } else if (c == '0') {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println("Relay OFF");
  }
}

// Sensors -> gateway
void reportTelemetry() {
  float voltage = readVoltage();
  float current = readCurrent();

  LoRa.beginPacket();
  LoRa.print("DATA:");
  LoRa.print(voltage, 1);
  LoRa.print(",");
  LoRa.print(current, 2);
  LoRa.endPacket();

  Serial.printf("-> gateway  V: %.1f  I: %.2fmA\n", voltage, current);
}

float readVoltage() {
  return voltageSensor.getRmsVoltage() * VOLTAGE_CALIBRATION;
}

float readCurrent() {
  const int samples = 100;
  float total = 0;
  for (int i = 0; i < samples; i++) {
    total += currentSensor.mA_AC();
  }
  return abs(total / samples) - CURRENT_OFFSET_mA;  // milliamps
}
