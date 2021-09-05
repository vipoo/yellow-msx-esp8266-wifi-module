

/*********
  Yellow MSX for RC2014
  RC2014 Wifi Module
  Serial to (telnet) TCP bridge
*********/

const int LED_PIN = 5;

#include "at-command-parser.h"
#include "gpio.h"
#include "parse-string.h"
#include "passthrough-escaping.h"
#include "system-operation-mode.h"
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ezTime.h>

WiFiClient client;
int updateProgressFilter = 0;

void setup() {
#ifdef WIFI_IS_OFF_AT_BOOT
  enableWiFiAtBootTime(); // can be called from anywhere with the same effect
#endif

  pinMode(LED_PIN, OUTPUT);

  Serial.begin(19200);
  setRXOpenDrain();
  setCTSFlowControl();

  Serial.println("\r\n\033[2JWifi Module for Yellow MSX.\r\n");

  WiFi.begin();

  int count = 20;
  while (WiFi.status() != WL_CONNECTED && count >= 0) {
    delay(500);
    Serial.print(".");
    count--;
  }

  if (WiFi.status() != WL_CONNECTED)
    Serial.print("\r\nWiFi not connected\r\n");
  else
    Serial.printf("\r\nWiFi connected to %s\r\n", WiFi.SSID());

  ArduinoOTA.onStart([]() {
    String type;

    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "application";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.print("Start updating " + type + "\r\n");
  });

  ArduinoOTA.onEnd([]() { Serial.print("\r\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if ((updateProgressFilter & 7) == 0 || progress == total)
      Serial.printf("\r\033[2KProgress: %u%%", (progress / (total / 100)));

    updateProgressFilter++;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.print("Auth Failed\r\n");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.print("Begin Failed\r\n");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.print("Connect Failed\r\n");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.print("Receive Failed\r\n");
    } else if (error == OTA_END_ERROR) {
      Serial.print("End Failed\r\n");
    }
  });
  ArduinoOTA.begin();

  waitForSync();

  Serial.println("UTC: " + UTC.dateTime());

  Timezone myTimeZone;
  myTimeZone.setLocation("Australia/Melbourne");
  Serial.print("Local Time is: " + myTimeZone.dateTime());
  Serial.print("\r\n");

  Serial.print("IP address: ");
  Serial.print(WiFi.localIP());
  Serial.print("\r\n");
  Serial.print("READY\r\n");
}

int incomingByte = 0;
int counter = 0;
int timeOfLastIncomingByte = 0;

bool wasPassthroughMode = false;

void loop() {
  const int timeSinceLastByte = millis() - timeOfLastIncomingByte;

  ArduinoOTA.handle();

  testForEscapeSequence(timeSinceLastByte);

  if (wasPassthroughMode && !isPassthroughMode()) {
    Serial.print("\r\nREADY\r\n");
  }

  wasPassthroughMode = isPassthroughMode();

  if (Serial.available() > 0) {
    digitalWrite(LED_PIN, HIGH); // turn the LED on
    counter = 100;

    incomingByte = Serial.read();

    if (isCommandMode())
      processCommandByte(incomingByte);

    else if (incomingByte == '+') {
      processPotentialEscape(timeSinceLastByte);

    } else {
      abortEscapeSquence();
      client.write((char)incomingByte);
    }

    timeOfLastIncomingByte = millis();
  } else {
    if (counter > 0)
      counter -= 1;
    if (counter == 0)
      digitalWrite(LED_PIN, LOW); // turn the LED off
  }

  if (isPassthroughMode())
    if (client.available() > 0) {
      Serial.print(client.read());
    }
}
