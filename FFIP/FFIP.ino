#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAddress.h>
#include <BLE2902.h>
#define BUZZZER_PIN  18
#include <ESP32Servo.h>

#define SERVICE_UUID "f69317b5-a6b2-4cf4-89e6-9c7d98be8891"
#define CHARACTERISTIC_UUID "2ec829c3-efad-4ba2-8ce1-bad71b1040f7"
#define PUMP_STATUS_CHARACTERISTIC_UUID "1cd909de-3a8e-43e1-a492-82917ab0b662"

#define SERVO_PIN 22

Servo servo;

BLEServer *pServer = NULL;
BLECharacteristic* pPumpStatusCharacteristic = NULL;
BLECharacteristic* pInsulinCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

uint32_t status = 0;

class MyServerCallbacks: public BLEServerCallbacks {
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
  void onWrite(BLECharacteristic *pCharacteristic) {
    pPumpStatusCharacteristic->setValue("1");
    pPumpStatusCharacteristic->notify();

    int value = std::stoi(pCharacteristic->getValue()) * 5;
    Serial.print("Recebido: ");
    Serial.println(value);
    
    Serial.println("aplicando insulina");
    for (int i = 0; i <= value; i+=5) {
      servo.write(i);
      delay(1000);
    }
    servo.write(0);
    delay(1000);

    pPumpStatusCharacteristic->setValue("0");
    pPumpStatusCharacteristic->notify();
    delay(1000);
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Configurando bluethooth...");

  BLEDevice::init("FreeFlow Insulin Pump");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pInsulinCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pInsulinCharacteristic->setCallbacks(new MyCallbacks());
  pInsulinCharacteristic->addDescriptor(new BLE2902());
  
  pPumpStatusCharacteristic = pService->createCharacteristic(
    PUMP_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  pPumpStatusCharacteristic->setValue("0");
  pPumpStatusCharacteristic->notify();

  Serial.println("Configurando servo motor...");
  servo.setPeriodHertz(50);  // Standard 50hz servo
  servo.attach(SERVO_PIN);
  servo.write(0);
}

void loop() {
  pPumpStatusCharacteristic->notify();
  delay(1000);
}