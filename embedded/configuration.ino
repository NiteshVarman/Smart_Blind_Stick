//final frontend
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "time.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WebServer.h>

// Define Web Server on port 80
WebServer server(80);



// WiFi Credentials
#define WIFI_SSID "Nitesh"
#define WIFI_PASSWORD "nitesh17"

// Sensor Pins
#define TRIG_PIN 5
#define ECHO_PIN 18
#define IR_SENSOR_PIN 4
#define TOUCH_PIN 15
#define BUZZER_PIN 19

// SMTP Credentials
#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT 465
#define SENDER_EMAIL "niteshvarman17@gmail.com"
#define SENDER_PASSWORD "jzki jmrh xelc nmnx"
#define RECIPIENT_EMAIL "royalbeast9966@gmail.com"

// GPS Setup
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

WiFiClientSecure client;

// NTP Server for IST
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;   // IST = GMT+5:30
const int daylightOffset_sec = 0;

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, 16, 17);  // GPS on UART2
    Serial.println("GPS Module initialized...");

    // WiFi connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("✅ WiFi Connected!");

    // Show the IP Address for frontend access
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());  // This is the address to use in your frontend (browser)

    // Set NTP Time (IST)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000); // Give time to sync NTP

    // Pin configuration
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(IR_SENSOR_PIN, INPUT);
    pinMode(TOUCH_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);  // Start with buzzer off
    server.on("/", handleRoot);   // Root page shows live sensor data
    server.begin();
    Serial.println("Web Server Started - Visit: http://" + WiFi.localIP().toString());

}

void handleRoot() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <title>Smart Blind Stick Dashboard</title>
        <meta http-equiv="refresh" content="2">
        <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #f4f4f9;
                color: #333;
                margin: 0;
                padding: 0;
                display: flex;
                align-items: center;
                justify-content: center;
                height: 100vh;
            }
            .container {
                background-color: #fff;
                padding: 20px;
                box-shadow: 0 0 15px rgba(0, 0, 0, 0.2);
                border-radius: 12px;
                text-align: center;
                max-width: 500px;
                width: 90%;
            }
            h2 {
                color: #007BFF;
                margin-bottom: 15px;
            }
            table {
                width: 100%;
                margin-top: 10px;
                border-collapse: collapse;
            }
            table, th, td {
                border: 1px solid #ddd;
            }
            th, td {
                padding: 10px;
                text-align: center;
            }
            th {
                background-color: #007BFF;
                color: white;
            }
            .status-good {
                color: green;
                font-weight: bold;
            }
            .status-bad {
                color: red;
                font-weight: bold;
            }
            .footer {
                margin-top: 10px;
                font-size: 12px;
                color: #888;
            }
        </style>
    </head>
    <body>
        <div class='container'>
            <h2>Smart Blind Stick - Live Dashboard</h2>
            <p><strong>Current Time:</strong> )rawliteral" + getISTTimestamp() + R"rawliteral(</p>
            <table>
                <tr>
                    <th>Sensor</th>
                    <th>Status / Value</th>
                </tr>
                <tr>
                    <td>Ultrasonic Distance</td>
                    <td>)rawliteral" + String(getUltrasonicDistance()) + R"rawliteral( cm</td>
                </tr>
                <tr>
                    <td>IR Sensor</td>
                    <td class=")rawliteral" + (digitalRead(IR_SENSOR_PIN) == LOW ? "status-bad" : "status-good") + R"rawliteral(">
                        )rawliteral" + (digitalRead(IR_SENSOR_PIN) == LOW ? "Object Detected" : "No Object") + R"rawliteral(
                    </td>
                </tr>
                <tr>
                    <td>Touch Sensor</td>
                    <td class=")rawliteral" + (digitalRead(TOUCH_PIN) == HIGH ? "status-bad" : "status-good") + R"rawliteral(">
                        )rawliteral" + (digitalRead(TOUCH_PIN) == HIGH ? "TOUCHED!" : "No Touch") + R"rawliteral(
                    </td>
                </tr>
                <tr>
                    <td>GPS Location</td>
                    <td>)rawliteral" + getGPSLocation() + R"rawliteral(</td>
                </tr>
            </table>
            <div class='footer'>
                <p>Developed by <strong>Nitesh, Rushyanth, Hasmith</strong> | Powered by ESP32</p>
            </div>
        </div>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", html);
}




String getISTTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Time Unavailable";
    }
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

String base64Encode(String input) {
    const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String encoded = "";
    int val = 0, valb = -6;
    for (char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded += base64_table[(val >> valb) & 63];
            valb -= 6;
        }
    }
    if (valb > -6) encoded += base64_table[((val << 8) >> (valb + 8)) & 63];
    while (encoded.length() % 4) encoded += '=';
    return encoded;
}

String getGPSLocation() {
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }
    if (gps.location.isValid()) {
        return "Latitude: " + String(gps.location.lat(), 6) +
               ", Longitude: " + String(gps.location.lng(), 6);
    } else {
        return "Location: Unavailable";
    }
}

void sendEmailAlert() {
    client.setInsecure();

    if (!client.connect(SMTP_SERVER, SMTP_PORT)) {
        Serial.println("❌ Failed to connect to SMTP Server");
        return;
    }

    String timestamp = getISTTimestamp();
    String location = getGPSLocation();
    String message = "🚨 Emergency! A blind person needs immediate assistance.\n\n";
    message += "Time: " + timestamp + "\n";
    message += location + "\n\n";
    message += "Please respond immediately.";


    // SMTP Dialog
    client.println("EHLO ESP32");
    delay(500);
    client.println("AUTH LOGIN");
    delay(500);
    client.println(base64Encode(SENDER_EMAIL));
    delay(500);
    client.println(base64Encode(SENDER_PASSWORD));
    delay(500);
    client.println("MAIL FROM:<" + String(SENDER_EMAIL) + ">");
    delay(500);
    client.println("RCPT TO:<" + String(RECIPIENT_EMAIL) + ">");
    delay(500);
    client.println("DATA");
    delay(500);
    client.println("From: Smart Blind Stick <" + String(SENDER_EMAIL) + ">");
    client.println("To: <" + String(RECIPIENT_EMAIL) + ">");
    client.println("Subject: 🚨 Emergency Alert!");
    client.println("MIME-Version: 1.0");
    client.println("Content-Type: text/plain; charset=UTF-8");
    client.println();
    client.println(message);
    client.println(".");
    delay(500);
    client.println("QUIT");

    Serial.println("✅ Emergency Email Sent!");
}

float getUltrasonicDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH);
    return duration * 0.034 / 2;
}

void handleBuzzer(int objectDetected, float distance) {
    if (objectDetected) {
        // Continuous beep for IR detection
        digitalWrite(BUZZER_PIN, HIGH);
    } else if (distance <= 30 && distance > 0) {
        // Single beep for ultrasonic detection
        digitalWrite(BUZZER_PIN, HIGH);
        delay(500);
        digitalWrite(BUZZER_PIN, LOW);
    } else {
        // No object, buzzer off
        digitalWrite(BUZZER_PIN, LOW);
    }
}

void loop() {
    float distance = getUltrasonicDistance();
    int irReading = digitalRead(IR_SENSOR_PIN);
    int touchDetected = digitalRead(TOUCH_PIN);

    int objectDetected = (irReading == LOW);  // LOW = object detected (IR sensor)

    // Debug info
    Serial.printf("Time: %s\n", getISTTimestamp().c_str());
    Serial.printf("Distance: %.2f cm\n", distance);
    Serial.printf("IR Sensor: %s\n", objectDetected ? "Object Detected" : "No Object");
    Serial.printf("Touch Sensor: %s\n", touchDetected ? "TOUCHED!" : "No Touch");
    Serial.printf("GPS: %s\n", getGPSLocation().c_str());
    Serial.println("--------------------------------------");

    if (touchDetected) {
        Serial.println("⚠ Emergency Triggered! Sending Email...");
        sendEmailAlert();
        delay(5000);  // Avoid repeat emails if button stays pressed
    }

    handleBuzzer(objectDetected, distance);
    delay(500); 
    server.handleClient();
 // Polling interval
}