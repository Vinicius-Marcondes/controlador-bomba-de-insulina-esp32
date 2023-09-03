#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ESP32Servo.h>

#define SERVICE_UUID "f69317b5-a6b2-4cf4-89e6-9c7d98be8891"
#define CHARACTERISTIC_UUID "2ec829c3-efad-4ba2-8ce1-bad71b1040f7"

#define SERVO_PIN 26

Servo servo;

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        int value = std::stoi(pCharacteristic->getValue());
        Serial.print("Recebido: ");
        Serial.println(value);
        servo.write(value);
        delay(2000);
        servo.write(0);
        delay(2000);
    }
};

void setup() {
    Serial.begin(9600);
    Serial.println("Configurando bluethooth...");
    BLEDevice::init("FreeFlow Insulin Pump");
    BLEServer *pServer = BLEDevice::createServer();

    BLEService *pService = pServer->createService(SERVICE_UUID);

    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

    pCharacteristic->setCallbacks(new MyCallbacks());

    pCharacteristic->setValue("Pump ready!");
    pService->start();

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();

    Serial.println("Configurando servo motor...");
    servo.setPeriodHertz(50);  // Standard 50hz servo
    servo.attach(SERVO_PIN);
}

void loop() {
    delay(2000);
}
