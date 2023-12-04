#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ESP32Servo.h>
#include <Preferences.h>

#define SERVICE_UUID "f69317b5-a6b2-4cf4-89e6-9c7d98be8891"
#define CHARACTERISTIC_UUID "2ec829c3-efad-4ba2-8ce1-bad71b1040f7"
#define PUMP_STATUS_CHARACTERISTIC_UUID "1cd909de-3a8e-43e1-a492-82917ab0b662"
#define PUMP_STOCK_CHARACTERISTIC_UUID "00324946-0c86-448e-b82b-ceb07b9e535e"

#define uS_TO_S_FACTOR 1000000  // Fator de conversão de micro segundos para segundos
#define TIME_TO_SLEEP  15

#define STATIC_PIN 123456

#define SERVO_PIN 33
#define LED_BUILTIN 2

Preferences preferences;

Servo servo;

BLEServer* pServer = nullptr;
BLECharacteristic* pPumpStatusCharacteristic = nullptr;
BLECharacteristic* pInsulinCharacteristic = nullptr;
BLECharacteristic* pStockCharacteristic = nullptr;

uint32_t status = 0;
int pos;

TaskHandle_t statusTask;

void sendPumpStatus(void*);

class FreeFlowServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        Serial.println("Conectado!");
        BLEDevice::startAdvertising();
    }

    void onDisconnect(BLEServer* server) override {
        Serial.println("Desconectado!");
    }
};

class FreeFlowInsulinCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override {
        if (pPumpStatusCharacteristic->getValue() == "0") {
            Serial.println("Bloqueio bomba");
            pPumpStatusCharacteristic->setValue("1");
            pPumpStatusCharacteristic->notify();

            const int value = std::floor(std::stoi(pCharacteristic->getValue()) * 3.5);

            Serial.print("Recebido: ");
            Serial.println(value);

            if (value + pos < 178) {
                for (int i = pos; i <= value; ++i) {
                    Serial.println(i);
                    servo.write(i);
                    pos++;
                    delay(100);
                }
                Serial.println("insulina aplicada");
                preferences.putInt("pos", pos);
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

class FreeFlowStockCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override {
        pos = 0;
        preferences.putUInt("pos", pos);
        for (int i = 0; i < 3; ++i) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(500);
            digitalWrite(LED_BUILTIN, LOW);
            delay(500);
        }
        servo.write(pos);
        status = 0;
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Configurando bluethooth...");

    BLEDevice::init("FreeFlowInsulinPump-esp32-v2");
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new FreeFlowServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);

    pInsulinCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pInsulinCharacteristic->setCallbacks(new FreeFlowInsulinCallbacks());
    pInsulinCharacteristic->addDescriptor(new BLE2902());

    pPumpStatusCharacteristic = pService->createCharacteristic(
            PUMP_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pPumpStatusCharacteristic->addDescriptor(new BLE2902());

    pStockCharacteristic = pService->createCharacteristic(
            PUMP_STOCK_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    pStockCharacteristic->setCallbacks(new FreeFlowStockCallbacks());
    pStockCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_OUT);
    pSecurity->setStaticPIN(STATIC_PIN);

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
    pos = preferences.getInt("pos", 0);

    Serial.println("Configurando servo motor...");
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    servo.setPeriodHertz(50);  // Standard 50hz servo
    servo.attach(SERVO_PIN);
    servo.write(pos);

    Serial.printf("Bomba iniciada na posicao: %d\n", pos);

    pinMode(LED_BUILTIN, OUTPUT);

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

    delay(10000);
}

void sendPumpStatus(void*) {
    while (true) {
        pPumpStatusCharacteristic->notify();
        delay(1000);
    }
}

void loop() {
    if (pServer->getConnectedCount() == 0) {
        Serial.println("Desligando...");
        delay(1000);
        esp_deep_sleep_start();
    }
    delay(100);
}