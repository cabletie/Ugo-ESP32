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

// Initialise the TinyPICO library
TinyPICO tp = TinyPICO();

uint8_t deviceMode = NORMAL_MODE;

uint8_t button;

bool otaModeStarted = false;
volatile bool ledState = false;

// TIMERS
unsigned long otaMillis, ledMillis, configTimer, otaTimer;

byte mac[6];
AsyncWebServer server(80);

byte dnsPort = 53;
DNSServer dnsServer;

WiFiClient espClient;
PubSubClient client(espClient);

DynamicJsonDocument json(2048); // config buffer

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

#include "actions.h"
#include "config_portal.h"
#include "ota.h"
#include "hass_register.h"

void setup()
{
    Serial.begin(115200);
    Serial.println("");

#if CORE_DEBUG_LEVEL == 5
    printWakeupReason();
    printResetReason();
    batteryPercentage();
#endif

    // Go to sleep immediately if woke up for something not related to deep sleep
    if (esp_reset_reason() != ESP_RST_DEEPSLEEP)
    {
        Serial.println("Skipping button detection.");
        blinkResetReason();
        goToSleep();
    }

    // Prepare the pins
    pinMode(button1_pin, INPUT);
    pinMode(button2_pin, INPUT);
    pinMode(button3_pin, INPUT);
    pinMode(button4_pin, INPUT);

    // This small delay is required for correct button detection
    delay(10);

    button = readButtons();

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        if (!SPIFFS.begin())
        {
            Serial.println("SPIFFS Mount Failed :: Needs formatting?");
            return;
        }
        if (FORMAT_SPIFFS_IF_FAILED)
        {
            Serial.println("SPIFFS Mount Failed :: FORMATTING");
        }
    }

    WiFi.macAddress(mac);
    readConfig();

    const char *ssid = json["id"].as<const char *>();
    const char *pass = json["pw"].as<const char *>();
    const char *ip = json["ip"].as<const char *>();
    const char *gw = json["gw"].as<const char *>();
    const char *sn = json["sn"].as<const char *>();
    const char *d1 = json["d1"].as<const char *>();
    const char *d2 = json["d2"].as<const char *>();

    if (json["id"] != "" && json["pw"] != "")
    {
        WiFi.mode(WIFI_STA);

        IPAddress ip_address, gateway_ip, subnet_mask, dns_address_1, dns_address_2;
        if (ip[0] != '\0' && ip_address.fromString(String(ip)) &&
            gw[0] != '\0' && gateway_ip.fromString(String(gw)) &&
            sn[0] != '\0' && subnet_mask.fromString(String(sn)))
        {

            if (d1[0] != '\0' && dns_address_1.fromString(String(d1)))
            {
                if (d2[0] != '\0' && dns_address_2.fromString(String(d2)))
                {
                    if (WiFi.config(ip_address, gateway_ip, subnet_mask, dns_address_1, dns_address_2))
                    {
                        Serial.println("STA successfully configured with IP, Gateway, Subnet, Primary DNS, Secondary DNS.");
                    }
                    else
                    {
                        Serial.println("STA failed to configure with IP, Gateway, Subnet, Primary DNS, Secondary DNS. Using auto config instead.");
                        Serial.println("[ip]: " + String(ip));
                        Serial.println("[gw]: " + String(gw));
                        Serial.println("[sn]: " + String(sn));
                        Serial.println("[d1]: " + String(d1));
                        Serial.println("[d2]: " + String(d2));
                    }
                }
                else
                {
                    if (WiFi.config(ip_address, gateway_ip, subnet_mask, dns_address_1))
                    {
                        Serial.println("STA successfully configured with IP, Gateway, Subnet, Primary DNS.");
                    }
                    else
                    {
                        Serial.println("STA failed to configure with IP, Gateway, Subnet, Primary DNS. Using auto config instead.");
                        Serial.println("[ip]: " + String(ip));
                        Serial.println("[gw]: " + String(gw));
                        Serial.println("[sn]: " + String(sn));
                        Serial.println("[d1]: " + String(d1));
                    }
                }
            }
            else
            {
                if (WiFi.config(ip_address, gateway_ip, subnet_mask))
                {
                    Serial.println("STA successfully configured with IP, Gateway, Subnet.");
                }
                else
                {
                    Serial.println("STA failed to configure with IP, Gateway, Subnet. Using auto config instead.");
                    Serial.println("[ip]: " + String(ip));
                    Serial.println("[gw]: " + String(gw));
                    Serial.println("[sn]: " + String(sn));
                }
            }
        }
        else
        {
            Serial.println("Failed to configure static IP.");
            Serial.println("[ip]: " + String(ip));
            Serial.println("[gw]: " + String(gw));
            Serial.println("[sn]: " + String(sn));
        }

        WiFi.begin(ssid, pass);

        for (int i = 0; i < 50; i++)
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                if (i > 40)
                {
                    deviceMode = CONFIG_MODE;
                    Serial.print("Failed to connect to: ");
                    Serial.println(ssid);
                    break;
                }
                delay(100);
            }
            else
            {
                Serial.println("Wifi connected...");
                Serial.print("SSID: ");
                Serial.println(WiFi.SSID());
                Serial.print("Mac address: ");
                Serial.println(WiFi.macAddress());
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());
                break;
            }
        }
    }
    else
    {
        deviceMode = CONFIG_MODE;
        Serial.println("No credentials set, going to config mode");
    }

    String ota_name = OTA_NAME + macLastThreeSegments(mac);
    ArduinoOTA.setHostname(ota_name.c_str());

    ArduinoOTA.onStart([]() {
        Serial.println("OTA UPLOAD STARTED...");
        tp.DotStar_SetPixelColor(0, 0, 255);
        delay(50);
    });
}

