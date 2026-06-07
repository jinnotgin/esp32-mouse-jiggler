#include <Arduino.h>
#include <BleMouse.h>
#include <Preferences.h>

#define DEVICE_NAME "Bluetooth Ergonomic Mouse"
#define DEVICE_MANUFACTURER "Microsoft"

#define BATTERY_LEVEL 84

// Configs for "realistic" battery tracking
#define REALISTIC_BATTERY true
#define CONNECTIONS_PER_BATTERY_PERCENT 12    // Assumes roughly 3 host-side BLE reconnects per day over a 12-month battery life.
#define BATTERY_MIN_LEVEL 15

#define X_RANDOM_RANGE 5
#define Y_RANDOM_RANGE 5
#define MIN_MOVE_INTERVAL 5000    // 5 seconds
#define MAX_MOVE_INTERVAL 30000   // 30 seconds
#define MIN_CLICK_INTERVAL 10000  // 10 seconds
#define MAX_CLICK_INTERVAL 60000  // 60 seconds
#define DISCONNECT_TIMEOUT 10000  // 10 seconds

#define BOOT_BUTTON 0  // GPIO0 is typically the BOOT button on most ESP32 boards

// Configuration variables
bool enableMouseMovement = true;
bool enableRightClick = false;

// LED configuration
#define USE_LED true  // Set to false if your board doesn't have an LED or you don't want to use it
#define LED_PIN 2     // Change this to match your board's LED pin, if different

Preferences prefs;
uint8_t currentBatteryLevel = BATTERY_LEVEL;
uint8_t bleConnectionCount = 0;
bool batteryConnectionCounted = false;

BleMouse bleMouse(DEVICE_NAME, DEVICE_MANUFACTURER, BATTERY_LEVEL);

unsigned long lastMoveTime = 0;
unsigned long lastClickTime = 0;
unsigned long moveInterval = 0;
unsigned long clickInterval = 0;
unsigned long lastConnectedTime = 0;
unsigned long lastDebounceTime = 0;
bool wasConnected = false;
bool lastButtonState = HIGH;
bool buttonState;
bool featuresActive = true;

// Function prototypes
void moveMouse();
void rightClick();
void checkConnectionAndReset();
void checkButton();
void updateLED();
void printConfig();
void wiggleMouse();
void setupBatterySpoofing();
void updateBatteryOnBleConnected();
void saveBatteryState();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Mouse Emulator");
  setupBatterySpoofing();

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  if (USE_LED) {
    pinMode(LED_PIN, OUTPUT);
  }

  bleMouse.begin();
  bleMouse.setBatteryLevel(currentBatteryLevel);

  // Initialize random intervals
  moveInterval = random(MIN_MOVE_INTERVAL, MAX_MOVE_INTERVAL);
  clickInterval = random(MIN_CLICK_INTERVAL, MAX_CLICK_INTERVAL);

  updateLED();
  printConfig();
}

void loop() {
  checkButton();

  if (bleMouse.isConnected()) {
    updateBatteryOnBleConnected();

    if (!wasConnected) {
      Serial.println("BLE Mouse connected");
      wasConnected = true;
      wiggleMouse(); // Wiggle mouse on connection
    }
    lastConnectedTime = millis();

    if (featuresActive) {
      unsigned long currentTime = millis();

      // Check if it's time to move the mouse
      if (enableMouseMovement && currentTime - lastMoveTime >= moveInterval) {
        moveMouse();
        lastMoveTime = currentTime;
        moveInterval = random(MIN_MOVE_INTERVAL, MAX_MOVE_INTERVAL);
      }

      // Check if it's time to right-click
      if (enableRightClick && currentTime - lastClickTime >= clickInterval) {
        rightClick();
        lastClickTime = currentTime;
        clickInterval = random(MIN_CLICK_INTERVAL, MAX_CLICK_INTERVAL);
      }
    }
  } else {
    batteryConnectionCounted = false;
    checkConnectionAndReset();
  }
}

void moveMouse() {
  int x = random(-X_RANDOM_RANGE, X_RANDOM_RANGE + 1);
  int y = random(-Y_RANDOM_RANGE, Y_RANDOM_RANGE + 1);

  bleMouse.move(x, y);
  Serial.printf("Moved mouse: x=%d, y=%d\n", x, y);
}

