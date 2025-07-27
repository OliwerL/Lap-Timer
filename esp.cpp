#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

/* ---------- Ultrasonic ----------- */
const int trigPin = 4;
const int echoPin = 16;
#define SOUND_SPEED       0.034f        // cm / µs
const float THRESHOLD_ENTER = 100.0f;   // wjazd
const float THRESHOLD_EXIT  = 120.0f;   // wyjazd (histereza)

/* ---------- BLE UUIDs ------------ */
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

/* ---------- BLE objects ---------- */
BLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;

/* ---------- Lap timing ----------- */
const uint8_t NUM_LAPS = 4;
float   lapTimes[NUM_LAPS] = {0.0f};
uint8_t currentLap  = 0;
unsigned long lapStartMs = 0;

bool armed  = false;   // czy liczymy okrążenia
bool inside = false;   // auto < THRESHOLD_ENTER?
bool test = false;

/* ------------------------------------------------------------------ */
/* Pomiar odległości                                                  */
/* ------------------------------------------------------------------ */
float measureDistanceCm() {
  digitalWrite(trigPin, LOW);  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long dur = pulseIn(echoPin, HIGH, 30000);        // 30 ms ≈ 5 m
  if (dur == 0) return 10000.0f;                   // brak echa
  return (dur * SOUND_SPEED) / 2.0f;
}

/* ------------------------------------------------------------------ */
void sendLaps() {
  if (!deviceConnected) return;

  char buf[64];
  snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f,%.2f",
           lapTimes[0], lapTimes[1], lapTimes[2], lapTimes[3]);
  pTxCharacteristic->setValue(buf);
  pTxCharacteristic->notify();
}

/* ------------------------------------------------------------------ */
/* BLE callbacks                                                       */
/* ------------------------------------------------------------------ */
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) { deviceConnected = true; }
  void onDisconnect(BLEServer* p) {
    deviceConnected = false;
    delay(500);
    p->startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) {
    String cmd = ch->getValue();

    if (cmd == "reset") {                      // zeruj i uzbrój
      memset(lapTimes, 0, sizeof(lapTimes));
      currentLap  = 0;
      lapStartMs  = 0;
      inside      = false;
      armed       = true;
      test        = false;
      sendLaps();
      Serial.println("RESET → armed = true");
      return;
    }
    else if (cmd == "stop") {                       // przerwij pomiar
      armed  = false;
      inside = false;
      Serial.println("STOP → armed = false");
      return;
    }
    else if (cmd == "test") {                       // zeruj i uzbrój w testowa wersje
      memset(lapTimes, 0, sizeof(lapTimes));
      currentLap  = 0;
      lapStartMs  = 0;
      inside      = false;
      armed       = true;
      test        = true;
      sendLaps();
      Serial.println("RESET → armed = true");
      return;
    }
    else if (cmd == "ping") {
      pTxCharacteristic->setValue("pong");
      pTxCharacteristic->notify();
    }
    else return;
  }
};

/* ------------------------------------------------------------------ */
void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  BLEDevice::init("ESP32-S3 LapTimer");
  auto* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  auto* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  auto* pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxCharacteristic->setCallbacks(new RxCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("LapTimer ready (waiting for 'reset')");
}

/* ------------------------------------------------------------------ */
void loop() {
  /* --------- 1. Pomiar odległości --------- */
  float dist = measureDistanceCm();

  /* --------- 2. Loguj dystans co 100 ms ---- */
  static unsigned long lastDistLog = 0;
  const unsigned long distLogInterval = 100;          // ms
  if (millis() - lastDistLog >= distLogInterval) {
    Serial.printf("dist: %.1f cm\n", dist);
    lastDistLog = millis();
  }

  /* --------- 3. Logika okrążeń ------------- */
  if (armed) {
    if (!inside && dist < THRESHOLD_ENTER) {
      inside = true;
      unsigned long now = millis();

      if (lapStartMs == 0) {
        lapStartMs = now;
        pTxCharacteristic->setValue("start");
        pTxCharacteristic->notify();                             // start 1. okr.
      } else {
        if (currentLap < NUM_LAPS)
        {
          float lapSec = (now - lapStartMs) / 1000.0f;  // koniec poprzedniego
          lapTimes[currentLap++] = lapSec;
          lapStartMs = now; 
        }
        else if (test)
        {
          float lapSec = (now - lapStartMs) / 1000.0f;  // koniec poprzedniego
          lapTimes[0] = lapTimes[1];
          lapTimes[1] = lapTimes[2];
          lapTimes[2] = lapTimes[3];
          lapTimes[3] = lapSec;
          lapStartMs = now; 
        }
                            // start kolejnego
      }
    }
    else if (inside && dist > THRESHOLD_EXIT) {
      inside = false;
    }
    if (test == false)
      {    
        if (currentLap >= NUM_LAPS) {                     // 4 okrążenia → stop
          armed = false;
          pTxCharacteristic->setValue("end");
          pTxCharacteristic->notify(); 
          Serial.println("4 laps done – armed = false");
      }
    }
  } 

  /* --------- 4. Wyślij BLE co 500 ms -------- */
  static unsigned long lastSent = 0;
  if (millis() - lastSent >= 500) {
    sendLaps();
    lastSent = millis();

    Serial.printf("laps: %.2f  %.2f  %.2f  %.2f\n",
                  lapTimes[0], lapTimes[1], lapTimes[2], lapTimes[3]);
  }

  delay(10);
}
