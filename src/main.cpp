#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAddress.h>
#include <BLE2902.h>
#include <ESP32Servo.h>
#include <Preferences.h>

#define SERVICE_UUID "f69317b5-a6b2-4cf4-89e6-9c7d98be8891"
#define CHARACTERISTIC_UUID "2ec829c3-efad-4ba2-8ce1-bad71b1040f7"
#define PUMP_STATUS_CHARACTERISTIC_UUID "1cd909de-3a8e-43e1-a492-82917ab0b662"

#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  15

#define SERVO_PIN 33
#define LED_BUILTIN 2

Preferences preferences;

Servo servo;

BLEServer* pServer = nullptr;
BLECharacteristic* pPumpStatusCharacteristic = nullptr;
BLECharacteristic* pInsulinCharacteristic = nullptr;

uint32_t status = 0;

RTC_DATA_ATTR int pos = 0;

TaskHandle_t statusTask;

void sendPumpStatus(void*);

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("Conectado!");
        BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
        Serial.println("desconectado!");
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        if (pPumpStatusCharacteristic->getValue() == "0") {
            Serial.println("Bloqueio bomba");
            pPumpStatusCharacteristic->setValue("1");
            pPumpStatusCharacteristic->notify();

            int value = std::floor(std::stoi(pCharacteristic->getValue())* 2.5);
            Serial.print("Recebido: ");
            Serial.println(value);

            if ((value + pos) < 125) {
                Serial.println("aplicando insulina");
                for (int i = pos; i <= value; ++i) {
                    digitalWrite(LED_BUILTIN, HIGH);
                    ++pos;
                    servo.write(i);
                    delay(100);
                    digitalWrite(LED_BUILTIN, LOW);
                }
                preferences.putUInt("counter", pos);

                pPumpStatusCharacteristic->setValue("0");
                pPumpStatusCharacteristic->notify();
                Serial.println("bomba liberada");
            } else {
                Serial.println("Quantidade de insulina maior que o estoque");
                pPumpStatusCharacteristic->setValue("2");
                pPumpStatusCharacteristic->notify();
            }
        } else {
            Serial.println("Bomba ocupada");
        }
        delay(1000);
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
            nullptr,              /* parameter of the task */
            1,                 /* priority of the task */
            &statusTask,            /* Task handle to keep track of created task */
            0);                /* pin task to core 0 */
    delay(500);

    preferences.begin("freeFlow", false);
    pos = preferences.getUInt("stock", 0);


    Serial.println("Configurando servo motor...");
    servo.setPeriodHertz(50);  // Standard 50hz servo
    servo.attach(SERVO_PIN);
    servo.write(pos);

    Serial.printf("Bomba iniciada na posicao: %d", pos);

    pinMode(LED_BUILTIN, OUTPUT);

    while(pServer->getConnectedCount()) {
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
        Serial.println("depois deep sleep");
    }
    delay(10000);

    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +" Seconds");

    esp_deep_sleep_start();
}

void sendPumpStatus(void*) {
    for (;;) {
        pPumpStatusCharacteristic->notify();
        delay(1000);
    }
}

void loop() {
}