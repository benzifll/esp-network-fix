#include <Arduino.h>

// Automatically select the correct WiFi library
#if defined(ESP32)
  #include <WiFi.h>
  #define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#else
  #include <ESP8266WiFi.h>
  #define GET_CHIP_ID() (ESP.getChipId())
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <VL53L0X.h>
#include <LiquidCrystal_I2C.h>

// ── Settings ─────────────────────────────────────────
#define WIFI_SSID   "RasberryPi"
#define WIFI_PASS   "rasberrypi123"
#define MQTT_HOST   "broker.hivemq.com"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "archeoiot/abc123xyz"

// Pin Definitions (Universal)
#if defined(ESP32)
  #define DS18B20_PIN 14 
  #define EC_PIN      34 
  #define MIXER_PIN   27 
  #define SDA_PIN     21
  #define SCL_PIN     22
#else
  #define DS18B20_PIN D5
  #define EC_PIN      A0
  #define MIXER_PIN   D6
  #define SDA_PIN     4
  #define SCL_PIN     5
#endif

const float EC_SLOPE        = 40.12f;
const unsigned long SEND_INTERVAL = 2000;
const unsigned long TEMP_CONV_MS  = 750;
const int           EC_SAMPLES    = 20;
const unsigned long EC_SAMPLE_MS  = 5;
const int           TANK_HEIGHT   = 1000; 

// ── Objects ────────────────────────────────────────────
OneWire           oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
VL53L0X           lidar;
WiFiClient        espClient;
PubSubClient      mqtt(espClient);

bool lidarOK = false;
float lastTemp = 25.0f;
bool tempRequested = false;
unsigned long lastTempRequest = 0;
int ecBuf[EC_SAMPLES];
int ecBufIdx = 0;
bool ecReady = false;
unsigned long lastEcSample = 0;
float lastEC = 0.0f;
float lastTDS = 0.0f;
int lastWL = 0;

// ── WiFi & MQTT Management ─────────────────────────────
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("\nWiFi: Searching for ");
  Serial.println(WIFI_SSID);
  
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Searching WiFi:");
  lcd.setCursor(0, 1); lcd.print(WIFI_SSID);

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  
  #if defined(ESP8266)
    WiFi.hostname("ASOA-ESP8266");
  #endif

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long start = millis();
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(1000);
    Serial.print(".");
    lcd.setCursor(dots % 16, 1);
    lcd.print(".");
    dots++;
    
    // Every 5 seconds, retry begin
    if (dots % 5 == 0) {
       WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[CONNECTED]");
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("WiFi Connected!");
    lcd.setCursor(0,1); lcd.print(WiFi.localIP().toString());
    delay(2000);
  } else {
    Serial.println("\n[FAILED]");
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("WiFi NOT FOUND");
    lcd.setCursor(0,1); lcd.print("Check Hotspot!");
    delay(2000);
  }
}

void handleMQTT() {
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  // If we aren't connected to WiFi, fix that first
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  static unsigned long lastMqttAttempt = 0;
  if (millis() - lastMqttAttempt < 5000) return;
  lastMqttAttempt = millis();

  Serial.print("Connecting to Website (MQTT)... ");
  String clientId = "ASOA_Device_" + String(GET_CHIP_ID(), HEX);
  
  if (mqtt.connect(clientId.c_str())) {
    Serial.println("Success!");
    lcd.setCursor(0, 1); lcd.print("Website: ONLINE ");
  } else {
    Serial.print("Failed, rc=");
    Serial.println(mqtt.state());
    lcd.setCursor(0, 1); lcd.print("Website: ERR " + String(mqtt.state()));
  }
}

// ── Sensor Logic (Non-blocking) ───────────────────────
void handleTemp() {
  if (!tempRequested) {
    tempSensor.requestTemperatures();
    lastTempRequest = millis();
    tempRequested = true;
    return;
  }
  if (millis() - lastTempRequest >= TEMP_CONV_MS) {
    float t = tempSensor.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C && t >= -10.0f) lastTemp = t;
    tempRequested = false;
  }
}

void sampleEC() {
  if (millis() - lastEcSample >= EC_SAMPLE_MS) {
    lastEcSample = millis();
    ecBuf[ecBufIdx++] = analogRead(EC_PIN);
    if (ecBufIdx >= EC_SAMPLES) {
      ecBufIdx = 0;
      ecReady = true;
    }
  }
}

void calcECandTDS() {
  if (!ecReady) return;
  ecReady = false;
  long sum = 0;
  for (int i = 0; i < EC_SAMPLES; i++) sum += ecBuf[i];
  
  float refVoltage = 3.3f; // ESP usually 3.3V
  float voltage = (sum / (float)EC_SAMPLES) * (refVoltage / 1024.0f);
  float compCoef = 1.0f + 0.019f * (lastTemp - 25.0f);
  float vComp = voltage / compCoef;
  lastEC = (vComp > 0.05f) ? (EC_SLOPE * vComp) : 0.0f;
  lastTDS = voltage * 6.84f * 0.64f;
}

void readLidar() {
  if (!lidarOK) return;
  uint16_t d = lidar.readRangeContinuousMillimeters();
  if (!lidar.timeoutOccurred()) {
    lastWL = constrain(map(d, TANK_HEIGHT, 0, 0, 100), 0, 100);
  }
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(lastTemp, 1); lcd.print("C WL:"); lcd.print(lastWL); lcd.print("%  ");
  lcd.setCursor(0, 1);
  lcd.print("EC:"); lcd.print(lastEC, 1); lcd.print(" TDS:"); lcd.print(lastTDS, 1); lcd.print("  ");
}

// ── Main System ──────────────────────────────────────
void setup() {
  Serial.begin(115200);
  
  #if defined(ESP32)
    Wire.begin(SDA_PIN, SCL_PIN);
  #else
    Wire.begin(SDA_PIN, SCL_PIN);
  #endif

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("ASOA System 3.1");

  pinMode(MIXER_PIN, OUTPUT);
  digitalWrite(MIXER_PIN, HIGH);

  tempSensor.begin();
  tempSensor.setWaitForConversion(false);

  lidar.setTimeout(200);
  lidarOK = lidar.init();
  if (lidarOK) lidar.startContinuous();

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  handleMQTT();
  handleTemp();
  sampleEC();
  calcECandTDS();

  static unsigned long lastSend = 0;
  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    readLidar();
    updateLCD();

    if (mqtt.connected()) {
      StaticJsonDocument<128> doc;
      doc["temp"] = round(lastTemp * 10) / 10.0;
      doc["ec"]   = round(lastEC * 100) / 100.0;
      doc["tds"]  = round(lastTDS * 100) / 100.0;
      doc["wl"]   = lastWL;

      String json;
      serializeJson(doc, json);
      mqtt.publish(MQTT_TOPIC, json.c_str());
      Serial.println("Data Sent: " + json);
    }
  }
}