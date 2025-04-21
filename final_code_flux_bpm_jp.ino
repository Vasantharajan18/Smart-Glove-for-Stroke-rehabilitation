#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

// WiFi credentials
const char* ssid = "vims";
const char* password = "1234567890";

// UDP configuration
WiFiUDP udp;
const char* udpAddress = "192.168.137.39"; // Change to your receiver IP
const int udpPort = 4321;

// Flex sensor pins (ADC1)
const int thumbPin = 39;
const int indexPin = 34;
const int middlePin = 35;
const int ringPin = 33;
const int pinkyPin = 32;

#define SAMPLE_COUNT 15
#define REPORT_INTERVAL_MS 500

int thumbValues[SAMPLE_COUNT] = {0};
int indexValues[SAMPLE_COUNT] = {0};
int middleValues[SAMPLE_COUNT] = {0};
int ringValues[SAMPLE_COUNT] = {0};
int pinkyValues[SAMPLE_COUNT] = {0};
int bufferIndex = 0;

PulseOximeter pox;
uint32_t tsLastReport = 0;

float calculateAverage(int values[]);
void onBeatDetected() {
  Serial.println("Heartbeat detected!");
}

void setup() {
  Serial.begin(115200);
  WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);

  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);
  Serial.println("UDP started");

  if (!pox.begin()) {
    Serial.println("MAX30100 NOT DETECTED! Check wiring.");
    while (1);
  }

  Serial.println("MAX30100 detected!");
  pox.setOnBeatDetectedCallback(onBeatDetected);
  pox.setIRLedCurrent(MAX30100_LED_CURR_27_1MA);
}

void loop() {
  // Read flex sensors
  thumbValues[bufferIndex] = analogReadMilliVolts(thumbPin);
  indexValues[bufferIndex] = analogReadMilliVolts(indexPin);
  middleValues[bufferIndex] = analogReadMilliVolts(middlePin);
  ringValues[bufferIndex] = analogReadMilliVolts(ringPin);
  pinkyValues[bufferIndex] = analogReadMilliVolts(pinkyPin);

  bufferIndex = (bufferIndex + 1) % SAMPLE_COUNT;

  // Update heart rate sensor
  pox.update();

  if (millis() - tsLastReport > REPORT_INTERVAL_MS) {
    float thumbAvg = calculateAverage(thumbValues) / 1000.0;
    float indexAvg = calculateAverage(indexValues) / 1000.0;
    float middleAvg = calculateAverage(middleValues) / 1000.0;
    float ringAvg = calculateAverage(ringValues) / 1000.0;
    float pinkyAvg = calculateAverage(pinkyValues) / 1000.0;

    float heartRate = pox.getHeartRate();
    if (heartRate < 40 || heartRate > 180) {
      heartRate = 0;
    }

    // Determine finger status
    const char* thumbStatus  = (thumbAvg  < 0.83) ? "CLOSED" : "OPEN";
    const char* indexStatus  = (indexAvg  < 0.94) ? "CLOSED" : "OPEN";
    const char* middleStatus = (middleAvg < 0.22) ? "CLOSED" : "OPEN";
    const char* ringStatus   = (ringAvg   < 0.45) ? "CLOSED" : "OPEN";
    const char* pinkyStatus  = (pinkyAvg  > 0.00) ? "OPEN"   : "UNKNOWN";

    // Print to Serial
    Serial.printf("\n💓 Heart Rate: %.1f bpm\n", heartRate);
    Serial.printf("Thumb  : %.2fV - %s\n", thumbAvg, thumbStatus);
    Serial.printf("Index  : %.2fV - %s\n", indexAvg, indexStatus);
    Serial.printf("Middle : %.2fV - %s\n", middleAvg, middleStatus);
    Serial.printf("Ring   : %.2fV - %s\n", ringAvg, ringStatus);
    Serial.printf("Pinky  : %.2fV - %s\n", pinkyAvg, pinkyStatus);
    Serial.println("-------------------------------------------------");

    // Format UDP packet
    char packet[300];
    snprintf(packet, sizeof(packet),
      "Thumb  : %.2fV - %s\n"
      "Index  : %.2fV - %s\n"
      "Middle : %.2fV - %s\n"
      "Ring   : %.2fV - %s\n"
      "Pinky  : %.2fV - %s\n"
      "Heart Rate : %.1f bpm\n"
      "-------------------------------------------------\n",
      thumbAvg, thumbStatus,
      indexAvg, indexStatus,
      middleAvg, middleStatus,
      ringAvg, ringStatus,
      pinkyAvg, pinkyStatus,
      heartRate
    );

    // Send UDP packet
    udp.beginPacket(udpAddress, udpPort);
    udp.write((const uint8_t*)packet, strlen(packet));
    udp.endPacket();
    Serial.println("UDP Packet Sent!");

    tsLastReport = millis();
  }

  delay(10);
}

float calculateAverage(int values[]) {
  float sum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += values[i];
  }
  return sum / SAMPLE_COUNT;
}
