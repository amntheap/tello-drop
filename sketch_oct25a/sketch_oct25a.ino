#include <Arduino.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==========================================
// PIN & SERVO CONFIGURATION
// ==========================================
#define SERVO_PIN        4   // GPIO 4 for PZ-15320 PWM signal
#define ONBOARD_LED_PIN  8   // Built-in LED on ESP32-S3 Super Mini (usually GPIO 8)

// Adjust these angles based on your physical latch mechanism:
#define SERVO_LOCK_POS   10  // Degrees: Pin inserted (Holding payload)
#define SERVO_DROP_POS  110  // Degrees: Pin retracted (Releasing payload)

// ==========================================
// BLE NORDIC UART UUIDs
// ==========================================
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

Servo dropServo;
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;

// Trigger drop sequence
void triggerDrop() {
    Serial.println("[DROP] Releasing payload...");
    digitalWrite(ONBOARD_LED_PIN, LOW); // Turn on LED during drop (Active LOW on Super Mini)
    
    dropServo.write(SERVO_DROP_POS);
    delay(1500); // Hold open to let string/ring fall clear

    dropServo.write(SERVO_LOCK_POS);
    digitalWrite(ONBOARD_LED_PIN, HIGH); // Turn off LED
    Serial.println("[DROP] Servo reset to lock position.");
}

// Server connection callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] iPhone connected!");
        // Blink LED twice to confirm connection
        for (int i = 0; i < 2; i++) {
            digitalWrite(ONBOARD_LED_PIN, LOW);
            delay(100);
            digitalWrite(ONBOARD_LED_PIN, HIGH);
            delay(100);
        }
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLE] Disconnected. Restarting advertising...");
        pServer->getAdvertising()->start();
    }
};

// Characteristic write callbacks (incoming commands from iPhone)
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        // Universal string handling compatible with both ESP32 Arduino Core 2.x and 3.x
        String rxValue = String(pCharacteristic->getValue().c_str());
        rxValue.trim();

        if (rxValue.length() > 0) {
            Serial.print("[BLE] Received: ");
            Serial.println(rxValue);

            if (rxValue.equalsIgnoreCase("DROP") || rxValue.equals("1")) {
                if (deviceConnected && pTxCharacteristic != NULL) {
                    pTxCharacteristic->setValue("DROPPED\n");
                    pTxCharacteristic->notify();
                }
                triggerDrop();
            } 
            else if (rxValue.equalsIgnoreCase("LOCK") || rxValue.equals("0")) {
                dropServo.write(SERVO_LOCK_POS);
                Serial.println("[SERVO] Manually set to LOCK");
            }
            else if (rxValue.equalsIgnoreCase("TEST")) {
                dropServo.write(SERVO_DROP_POS);
                delay(800);
                dropServo.write(SERVO_LOCK_POS);
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Tello Drop System Initializing ===");

    // Onboard LED setup
    pinMode(ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(ONBOARD_LED_PIN, HIGH); // Off initially

    // Initialize Servo
    ESP32PWM::allocateTimer(0);
    dropServo.setPeriodHertz(50); // Standard 50Hz PWM
    dropServo.attach(SERVO_PIN, 500, 2400); // 500us - 2400us pulse
    dropServo.write(SERVO_LOCK_POS);
    Serial.println("[SERVO] Attached to GPIO 4 (Locked)");

    // Initialize BLE Device
    BLEDevice::init("TelloDropSystem");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // TX Characteristic (Notify iPhone)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic (Receive commands from iPhone)
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start Service & Advertising
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Functions that help with iPhone connections
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();

    Serial.println("[BLE] Advertising as 'TelloDropSystem' - Ready!");
}

void loop() {
    delay(50);
}