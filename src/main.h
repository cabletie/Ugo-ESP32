#pragma once

#include <Arduino.h>
#include <TinyPICO.h>

// APA102 Dotstar
#define DOTSTAR_PWR 13
#define DOTSTAR_DATA 2
#define DOTSTAR_CLK 12

// Battery
#define BAT_CHARGE 34
#define BAT_VOLTAGE 35

#ifdef ESP32
#include <WiFiClientSecure.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#else
#error Platform not supported
#endif
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

#define OTA_NAME "Ugo_" // Last 6 MAC address characters will be appended at the end of the OTA name, "Ugo_XXXXXX" by default
#define AP_NAME "Ugo_"  // Last 6 MAC address characters will be appended at the end of the AP name, "Ugo_XXXXXX" by default
#define button1_pin 25
#define button2_pin 26
#define button3_pin 27
#define button4_pin 4
#define OTA_TIMEOUT 300000    // 5 minutes
#define CONFIG_TIMEOUT 300000 // 5 minutes

// DO NOT CHANGE DEFINES BELOW
#define NORMAL_MODE 0
#define OTA_MODE 1
#define CONFIG_MODE 2
#define HASS_REGISTER_MODE 3

// If the max message size is too small, throw an error at compile time. See PubSubClient.cpp line 359
#if MQTT_MAX_PACKET_SIZE < 512
#error "MQTT_MAX_PACKET_SIZE is too small in libraries/PubSubClient/src/PubSubClient.h, increase it to 512"
#endif

#define MQTT_PUBLISH_TRIES 3

// Initialise the TinyPICO library
extern TinyPICO tp;

extern uint8_t deviceMode;

extern uint8_t button;

extern bool otaModeStarted;
extern volatile bool ledState;

// TIMERS
extern unsigned long otaMillis, ledMillis, configTimer, otaTimer;

extern byte mac[6];
extern AsyncWebServer server;

extern byte dnsPort;
extern DNSServer dnsServer;

extern WiFiClient espClientInsecure;
extern PubSubClient mqttClientInsecure;
extern WiFiClientSecure espClientSecure;
extern PubSubClient mqttClientSecure;

extern DynamicJsonDocument json; // config buffer

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED false // false

//#define BUTTON_PIN_BITMASK 0x300000000 // 2^33 in hex
//                                  10 // Pin  4
//                                4000 // Pin 14
//                                8000 // Pin 15
//                             2000000 // Pin 25
//                             4000000 // Pin 26
//                             8000000 // Pin 27
//                           100000000 // Pin 32
//                           200000000 // Pin 33
//#define BUTTON_PIN_BITMASK 0x00800C010 // 4 + 14 + 15 + 27
//#define BUTTON_PIN_BITMASK 0x308000010 // 4 + 33 + 32 + 27
#define BUTTON_PIN_BITMASK 0x00E000010 // 4 + 25 + 26 + 27

void setup();

void loop();