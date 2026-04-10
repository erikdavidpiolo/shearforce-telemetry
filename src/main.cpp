#include <Arduino.h>
#include "BluetoothSerial.h"

// ESC 1: UART2
HardwareSerial SerialESC1(2);
#define RXD2 16

// ESC 2: UART1
HardwareSerial SerialESC2(1);
#define RXD1 18  // Change this pin to whatever you're using

BluetoothSerial SerialBT;
#define BT_DEVICE_NAME "ESC_Telemetry"

struct ESC_Data {
  uint8_t  temp;
  uint16_t voltage;
  uint16_t current;
  uint16_t consumption;
  uint16_t erpm;
  bool     valid;  // tracks if we've received good data yet
};

ESC_Data tlm1, tlm2;
uint8_t buf1[10], buf2[10];

uint8_t update_crc8(uint8_t crc, uint8_t crc_seed) {
  uint8_t crc_u = crc ^ crc_seed;
  for (int i = 0; i < 8; i++) {
    crc_u = (crc_u & 0x80) ? 0x7 ^ (crc_u << 1) : (crc_u << 1);
  }
  return crc_u;
}

uint8_t get_crc8(uint8_t *buf, uint8_t size) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < size; i++) crc = update_crc8(buf[i], crc);
  return crc;
}

void parseESC(uint8_t *buffer, ESC_Data &tlm) {
  if (get_crc8(buffer, 9) == buffer[9]) {
    tlm.temp        = buffer[0];
    tlm.voltage     = (buffer[1] << 8) | buffer[2];
    tlm.current     = (buffer[3] << 8) | buffer[4];
    tlm.consumption = (buffer[5] << 8) | buffer[6];
    tlm.erpm        = (buffer[7] << 8) | buffer[8];
    tlm.valid       = true;
  }
}

void broadcastPrint(const char* msg) {
  Serial.print(msg);
  if (SerialBT.connected()) {
    SerialBT.print(msg);
  }
}

void setup() {
  Serial.begin(115200);
  SerialESC1.begin(115200, SERIAL_8N1, RXD2, -1, false);
  SerialESC2.begin(115200, SERIAL_8N1, RXD1, -1, false);

  SerialBT.begin(BT_DEVICE_NAME);

  tlm1.valid = false;
  tlm2.valid = false;

  Serial.println("Dual ESC Telemetry Online");
  Serial.printf("Bluetooth device ready: \"%s\"\n", BT_DEVICE_NAME);
}

void loop() {
  // Read ESC 1
  if (SerialESC1.available() >= 10) {
    SerialESC1.readBytes(buf1, 10);
    parseESC(buf1, tlm1);
  }

  // Read ESC 2
  if (SerialESC2.available() >= 10) {
    SerialESC2.readBytes(buf2, 10);
    parseESC(buf2, tlm2);
  }

  // Only broadcast once both ESCs have sent valid data
  if (tlm1.valid && tlm2.valid) {
    // Combined values:
    // - temp: max of the two (most conservative/safe)
    // - voltage: average (they share the same battery, should be close)
    // - current: sum (total draw from both ESCs)
    // - consumption: sum (total mAh used)
    // - erpm: average (or pick whichever makes sense for your setup)

    uint8_t  combined_temp        = max(tlm1.temp, tlm2.temp);
    float    combined_voltage     = ((tlm1.voltage + tlm2.voltage) / 2.0f) / 100.0f;
    float    combined_current     = (tlm1.current + tlm2.current) / 100.0f;
    uint16_t combined_consumption = tlm1.consumption + tlm2.consumption;
    uint32_t combined_erpm        = ((uint32_t)(tlm1.erpm + tlm2.erpm) / 2) * 100;

    char msg[128];
    snprintf(msg, sizeof(msg), "/*%d,%.2f,%.2f,%d,%lu*/\n",
             combined_temp,
             combined_voltage,
             combined_current,
             combined_consumption,
             combined_erpm);

    broadcastPrint(msg);
  }
}