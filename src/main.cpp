#include <Arduino.h>
#include "BluetoothSerial.h"

HardwareSerial SerialESC1(2);
#define RXD2 16 //esp32 -> rx2

HardwareSerial SerialESC2(1);
#define RXD1 18  // Change to your actual pin -> D18

//dont forget common gnd! gnd-> gnd

BluetoothSerial SerialBT;
#define BT_DEVICE_NAME "ESC_Telemetry" //changes name of the bt device!

struct ESC_Data {
  uint8_t  temp;
  uint16_t voltage;
  uint16_t current;
  uint16_t consumption;
  uint16_t erpm;
  bool     valid;
};

ESC_Data tlm1, tlm2;
uint8_t buf1[10], buf2[10];


//this stuff checks that you have everything before sending it out -refer to kiss telemetry pdf file
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
//end of thing


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


//this is for debuggging
void broadcastPrint(const char* msg) {
  Serial.print(msg);
  if (SerialBT.connected()) {
    SerialBT.print(msg);
  }
}


//setup once
void setup() {
  Serial.begin(115200);
  SerialESC1.begin(115200, SERIAL_8N1, RXD2, -1, false);
  SerialESC2.begin(115200, SERIAL_8N1, RXD1, -1, false);

  SerialBT.begin(BT_DEVICE_NAME);

  tlm1.valid = false;
  tlm2.valid = false;

  Serial.println("Dual ESC Telemetry Online"); //change if u want
  Serial.printf("Bluetooth device ready: \"%s\"\n", BT_DEVICE_NAME);
}


//main loop
void loop() {
  //checks if the telemetry from esc is actually sending sutff
  if (SerialESC1.available() >= 10) {
    SerialESC1.readBytes(buf1, 10);
    parseESC(buf1, tlm1);    // <--- 
  }

  if (SerialESC2.available() >= 10) {
    SerialESC2.readBytes(buf2, 10);
    parseESC(buf2, tlm2);
  }

//dont change lol


//serial studio: just set it up to dis
//first 5 values coorespond to esc1
//next 5 values is esc2
  if (tlm1.valid && tlm2.valid) {
    char msg[128];
    snprintf(msg, sizeof(msg), "%d,%.2f,%.2f,%d,%d,%d,%.2f,%.2f,%d,%d\n",
             tlm1.temp,
             tlm1.voltage / 100.0,
             tlm1.current / 100.0,
             tlm1.consumption,
             tlm1.erpm * 100,
             tlm2.temp,
             tlm2.voltage / 100.0,
             tlm2.current / 100.0,
             tlm2.consumption,
             tlm2.erpm * 100);

    broadcastPrint(msg); 
    //bluetooth send
    tlm1.valid = false;
    tlm2.valid = false;
  }
}