void rightClick() {
  bleMouse.click(MOUSE_RIGHT);
  Serial.println("Performed right-click");
}

void checkConnectionAndReset() {
  if (wasConnected) {
    Serial.println("BLE Mouse disconnected");
    wasConnected = false;
  }

  if (millis() - lastConnectedTime > DISCONNECT_TIMEOUT) {
    Serial.println("Connection timeout. Restarting ESP32...");
    delay(1000);  // Short delay to allow serial message to be sent
    ESP.restart();
  } else {
    Serial.println("Waiting for connection...");
    delay(1000);  // Check connection status every second
  }
}

void checkButton() {
  int reading = digitalRead(BOOT_BUTTON);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        // Toggle features on/off
        featuresActive = !featuresActive;
        updateLED();
        printConfig();
        if (featuresActive && bleMouse.isConnected()) {
          wiggleMouse(); // Wiggle mouse when features are enabled
        }
      }
    }
  }

  lastButtonState = reading;
}

void updateLED() {
  if (USE_LED) {
    digitalWrite(LED_PIN, featuresActive ? HIGH : LOW);
  }
}

void printConfig() {
  Serial.println("Configuration:");
  Serial.print("Features: ");
  Serial.println(featuresActive ? "Active" : "Inactive");
  Serial.print("Mouse Movement: ");
  Serial.println(featuresActive && enableMouseMovement ? "Enabled" : "Disabled");
  Serial.print("Right Click: ");
  Serial.println(featuresActive && enableRightClick ? "Enabled" : "Disabled");
  Serial.print("LED State: ");
  Serial.println(featuresActive ? "ON" : "OFF");
}

void wiggleMouse() {
  Serial.println("Performing wiggle");
  int wiggleCount = random(3, 6);  // Random number of wiggle movements
  int wiggleDelay = 50;

  for (int i = 0; i < wiggleCount; i++) {
    int x = random(-20, 21);  // Random x movement between -20 and 20
    int y = random(-20, 21);  // Random y movement between -20 and 20

    bleMouse.move(x, y);
    delay(wiggleDelay);

    // Move back, but not exactly to the original position
    bleMouse.move(-x + random(-5, 6), -y + random(-5, 6));
    delay(wiggleDelay);
  }
}

void setupBatterySpoofing() {
  if (REALISTIC_BATTERY) {
    prefs.begin("jiggler", false);
    currentBatteryLevel = prefs.getUChar("battery", BATTERY_LEVEL);
    bleConnectionCount = prefs.getUChar("connCount", 0);

    if (currentBatteryLevel > 100) {
      currentBatteryLevel = BATTERY_LEVEL;
    }

    if (currentBatteryLevel < BATTERY_MIN_LEVEL) {
      currentBatteryLevel = BATTERY_MIN_LEVEL;
    }

    if (bleConnectionCount >= CONNECTIONS_PER_BATTERY_PERCENT) {
      bleConnectionCount = 0;
    }

    saveBatteryState();
  } else {
    currentBatteryLevel = BATTERY_LEVEL;
    prefs.begin("jiggler", false);
    prefs.clear();
    prefs.end();
  }
}

void updateBatteryOnBleConnected() {
  if (!REALISTIC_BATTERY || batteryConnectionCounted) {
    return;
  }

  batteryConnectionCounted = true;
  bleConnectionCount++;

  if (bleConnectionCount >= CONNECTIONS_PER_BATTERY_PERCENT) {
    bleConnectionCount = 0;
    if (currentBatteryLevel > BATTERY_MIN_LEVEL) {
      currentBatteryLevel--;
    } else {
      currentBatteryLevel = 100;
    }
  }

  bleMouse.setBatteryLevel(currentBatteryLevel);
  saveBatteryState();
  Serial.printf("Battery spoof: %u%%, connection count: %u/%u\n",
                currentBatteryLevel,
                bleConnectionCount,
                CONNECTIONS_PER_BATTERY_PERCENT);
}

void saveBatteryState() {
  if (!REALISTIC_BATTERY) {
    return;
  }

  prefs.putUChar("battery", currentBatteryLevel);
  prefs.putUChar("connCount", bleConnectionCount);
}
