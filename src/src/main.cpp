#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "SparkFunLSM6DSO.h"
#include <time.h>

// Wi-Fi credentials
const char* ssid = "Testuser";
const char* password = "";

// Textbelt API
const String textbelt_url = "https://textbelt.com/text";
const String textbelt_key = "";

// Flask server API
const String server_url = "http://<>/api/resource"; // Replace with your Flask server's IP

// LED Pin
#define LED_PIN 13
bool ledState = false;

// Calibration and motion detection variables
const int SAMPLES_FOR_CALIBRATION = 100;
float accelerationThreshold;
float baselineAcceleration = 0;

const int LIGHT_TIME = 3000; // Time in milliseconds to keep the LED on
unsigned long previousMillis = 0;
bool ledOn = false; // Tracks if the LED is currently on

// Wi-Fi timeout (in milliseconds)
const unsigned long wifiTimeout = 15000; // 15 seconds

// Initialize LSM6DSO
LSM6DSO myIMU;

// Function to calculate RMS of accelerometer readings
float calculateRMS() {
    float x = myIMU.readFloatAccelX();
    float y = myIMU.readFloatAccelY();
    return sqrt(x * x + y * y);
}

// Function to calibrate the sensor
void calibrateSensor() {
    float sum = 0;
    float maxReading = -1000;
    float minReading = 1000;

    Serial.println("Calibrating sensor...");
    Serial.println("Keep the sensor still...");

    // Collect samples
    for (int i = 0; i < SAMPLES_FOR_CALIBRATION; i++) {
        float rms = calculateRMS();
        sum += rms;
        maxReading = max(maxReading, rms);
        minReading = min(minReading, rms);
        delay(20);
    }

    // Calculate baseline and threshold
    baselineAcceleration = sum / SAMPLES_FOR_CALIBRATION;
    float range = maxReading - minReading;
    accelerationThreshold = baselineAcceleration + (range * 5);

    Serial.println("Calibration complete!");
    Serial.print("Baseline: ");
    Serial.println(baselineAcceleration);
    Serial.print("Threshold: ");
    Serial.println(accelerationThreshold);
}

// Function to send SMS
void sendSMS(const String& message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Set up the payload
        String payload = "phone=%2B1&message=" + message + "&key=" + textbelt_key;

        // Start HTTP POST request
        Serial.println("\nSending SMS...");
        http.begin(textbelt_url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpResponseCode = http.POST(payload);

        // Handle response
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response: " + response);
        } else {
            Serial.println("Error: " + String(httpResponseCode));
        }

        http.end();
    } else {
        Serial.println("Wi-Fi not connected. Unable to send SMS.");
    }
}

// Function to get the current timestamp in human-readable format
String getCurrentTimestamp() {
    const char* ntpServer = "pool.ntp.org";
    const long gmtOffset_sec = -8 * 3600; // Adjust for PST (GMT-8)
    const int daylightOffset_sec = 3600; // Adjust for daylight savings

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "1970-01-01 00:00:00"; // Fallback timestamp
    }

    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

// Function to log motion data to Flask server
void logMotionToServer(const String& timestamp) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Prepare JSON payload
        String payload = "{\"timestamp\":\"" + timestamp + "\"}";

        // Configure HTTP request
        Serial.println("\nLogging motion to server...");
        Serial.println("Payload: " + payload);

        http.begin(server_url);
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST(payload);

        // Handle response
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response from server: " + response);
        } else {
            Serial.println("Error posting to server: " + String(httpResponseCode));
        }

        http.end();
    } else {
        Serial.println("Wi-Fi not connected. Unable to log motion to server.");
    }
}

void setup() {
    Serial.begin(9600);
    delay(500);

    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // Ensure LED starts off

    // Initialize IMU
    Wire.begin();
    if (!myIMU.begin()) {
        Serial.println("Could not connect to IMU.");
        Serial.println("Freezing");
        while (1);
    }

    // Initialize IMU settings
    if (myIMU.initialize(BASIC_SETTINGS)) {
        Serial.println("IMU Loaded Settings.");
    }

    // Calibrate the sensor
    calibrateSensor();

    // Connect to Wi-Fi
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < wifiTimeout) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to Wi-Fi");
        digitalWrite(LED_PIN, HIGH); // Turn on LED to indicate Wi-Fi connection
    } else {
        Serial.println("\nWi-Fi connection failed");
        digitalWrite(LED_PIN, LOW);
    }
}

void loop() {
    unsigned long currentTime = millis();
    float rms = calculateRMS();

    // Check if motion exceeds threshold
    if (rms > accelerationThreshold) {
        if (!ledOn) { // Turn LED on and send SMS/log motion only if it's not already on
            Serial.println("Motion detected!");
            digitalWrite(LED_PIN, HIGH);
            ledOn = true;
            previousMillis = currentTime;

            // Send SMS notification
            sendSMS("Motion detected at your door!");

            // Log motion to Flask server
            String timestamp = getCurrentTimestamp();
            logMotionToServer(timestamp);

            delay(2000); // Delay to prevent multiple triggers
        }
    }

    // Turn off LED after 3 seconds
    if (ledOn && (currentTime - previousMillis >= LIGHT_TIME)) {
        digitalWrite(LED_PIN, LOW);
        ledOn = false;
        Serial.println("LED turned off after 3 seconds.");
    }

    delay(100); // Small delay for stability
}
