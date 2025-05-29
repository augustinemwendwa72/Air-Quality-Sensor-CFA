#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "Adafruit_PM25AQI.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 36
#define PMS_RX 21
#define PMS_TX 45
#define GSM_RXD 17
#define GSM_TXD 18
#define QUECTEL_PWR_KEY 16
#define GSM_RST_PIN 42
#define DHTTYPE DHT22  // DHT 22 (AM2302), AM2321
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

unsigned int serialLogTimeInterval = 5000;
unsigned int dotTime = 0;
bool hasIp = false;
String scannedResponse = "";

HardwareSerial SerialAT(2);
HardwareSerial pmsSerial(1);
Adafruit_PM25AQI pms;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 70000;
int maxRetries = 1;
int pm1_0 = 0, pm2_5 = 0, pm10_0 = 0;
String currentTimestamp = "";

// Declare the helper function first
String repeatChar(char c, int count) {
  String s = "";
  for (int i = 0; i < count; i++) {
    s += c;
  }
  return s;
}

void clearSerialBuffer() {
  while (SerialAT.available()) Serial.print(SerialAT.readString());
}

bool sendAT(String command, String expected, int timeout = 5000, int width = 60) {
  clearSerialBuffer();
  SerialAT.println(command);  // send command to EC200
  String line = repeatChar('_', width);
  // Serial.println("||==========================================================||");
  Serial.println("📤 " + command);
  // Serial.println("||==========================================================||");

  String response = "";
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
    }
    if (response.indexOf(expected) != -1) {
      // Serial.println("||==========================================================||");
      Serial.println("✅ Response: " + response);
      // Serial.println("||==========================================================||");
      return true;
    }
  }
  // Serial.println("||==========================================================||");
  Serial.println("❌ No valid response...");
  // Serial.println("||==========================================================||");
  return false;
}



String getTimestampFromEC200U() {
  if (sendAT("AT+QNTP=1,\"pool.ntp.org\",123", "QNTP:", 10000)) {
    Serial.println("NTP sync success.");
    Serial.println("Full Response: " + scannedResponse);

    int start = scannedResponse.indexOf("+QNTP: 0,\"");
    if (start == -1) return "1970-01-01T00:00:00+00";
    start += strlen("+QNTP: 0,\"");
    int end = scannedResponse.indexOf("\"", start);
    if (end == -1) return "1970-01-01T00:00:00+00";

    String raw = scannedResponse.substring(start, end);
    Serial.println("Raw time: " + raw);

    String year = raw.substring(0, 4);
    String month = raw.substring(5, 7);
    String day = raw.substring(8, 10);
    String time = raw.substring(11, 19);
    String tz = raw.substring(19);

    String formatted = year + "-" + month + "-" + day + "T" + time + tz;
    Serial.println("Timestamp: " + formatted);
    return formatted;
  }

  Serial.println("NTP sync failed.");
  return "1970-01-01T00:00:00+00";
}

void powerOnEC200U() {
  Serial.println("Powering on EC200U...");
  pinMode(QUECTEL_PWR_KEY, OUTPUT);
  digitalWrite(QUECTEL_PWR_KEY, LOW);
  delay(1000);
  digitalWrite(QUECTEL_PWR_KEY, HIGH);
  delay(2000);
  digitalWrite(QUECTEL_PWR_KEY, LOW);
  delay(5000);
  Serial.println("EC200U powered on.");
}

void initEC200U() {
  Serial.println("Initializing EC200U...");
  sendAT("AT+CMEE=2", "OK", 2000, maxRetries);
  sendAT("AT", "OK", 2000, maxRetries);
  sendAT("AT+CPIN?", "READY", 3000, maxRetries);
  sendAT("AT+QHTTPCFG=\"requestheader\",1", "OK", 3000, maxRetries);  // Enable custom headers

  if (sendAT("AT+QIACT?", "+QIACT:", 5000, maxRetries)) {
    hasIp = true;
  } else {
    hasIp = false;
    sendAT("AT+QICSGP=1,1,\"\",\"\",\"\",1", "OK", 3000, maxRetries);
    sendAT("AT+QIACT=1", "OK", 5000, maxRetries);
    sendAT("AT+CFUN=1,1", "OK", 3000, maxRetries);
    delay(10000);
    hasIp = sendAT("AT+QIACT?", "+QIACT", 5000, maxRetries);
  }
  Serial.println("EC200U initialized. IP status: " + String(hasIp ? "Connected" : "Not connected"));
}

