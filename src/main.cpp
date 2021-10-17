#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef ESP32
#include <WiFi.h>
#include <esp_now.h>
#else
#include <ESP8266WiFi.h>
#include <espnow.h>
#endif

#define moisture_max 0.80
#define moisture_min 0.30

#define DEBUG_FLAG

// Selectors
#define S0 14 // GPIO14
#define S1 12 // GPIO12
#define S2 13 // GPIO13

// Misc Pins (Power, Enable and analog pin)
#define MOISTURE_VCC 00 // GPIO0
#define OUTPUT_VCC 05   // GPIO5
#define OUTPUT_Ebar 04  // GPIO4
#define ADC_PIN 0       // A0

#define GROUP_ID 855544
uint8_t broadcastAddress[] = {0x24, 0xA1, 0x60, 0x3A, 0xD1, 0xD1};

// Globals Internal
StaticJsonDocument<300> doc;

uint8_t macAddr_int[6];
char macAddr[18];

// -----------------
// --- FUNCTIONS ---
// -----------------

// Callback when data is sent

void gotoSleep()
{
#ifdef DEBUG_FLAG
    long currentmillis = millis(); // get the  current milliseconds from arduino
    // report milliseconds
    Serial.print("Total milliseconds running: ");
    Serial.println(currentmillis);
#endif

    delay(10);
    ESP.deepSleep(3.6e9); // deep sleep
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{
#ifdef DEBUG_FLAG
    Serial.print(sendStatus);
    if (sendStatus == 0)
    {
        char mac_recv[18];
        sprintf(mac_recv, "%02X%02X%02X%02X%02X%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        Serial.print("Mac addr of recipient: ");
        Serial.print(mac_recv);
        String msgStr;
        serializeJson(doc, msgStr);
        Serial.print(" status: ");
        Serial.print(msgStr);
        Serial.println();
    }
    else
        Serial.println("Delivery fail");
#endif
    gotoSleep();
}

void sendReading()
{
    String msgStr;
    StaticJsonDocument<300> msgJson;

    msgJson["group_id"] = GROUP_ID;
    sprintf(macAddr, "%02X%02X%02X%02X%02X%02X", macAddr_int[0], macAddr_int[1], macAddr_int[2], macAddr_int[3], macAddr_int[4], macAddr_int[5]);
    msgJson["mac"] = macAddr;
    msgJson["data"] = doc;
    msgJson["uptime"] = millis();

    serializeJson(msgJson, msgStr);
#ifdef DEBUG_FLAG
    Serial.println(msgStr.length());
    Serial.println(msgStr);
#endif
    esp_now_send(broadcastAddress, (uint8_t *)&msgStr, msgStr.length());
}

void newReading()
{

// TURN ON
#ifdef DEBUG_FLAG
    String debug = "New Reading\n";
    Serial.println("New read...");
#endif
    digitalWrite(OUTPUT_VCC, HIGH);
    digitalWrite(OUTPUT_Ebar, LOW);
    delay(50);

    // FIRST READ
    digitalWrite(S0, LOW);
    float battery_raw = analogRead(ADC_PIN) / 1024.f;
#ifdef DEBUG_FLAG
    debug += "battery_raw: " + String(battery_raw) + "\n";
    Serial.print("Voltage measured: ");
    Serial.println(battery_raw);
#endif
    battery_raw = battery_raw * 5;
    /*
    - 0.655 -> 3.34
    - 0.650 -> 3.32
    - 0.633 -> 3.24
    - 0.616 -> 3.17
    */
#ifdef DEBUG_FLAG
    debug += "battery_calculated " + String(battery_raw) + "\n";
    Serial.print("Voltage val: ");
    Serial.println(battery_raw);
#endif
    delay(10);

    // SECOND READ
    digitalWrite(S0, HIGH);
    float moisture_raw = analogRead(ADC_PIN) / 1024.f;
#ifdef DEBUG_FLAG
    debug += "moisture_raw " + String(moisture_raw) + "\n";
    Serial.print("Moisture measured: ");
    Serial.println(moisture_raw);
#endif
    int moisture_calculated = map(moisture_raw * 100, moisture_max * 100, moisture_min * 100, 0, 100);
#ifdef DEBUG_FLAG
    debug += "moisture_calculated: " + String(moisture_calculated) + " (raw*100: " + String(long(moisture_raw * 100)) + " min*100: " + String(long(moisture_min * 100)) + ", max*100: " + String(long(moisture_max * 100)) + ", 0 -> 100)\n";
    Serial.print("Mapped moisture val: ");
    Serial.println(moisture_calculated);
#endif
    // TURN OFF
    digitalWrite(MOISTURE_VCC, LOW);
    digitalWrite(OUTPUT_Ebar, HIGH);
    digitalWrite(OUTPUT_VCC, LOW);
#ifdef DEBUG_FLAG
    debug += "Done.\n------\n";
    Serial.println("-------");
    doc["debug"] = debug;
#endif
    doc["battery"] = battery_raw;
    doc["moisture"] = moisture_calculated;
}

// -----------------
// --- Main Setup --
// -----------------

void setup()
{
#ifdef DEBUG_FLAG
    Serial.begin(115200);
    delay(10);
    Serial.println("---- New Run ----");
#endif
    pinMode(MOISTURE_VCC, OUTPUT); // moisture vcc
    pinMode(OUTPUT_VCC, OUTPUT);   // VCC
    pinMode(OUTPUT_Ebar, OUTPUT);  // Enable

    pinMode(S0, OUTPUT); // S0
    pinMode(S1, OUTPUT); // S1
    pinMode(S2, OUTPUT); // S2

    digitalWrite(MOISTURE_VCC, HIGH);
    digitalWrite(OUTPUT_VCC, HIGH);
    digitalWrite(OUTPUT_Ebar, LOW);

    digitalWrite(S0, LOW);
    digitalWrite(S1, LOW);
    digitalWrite(S2, LOW);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != 0)
    {
#ifdef DEBUG_FLAG
        Serial.println("Error initializing ESP-NOW, restarting");
#endif
        delay(1000);
        ESP.restart();
    }
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_register_send_cb(OnDataSent);
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

    WiFi.macAddress(macAddr_int);
    sprintf(macAddr, "%02X%02X%02X%02X%02X%02X", macAddr_int[0], macAddr_int[1], macAddr_int[2], macAddr_int[3], macAddr_int[4], macAddr_int[5]);

    newReading();
    sendReading(); // send initial state when booted
}

void loop() {}