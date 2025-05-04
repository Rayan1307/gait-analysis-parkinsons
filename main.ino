// Required Libraries
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <NewPing.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <FirebaseESP32.h>

// Wi-Fi & Cloud Config
#define FIREBASE_HOST "your-project.firebaseio.com"
#define FIREBASE_AUTH "your-api-key"
#define BLYNK_AUTH "your-blynk-auth"
#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASS "Your_WiFi_Password"

// Sensor Pins
#define FSR_PIN 34        // Force Sensor (FSR402)
#define TRIG_PIN 5        // Ultrasonic Trigger
#define ECHO_PIN 18       // Ultrasonic Echo
#define VIBRATION_PIN 23  // Vibration Motor

// Objects
Adafruit_MPU6050 mpu;
NewPing sonar(TRIG_PIN, ECHO_PIN, 200);
FirebaseData fbData;

// Moving Average Filter for Noise Reduction
#define WINDOW_SIZE 5
int readings[WINDOW_SIZE] = {0};
int index = 0;

// Function to calculate Moving Average
int movingAverage(int newValue) {
  readings[index] = newValue;
  index = (index + 1) % WINDOW_SIZE;
  int sum = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    sum += readings[i];
  }
  return sum / WINDOW_SIZE;
}

void setup() {
  Serial.begin(115200);

  // Wi-Fi & Blynk Initialization
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PASS);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  
  // MPU6050 Initialization
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 Initialization Failed!");
    while (1);
  }

  // Pin Setup
  pinMode(FSR_PIN, INPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
}

void loop() {
  Blynk.run();  // Run Blynk service

  // Read Sensors
  int forceValue = analogRead(FSR_PIN);
  forceValue = movingAverage(forceValue);  // Apply filtering

  int strideLength = sonar.ping_cm();  // Measure stride using Ultrasonic Sensor

  // Read MPU6050 Data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float tremor = a.acceleration.x;  // Detect tremors

  // Detect Gait Abnormalities
  if (forceValue < 100) {
    Serial.println("⚠ Low Foot Pressure - Possible Shuffling Gait");
    triggerAlert();
  }
  if (strideLength < 10) {
    Serial.println("⚠ Short Stride - Possible Freezing of Gait");
    triggerAlert();
  }
  if (tremor > 1.5) {
    Serial.println("⚠ Tremors Detected");
    triggerAlert();
  }

  // Send Data to Blynk (Mobile App)
  Blynk.virtualWrite(V1, forceValue);
  Blynk.virtualWrite(V2, strideLength);
  Blynk.virtualWrite(V3, tremor);

  // Send Data to Firebase (Cloud)
  Firebase.setFloat(fbData, "/Gait/Force", forceValue);
  Firebase.setFloat(fbData, "/Gait/Stride", strideLength);
  Firebase.setFloat(fbData, "/Gait/Tremor", tremor);

  delay(1000);  // Wait before next reading
}

// Function to trigger vibration alert
void triggerAlert() {
  digitalWrite(VIBRATION_PIN, HIGH);
  delay(500);
  digitalWrite(VIBRATION_PIN, LOW);
}