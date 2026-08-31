/*
 * ESP32_Gateway.ino
 * -----------------
 * Bridges the mobile app (Bluetooth SPP) and the remote relay/sensor node (LoRa).
 *
 *  Mobile app --BT--> [this board] --LoRa--> Relay/Sensor node
 *  Mobile app <--BT-- [this board] <--LoRa-- Relay/Sensor node
 *
 * Library required: "LoRa" by sandeepmistry
 *   (Arduino IDE: Library Manager -> search "LoRa" by sandeepmistry)
 *
 */

#include <SPI.h>
#include <LoRa.h>
#include <BluetoothSerial.h>

#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   14
#define LORA_DIO0  26
#define LORA_BAND  433E6   
#define LORA_SYNC_WORD 0xF3   // must match on both boards

BluetoothSerial SerialBT;

String lastVoltage = "0";
String lastCurrent = "0";

void setup() {
  Serial.begin(115200);

  SerialBT.begin("ESP-Monitoring");   
  Serial.println("Bluetooth ready, waiting for mobile app...");

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
  forwardAppCommandsToNode();
  forwardNodeTelemetryToApp();
}

// Mobile app -> Node
void forwardAppCommandsToNode() {
  if (!SerialBT.available()) return;

  char c = SerialBT.read();
  if (c != '1' && c != '0') return;

  LoRa.beginPacket();
  LoRa.print("CMD:");
  LoRa.print(c);
  LoRa.endPacket();

  Serial.printf("Command '%c' -> node\n", c);
}

// Node -> Mobile app
void forwardNodeTelemetryToApp() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  String received;
  while (LoRa.available()) received += (char)LoRa.read();

  if (!received.startsWith("DATA:")) return;

  String payload = received.substring(5);
  int comma = payload.indexOf(',');
  if (comma <= 0) return;

  lastVoltage = payload.substring(0, comma);
  lastCurrent = payload.substring(comma + 1);

  String out = lastVoltage + ":" + lastCurrent;
  SerialBT.println(out);
  Serial.println("-> app: " + out);
}