bool readPMSData() {
  PM25_AQI_Data data;
  if (pms.read(&data)) {
    pm1_0 = data.pm10_env;
    pm2_5 = data.pm25_env;
    pm10_0 = data.pm100_env;
    return true;
  }
  return false;
}

void sendSensorData() {
  if (!readPMSData()) {
    Serial.println("PMS read failed");
    return;
  }

  currentTimestamp = getTimestampFromEC200U();
  Serial.println("Timestamp: " + currentTimestamp);

  StaticJsonDocument<512> doc;
  doc["software_version"] = "NRZ-2020-129";
  doc["timestamp"] = currentTimestamp;

  JsonArray values = doc.createNestedArray("sensordatavalues");
  JsonObject obj0 = values.createNestedObject();
  obj0["value_type"] = "P0";
  obj0["value"] = pm10_0;
  JsonObject obj1 = values.createNestedObject();
  obj1["value_type"] = "P1";
  obj1["value"] = pm1_0;
  JsonObject obj2 = values.createNestedObject();
  obj2["value_type"] = "P2";
  obj2["value"] = pm2_5;

  doc["sensor_type"] = "PMS";
  doc["APN_PIN"] = 1;

  String payload;
  serializeJson(doc, payload);
  Serial.println("JSON payload:");
  Serial.println(payload);

  String url = "http://staging.api.sensors.africa/v1/push-sensor-data/";
  String host = "staging.api.sensors.africa";
  String sensorID = "esp32-" + String((uint32_t)ESP.getEfuseMac());

  // Set the URL
  sendAT("AT+QHTTPURL=" + String(url.length()) + ",80", "CONNECT", 3000, maxRetries);
  SerialAT.print(url);
  SerialAT.write(0x1A);
  delay(3000);

  // Prepare full HTTP request
  String fullRequest =
    "POST /v1/push-sensor-data/ HTTP/1.1\r\n"
    "Host: "
    + host + "\r\n"
             "Accept: */*\r\n"
             "User-Agent: QUECTEL EC200\r\n"
             "Content-Type: application/json\r\n"
             "X-Sensor: "
    + sensorID + "\r\n"
                 "X-PIN: 1\r\n"
                 "Content-Length: "
    + String(payload.length()) + "\r\n"
                                 "\r\n"
    + payload;

  Serial.println("Request: \n" + fullRequest);

  sendAT("AT+QHTTPPOST=" + String(fullRequest.length()) + ",80,80", "CONNECT", 5000, maxRetries);
  SerialAT.print(fullRequest);
  SerialAT.write(0x1A);
  delay(5000);

  sendAT("AT+QHTTPREAD=80", "OK", 5000, maxRetries);
}


void printDhtValues(){
    sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
  }

}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pmsSerial.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  SerialAT.begin(115200, SERIAL_8N1, GSM_RXD, GSM_TXD);

  dht.begin();
  Serial.println(F("DHTxx Unified Sensor Example"));

  if (!pms.begin_UART(&pmsSerial)) {
    Serial.println("Could not find PMS5003 sensor!");
    while (1) delay(10);
  }

  Serial.println("Sensors initialized.");
  powerOnEC200U();
  initEC200U();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSendTime >= sendInterval || lastSendTime == 0) {
    printDhtValues();
    sendSensorData();
    lastSendTime = now;
  }

  if(now - dotTime >= serialLogTimeInterval){
    Serial.print(".");
    dotTime = now;
  }
}

