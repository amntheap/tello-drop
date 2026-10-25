# Tello Drop Controller 🪂

A lightweight, Bluetooth Low Energy (BLE) payload drop system for the Ryze Tello drone using an ESP32-S3 Super Mini and a PZ-15320 micro servo.

## 📱 Web Bluetooth Controller (iPhone & Android)

* **Live Web App:** [https://amntheap.github.io/tello-drop/](https://amntheap.github.io/tello-drop/)
* On iPhone, open the link above in the free **Bluefy - Web BLE Browser** app.

## ⚙️ Hardware Setup

* **Microcontroller:** ESP32-S3 Super Mini
* **Servo:** PZ-15320 (1.7g linear/micro servo)
* **Battery:** 1S LiPo (3.7V, ~150mAh)
* **Wiring:**
  * Servo VCC (Red) -> LiPo (+) / ESP32 5V
  * Servo GND (Black) -> LiPo (-) / ESP32 GND
  * Servo PWM (Orange/White) -> ESP32 GPIO 4
