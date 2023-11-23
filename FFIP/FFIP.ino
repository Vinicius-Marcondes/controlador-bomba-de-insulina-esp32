#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAddress.h>
#include <BLE2902.h>
#include <ESP32Servo.h>

#define SERVICE_UUID "f69317b5-a6b2-4cf4-89e6-9c7d98be8891"
#define CHARACTERISTIC_UUID "2ec829c3-efad-4ba2-8ce1-bad71b1040f7"
#define PUMP_STATUS_CHARACTERISTIC_UUID "1cd909de-3a8e-43e1-a492-82917ab0b662"

#define BUZZZER_PIN 18
#define SERVO_PIN 33

Servo servo;

BLEServer* pServer = NULL;
BLECharacteristic* pPumpStatusCharacteristic = NULL;
BLECharacteristic* pInsulinCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

uint32_t status = 0;

RTC_DATA_ATTR int stock = 50;

TaskHandle_t Task1;
TaskHandle_t Task2;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Conectado!");
    deviceConnected = true;
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer* pServer) {
    Serial.println("desconectado!");
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    if (pPumpStatusCharacteristic->getValue() == "0") {
        Serial.println("Bloqueio bomba");
        pPumpStatusCharacteristic->setValue("1");
        pPumpStatusCharacteristic->notify();

        int value = std::stoi(pCharacteristic->getValue()) * 5;
        Serial.print("Recebido: ");
        Serial.println(value);

        Serial.println("aplicando insulina");
        for (int i = 0; i <= value; i += 5) {
          servo.write(i);
          delay(1000);
        }
        servo.write(0);
        delay(1000);

        pPumpStatusCharacteristic->setValue("0");
        pPumpStatusCharacteristic->notify();
        delay(1000);
        Serial.println("bomba liberada");
      } else {
        Serial.println("Bomba ocupada");
      }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Configurando bluethooth...");

  BLEDevice::init("FreeFlowInsulinPump-esp32-v2");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pInsulinCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pInsulinCharacteristic->setCallbacks(new MyCallbacks());
  pInsulinCharacteristic->addDescriptor(new BLE2902());

  pPumpStatusCharacteristic = pService->createCharacteristic(
    PUMP_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  pService->start();

  BLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  pPumpStatusCharacteristic->setValue("0");
  pPumpStatusCharacteristic->notify();

  xTaskCreatePinnedToCore(
    sendPumpStatus,    /* Task function. */
    "send pup status", /* name of task. */
    10000,             /* Stack size of task */
    NULL,              /* parameter of the task */
    1,                 /* priority of the task */
    &Task1,            /* Task handle to keep track of created task */
    0);                /* pin task to core 0 */
  delay(500);

  Serial.println("Configurando servo motor...");
  servo.setPeriodHertz(50);  // Standard 50hz servo
  servo.attach(SERVO_PIN);
  servo.write(0);
}

void sendPumpStatus(void* pvParameters) {
  for (;;) {
    pPumpStatusCharacteristic->notify();
    Serial.println("core 1 notify");
    delay(2000);
  }
}

void loop() {
  delay(1000);
}