void loop()
{
    // Detect button combo to enter config mode
    toggleConfigMode();

    if (deviceMode == CONFIG_MODE)
    {
        Serial.println("STARTING CONFIG ACCESS POINT...");
        tp.DotStar_SetPixelColor(255, 255, 0);
        delay(50);
        startConfigPortal();
        Serial.println("RETURNING TO NORMAL MODE...");
        deviceMode = NORMAL_MODE;
        return;
    }

    // Detect button combo to enter Hass.io register mode
    toggleHassRegisterMode();

    if (deviceMode == HASS_REGISTER_MODE)
    {
        Serial.println("REGISTERING WITH HASS.IO...");
        tp.DotStar_SetPixelColor(255, 0, 255);
        delay(50);
        startHassRegister();
        Serial.println("RETURNING TO NORMAL MODE...");
        deviceMode = NORMAL_MODE;
        return;
    }

    // Detect button combo to enter OTA mode
    toggleOTAMode();

    if (deviceMode == OTA_MODE)
    {
        Serial.println("WAITING FOR OTA UPDATE...");
        tp.DotStar_SetPixelColor(0, 255, 255);
        delay(50);
        startOTA();
        Serial.println("RETURNING TO NORMAL MODE...");
        deviceMode = NORMAL_MODE;
        return;
    }

    if (deviceMode != NORMAL_MODE)
        return;

    handleButtonAction();

    switch (button)
    {
    case 1:
        tp.DotStar_SetPixelColor(255, 0, 0);
        break;
    case 2:
        tp.DotStar_SetPixelColor(0, 255, 0);
        break;
    case 3:
        tp.DotStar_SetPixelColor(0, 0, 255);
        break;
    case 4:
        tp.DotStar_SetPixelColor(0, 255, 255);
        break;
    case 5:
        tp.DotStar_SetPixelColor(255, 0, 255);
        break;
    case 6:
        tp.DotStar_SetPixelColor(255, 255, 0);
        break;
    case 7:
        tp.DotStar_SetPixelColor(255, 255, 255);
        break;
    default:
        break;
    }
    delay(50); // Give time to the dotstar to sync

    if (json["ha_enabled"].as<bool>())
    {
        publishBatteryLevel();
    }

    goToSleep();
}