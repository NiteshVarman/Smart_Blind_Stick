//firebase with improved buzzer
//final firebase working code
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "time.h"

// WiFi Credentials
#define WIFI_SSID "Nitesh"
#define WIFI_PASSWORD "nitesh17"

// Firebase Credentials
#define FIREBASE_HOST "https://smart-blind-stick-70181-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "hPu1blrDTiVDb2Z3UTqGYY9aD9YJ1HUAIam6d9Ox"

// Sensor Pins
#define TRIG_PIN 5
#define ECHO_PIN 18
#define IR_SENSOR_PIN 4
#define TOUCH_PIN 15
#define BUZZER_PIN 19

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // IST (GMT+5:30)
const int daylightOffset_sec = 0;

// GPS Setup
HardwareSerial gpsSerial(2); // Use UART2 (Pins 16=RX, 17=TX)
TinyGPSPlus gps;

// Firebase Objects
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;

// Setup function
void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, 16, 17);  // GPS RX=16, TX=17

    // WiFi Connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi Connected");

    // Time Sync
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Firebase Initialization
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Serial.println("✅ Firebase Initialized");

    // Pin Modes
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(IR_SENSOR_PIN, INPUT);
    pinMode(TOUCH_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

// Get current time in IST
String getISTTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "N/A";
    }
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

// Read GPS location
String getGPSLocation() {
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }

    if (gps.location.isValid()) {
        return "Latitude: " + String(gps.location.lat(), 6) + 
               ", Longitude: " + String(gps.location.lng(), 6);
    } else {
        return "Location: Not Available";
    }
}

// Read Ultrasonic sensor distance
float getUltrasonicDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    return duration * 0.034 / 2;  // Distance in cm
}

// Handle buzzer logic

void handleBuzzer(bool objectDetected, float distance) {
    if (objectDetected) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(3000);
        digitalWrite(BUZZER_PIN, LOW);
    } else if (distance <= 30 && distance > 0.5) {
        for (int i = 0; i < 3; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(500);
            digitalWrite(BUZZER_PIN, LOW);
            delay(500);
        }
    } else {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

// Upload data to Firebase
void uploadToFirebase(float distance, bool objectDetected, bool touchDetected, const String& timestamp, const String& location) {
    String safeTimestamp = timestamp;
    safeTimestamp.replace(":", "-");
    safeTimestamp.replace(" ", "_");

    String path = "/BlindStick/" + safeTimestamp;

    if (!Firebase.setFloat(fbdo, path + "/Distance", distance)) {
        Serial.print("❌ Distance Upload Failed: ");
        Serial.println(fbdo.errorReason());
    }
    if (!Firebase.setBool(fbdo, path + "/ObjectDetected", objectDetected)) {
        Serial.print("❌ ObjectDetected Upload Failed: ");
        Serial.println(fbdo.errorReason());
    }
    if (!Firebase.setBool(fbdo, path + "/TouchDetected", touchDetected)) {
        Serial.print("❌ TouchDetected Upload Failed: ");
        Serial.println(fbdo.errorReason());
    }
    if (!Firebase.setString(fbdo, path + "/Location", location)) {
        Serial.print("❌ Location Upload Failed: ");
        Serial.println(fbdo.errorReason());
    }

    if (fbdo.httpCode() == 200) {
        Serial.println("✅ Data Sent to Firebase");
    } else {
        Serial.print("❌ Firebase Error: ");
        Serial.println(fbdo.errorReason());
    }
}


// Main loop
void loop() {
    float distance = getUltrasonicDistance();
    bool objectDetected = (digitalRead(IR_SENSOR_PIN) == LOW);
    bool touchDetected = (digitalRead(TOUCH_PIN) == HIGH);

    String timestamp = getISTTimestamp();
    String location = getGPSLocation();

    // Print to Serial Monitor
    Serial.println("Timestamp: " + timestamp);
    Serial.printf("Distance: %.2f cm\n", distance);
    Serial.printf("IR Sensor: %s\n", objectDetected ? "Object Detected" : "No Object");
    Serial.printf("Touch Sensor: %s\n", touchDetected ? "Emergency Triggered" : "No Touch");
    Serial.println(location);
    Serial.println("------------------------------");

    // Send data to Firebase
    uploadToFirebase(distance, objectDetected, touchDetected, timestamp, location);

    // Trigger buzzer if required
    handleBuzzer(objectDetected, distance);

    delay(2000); // Delay for 2 seconds
}