#include <CAN.h>

#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define MAX_CAN_PAYLOAD 8

// Buffers for Serial TX
char serialBuffer[MAX_CAN_PAYLOAD];
int bufferIndex = 0;

// Volatile variables for safe ISR communication
volatile bool msgReceived = false;
volatile long rxPacketId = 0;
volatile int rxPacketSize = 0;
uint8_t rxDataBuffer[MAX_CAN_PAYLOAD];

void onCANReceive(int packetSize); 

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  CAN.setPins(CAN_RX_PIN, CAN_TX_PIN);

  if (!CAN.begin(500E3)) {
    Serial.println("CAN init failed!");
    while (1);
  }
  Serial.println("CAN bus ready at 500 kbps");
  Serial.println("Type text and press Enter...");

  CAN.onReceive(onCANReceive);
}

void loop() {
  // 1. SAFELY PROCESS INCOMING CAN MESSAGES OUTSIDE ISR
  if (msgReceived) {
    if( rxPacketId == 0x124){
      digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);
      Serial.printf("Received ID: 0x%X  Len: %d  Data: ", rxPacketId, rxPacketSize);
      Serial.printf("\n");
      
      for (int i = 0; i < rxPacketSize; i++) {
        Serial.printf("0x%02X ", rxDataBuffer[i]);
        Serial.print((char)rxDataBuffer[i]);
      }
    }
    Serial.println();
    msgReceived = false; // Clear flag
  }

  // 2. PROCESS SERIAL DATA INPUT
  while (Serial.available() > 0) {
    char incomingByte = Serial.read();

    if (incomingByte == '\n' || incomingByte == '\r') {
      if (bufferIndex > 0) {
        CAN.beginPacket(0x124);
        for (int i = 0; i < bufferIndex; i++) {
          CAN.write(serialBuffer[i]);
        }
        
        if (CAN.endPacket()) {
          Serial.printf("Sent %d bytes over CAN!\n", bufferIndex);
        } else {
          Serial.println("CAN TX Error!");
        }
        bufferIndex = 0;
      }
    } 
    else {
      if (bufferIndex < MAX_CAN_PAYLOAD) {
        serialBuffer[bufferIndex] = incomingByte;
        bufferIndex++;
      }
    }
  }

  // 3. PERIODIC HEARTBEAT FRAME (Every 2 seconds)
  static unsigned long lastTick = 0;
  if (millis() - lastTick > 2000) {
    CAN.beginPacket(0x123);
    CAN.write(0xDE); CAN.write(0xAD); CAN.write(0xBE); CAN.write(0xEF);
    CAN.endPacket();
    lastTick = millis();
  }
}

// Keep the ISR extremely fast and minimal
void onCANReceive(int packetSize) {
  // Guard mirror buffer against overflows
  if (packetSize > MAX_CAN_PAYLOAD) packetSize = MAX_CAN_PAYLOAD;
  
  rxPacketId = CAN.packetId();
  rxPacketSize = packetSize;
  
  for (int i = 0; i < packetSize; i++) {
    if (CAN.available()) {
      rxDataBuffer[i] = CAN.read();
    }
  }
  
  msgReceived = true; // Alert the loop thread safely
}
