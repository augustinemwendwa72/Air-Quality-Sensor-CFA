#include <Arduino.h>
#include <HardwareSerial.h>

// Pin configuration
#define PMS_TX           45   // Reserved for PMS sensor
#define GSM_RXD          17   // RXD from GSM (connect to TX of GSM)
#define GSM_TXD          18   // TXD to GSM (connect to RX of GSM)
#define GSM_RST_PIN      42   // Active LOW Reset pin for GSM
#define PWRKEY      16   // Active LOW Reset pin for GSM

HardwareSerial gsmSerial(2); // Use UART1 for GSM

void resetGSM() {
  
  digitalWrite(PWRKEY, HIGH);
  digitalWrite(GSM_RST_PIN, HIGH);
  delay(100);  // Active low pulse
  digitalWrite(GSM_RST_PIN, LOW);
  delay(5000); // Wait for GSM to restart
}

void sendATCommand(String cmd, const char* waitFor = "OK", uint16_t timeout = 1000) {
  //gsmSerial.flush(); // Clear any previous data in the serial buffer
  Serial.print("[GSM] Sending command: ");
  Serial.println(cmd);
  gsmSerial.flush(); // Clear any previous data in the serial buffer
  gsmSerial.println(cmd);
  unsigned long start = millis();
  String response;
  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      if (response.indexOf(waitFor) != -1) {
        Serial.println("[GSM] ✅ Response:");
        Serial.println(response);
        return;
      }
    }
  }
  Serial.println("[GSM] ⚠️ Timeout or unexpected response:");
  Serial.println(response);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(GSM_RST_PIN, OUTPUT);
  pinMode(PWRKEY, OUTPUT);
  Serial.println("ESP32-S3 + Quectel GSM: HTTP GET loop test");

  // Start GSM UART
  gsmSerial.begin(115200, SERIAL_8N1, GSM_RXD, GSM_TXD);

  // Reset the GSM module using nRESET
  resetGSM();

  delay(1000);

  // GSM initialization
  // sendATCommand("AT");
  // sendATCommand("ATE0");
  // sendATCommand("ATE0");
  // sendATCommand("ATV1");
  // sendATCommand("AT+CPIN?");
  // sendATCommand("AT+CSQ");
  // sendATCommand("AT+CREG?");
  // sendATCommand("AT+CGATT?");
  // sendATCommand("AT+QIFGCNT=0");
  // sendATCommand("AT+QICSGP=1,1,\"internet\",\"\",\"\",1"); // Set your SIM APN
  // sendATCommand("AT+QIACT=1", "OK", 10000); // Activate PDP context
}

void loop() {
  // Set URL length and send it
  if(Serial.available()) {
    sendATCommand(Serial.readString());
  }
  // // GSM initialization
  // sendATCommand("AT");
  // sendATCommand("ATE0");
  // sendATCommand("AT+CPIN?");
  // sendATCommand("AT+CSQ");
  // sendATCommand("AT+CREG?");
  // sendATCommand("AT+CGATT?");
  // sendATCommand("AT+QIFGCNT=0");
  // sendATCommand("AT+QICSGP=1,1,\"internet\",\"\",\"\",1"); // Set your SIM APN
  // sendATCommand("AT+QIACT=1", "OK", 10000); // Activate PDP context
  // sendATCommand("AT+QHTTPURL=22,80", "CONNECT");
  // gsmSerial.print("https://www.google.com");
  // delay(100);

  // // Perform HTTP GET
  // sendATCommand("AT+QHTTPGET=80", "+QHTTPGET:");
  // delay(3000); // Wait for response
  // sendATCommand("AT+QHTTPREAD=80", "+QHTTPREAD:");

  // Serial.println("Waiting 5 seconds before next request...");
  // delay(5000);
}



// sendAT("AT+CMEE=2", "OK", 2000, maxRetries);
//   sendAT("AT", "OK", 2000, maxRetries);
//   sendAT("AT+CPIN?", "READY", 3000, maxRetries);
//   sendAT("AT+QHTTPCFG=\"requestheader\",1", "OK", 3000, maxRetries);  // Enable custom headers

//   if (sendAT("AT+QIACT?", "+QIACT:", 5000, maxRetries)) {
//     hasIp = true;
//   } else {
//     hasIp = false;
//     sendAT("AT+QICSGP=1,1,\"\",\"\",\"\",1", "OK", 3000, maxRetries);
//     sendAT("AT+QIACT=1", "OK", 5000, maxRetries);
//     sendAT("AT+CFUN=1,1", "OK", 3000, maxRetries);
//     delay(10000);
//     hasIp = sendAT("AT+QIACT?", "+QIACT", 5000, maxRetries);