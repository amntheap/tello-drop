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
#define ONBOARD_LED_PIN  8   // Built-in LED on ESP32-S3 Super Mini

// Safe pulse limits for PZ-15320 micro servo:
// 1000us - 2000us prevents ramming into mechanical end-stops
#define SERVO_MIN_US     1000
#define SERVO_MAX_US     2000

// Safe angles: kept away from 0 and 180 to prevent mechanical stall
#define SERVO_LOCK_POS   30   // Degrees: Latch engaged
#define SERVO_DROP_POS  120   // Degrees: Latch released

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

// ==========================================
// THERMAL PROTECTION FUNCTION:
// Moves the servo and IMMEDIATELY DETACHES
// ==========================================
void moveServoSafely(int angle, int settleTimeMs = 500) {
    Serial.printf("[SERVO] Moving to %d deg...\n", angle);
    
    // 1. Attach with safe pulse limits
    dropServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    dropServo.write(angle);
    delay(settleTimeMs); // Allow time to reach target position

    // 2. CRITICAL: Detach PWM so the micro motor turns OFF completely
    // This stops stall current and keeps the servo cold!
    dropServo.detach();
    Serial.println("[SERVO] Detached (0mA standby current - cold).");
}

void triggerDrop() {
    Serial.println("[DROP] Releasing payload...");
    digitalWrite(ONBOARD_LED_PIN, LOW); // LED ON
    
    // 1. Move to release position, wait 1.2s for string/ring to fall
    moveServoSafely(SERVO_DROP_POS, 1200);

    // 2. Return to lock position and power off
    moveServoSafely(SERVO_LOCK_POS, 500);

    digitalWrite(ONBOARD_LED_PIN, HIGH); // LED OFF
    Serial.println("[DROP] Sequence completed.");
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] iPhone connected!");
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

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
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
                moveServoSafely(SERVO_LOCK_POS, 500);
            }
            else if (rxValue.equalsIgnoreCase("TEST")) {
                moveServoSafely(SERVO_DROP_POS, 800);
                moveServoSafely(SERVO_LOCK_POS, 500);
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Tello Drop System (Thermal-Protected) ===");

    pinMode(ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(ONBOARD_LED_PIN, HIGH);

    // Initialize Timer
    ESP32PWM::allocateTimer(0);
    dropServo.setPeriodHertz(50);

    // Initial safe lock position, then immediately shut down motor
    moveServoSafely(SERVO_LOCK_POS, 500);

    // Initialize BLE
    BLEDevice::init("TelloDropSystem");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();

    Serial.println("[BLE] Advertising as 'TelloDropSystem' - Ready!");
}

void loop() {
    delay(50);
}