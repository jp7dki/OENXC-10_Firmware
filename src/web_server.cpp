#include "web_server.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include "settings.h"
#include "web_assets.h"
#include "rx8900ce.h"

// Globals
const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80);

extern SemaphoreHandle_t i2cSpiMutex;

volatile bool ntpTimeSyncedFlag = false;
String lastNtpSyncTime = "Not Synced";
bool ntpConfigured = false;

// Callback triggered by ESP32 SNTP client when time is successfully synced
void time_sync_notification_cb(struct timeval *tv) {
    Serial.println("NTP Time Synced with server.");
    ntpTimeSyncedFlag = true;
}

void web_server_init() {
    // Register SNTP callback
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // 1. Configure WiFi Mode
    WiFi.mode(WIFI_AP_STA);
    
    // 2. Start AP Mode
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("OENXC-10", "password");
    Serial.println("AP Mode started. IP: 192.168.4.1");
    
    // 3. Try to connect to Client Mode if configured
    AppSettings s = settingsManager.get();
    if (s.clientSSID.length() > 0) {
        if (!s.useDHCP && s.staticIP.length() > 0) {
            IPAddress ip, gw, mask;
            ip.fromString(s.staticIP);
            gw.fromString(s.staticGateway);
            mask.fromString(s.staticMask);
            WiFi.config(ip, gw, mask, gw, IPAddress(8, 8, 8, 8)); // Use gateway and Google DNS
        }
        WiFi.begin(s.clientSSID.c_str(), s.clientPass.c_str());
        Serial.print("Connecting to WiFi: ");
        Serial.println(s.clientSSID);
    }
    
    // 4. Start DNS Server for Captive Portal
    dnsServer.start(DNS_PORT, "*", apIP);
    
    // 5. Setup Web Server Routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", WEB_HTML);
    });
    
    // Captive portal fallback
    server.onNotFound([](AsyncWebServerRequest *request){
        if (request->host().indexOf("192.168.4.1") < 0) {
            request->redirect("http://192.168.4.1/");
        } else {
            request->send(200, "text/html", WEB_HTML);
        }
    });

    // API: Get Settings
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request){
        AppSettings cs = settingsManager.get();
        JsonDocument doc;
        doc["ssid"] = cs.clientSSID;
        doc["pass"] = cs.clientPass;
        doc["dhcp"] = cs.useDHCP;
        doc["ip"] = cs.staticIP;
        doc["gw"] = cs.staticGateway;
        doc["mask"] = cs.staticMask;
        doc["ntp"] = cs.ntpServer;
        
        doc["appMode"] = cs.appMode;
        doc["dispMode"] = cs.displayMode;
        doc["current"] = cs.nixieCurrent;
        doc["brightAuto"] = cs.brightnessAuto;
        doc["brightLv"] = cs.brightnessLevel;
        doc["adcMin"] = cs.adcMinThreshold;
        doc["adcMax"] = cs.adcMaxThreshold;
        doc["psEnable"] = cs.powerSaveEnable;
        doc["psOnHr"] = cs.powerSaveOnHour;
        doc["psOnMin"] = cs.powerSaveOnMin;
        doc["psOffHr"] = cs.powerSaveOffHour;
        doc["psOffMin"] = cs.powerSaveOffMin;
        doc["cumSec"] = cs.cumulativeSeconds;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API: Save Settings
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
        AppSettings ns = settingsManager.get();
        
        String oldSSID = ns.clientSSID;
        String oldPass = ns.clientPass;
        String oldNtp = ns.ntpServer;
        
        ns.clientSSID = json["ssid"].as<String>();
        ns.clientPass = json["pass"].as<String>();
        ns.useDHCP = json["dhcp"].as<bool>();
        ns.staticIP = json["ip"].as<String>();
        ns.staticGateway = json["gw"].as<String>();
        ns.staticMask = json["mask"].as<String>();
        ns.ntpServer = json["ntp"].as<String>();
        ns.appMode = json["appMode"].as<uint8_t>();
        ns.displayMode = json["dispMode"].as<uint8_t>();
        ns.nixieCurrent = json["current"].as<uint8_t>();
        ns.brightnessAuto = json["brightAuto"].as<bool>();
        ns.brightnessLevel = json["brightLv"].as<uint8_t>();
        
        if (json.containsKey("adcMin")) ns.adcMinThreshold = json["adcMin"].as<uint16_t>();
        if (json.containsKey("adcMax")) ns.adcMaxThreshold = json["adcMax"].as<uint16_t>();
        
        ns.powerSaveEnable = json["psEnable"].as<bool>();
        ns.powerSaveOnHour = json["psOnHr"].as<uint8_t>();
        ns.powerSaveOnMin = json["psOnMin"].as<uint8_t>();
        ns.powerSaveOffHour = json["psOffHr"].as<uint8_t>();
        ns.powerSaveOffMin = json["psOffMin"].as<uint8_t>();

        settingsManager.update(ns);
        
        // Reconnect WiFi and restart NTP if SSID/Pass/NTP changed
        if (ns.clientSSID != oldSSID || ns.clientPass != oldPass || ns.ntpServer != oldNtp) {
            WiFi.disconnect();
            ntpConfigured = false;
            if (ns.clientSSID.length() > 0) {
                WiFi.begin(ns.clientSSID.c_str(), ns.clientPass.c_str());
            }
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));

    // API: Sync Time from Browser
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/time", [](AsyncWebServerRequest *request, JsonVariant &json) {
        RTC_Time newTime;
        newTime.year = json["year"].as<uint16_t>();
        newTime.month = json["month"].as<uint8_t>();
        newTime.day = json["day"].as<uint8_t>();
        newTime.hour = json["hour"].as<uint8_t>();
        newTime.min = json["min"].as<uint8_t>();
        newTime.sec = json["sec"].as<uint8_t>();
        newTime.week = json["week"].as<uint8_t>();

        if (xSemaphoreTake(i2cSpiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            rx8900_writeTime(&newTime);
            xSemaphoreGive(i2cSpiMutex);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(500, "application/json", "{\"status\":\"error\"}");
        }
    }));

    // API: Trigger Demo Shuffle
    server.on("/api/demo_shuffle", HTTP_POST, [](AsyncWebServerRequest *request){
        extern volatile bool triggerDemoFlag;
        extern TaskHandle_t sensorTaskHandle;
        triggerDemoFlag = true;
        if (sensorTaskHandle != NULL) {
            xTaskNotifyGive(sensorTaskHandle);
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // API: Get System Status (Sensors)
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"temp\":" + String(currentTemp, 2) + ",";
        json += "\"hum\":" + String(currentHum, 2) + ",";
        json += "\"pres\":" + String(currentPres, 2) + ",";
        json += "\"adc\":" + String(currentAmbientLight) + ",";
        
        String wifiSSID = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Disconnected";
        json += "\"wifi\":\"" + wifiSSID + "\",";
        json += "\"ntpSync\":\"" + lastNtpSyncTime + "\"";
        
        json += "}";
        
        request->send(200, "application/json", json);
    });

    server.begin();
}

void web_server_loop() {
    dnsServer.processNextRequest();
    
    // Check if WiFi connected and NTP needs to be started
    if (!ntpConfigured && WiFi.status() == WL_CONNECTED) {
        AppSettings s = settingsManager.get();
        if (s.ntpServer.length() > 0) {
            configTime(9 * 3600, 0, s.ntpServer.c_str(), "pool.ntp.org", "time.google.com");
            Serial.println("WiFi connected. SNTP initialized.");
        }
        ntpConfigured = true;
    }

    // Fallback: Check if time is synced by polling, in case callback fails
    static uint32_t lastPollTime = 0;
    if (ntpConfigured && millis() - lastPollTime > 5000) {
        lastPollTime = millis();
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year > (2020 - 1900) && lastNtpSyncTime == "Not Synced") {
            // It synced silently!
            ntpTimeSyncedFlag = true;
        }
    }

    // If NTP successfully synced, accurately write it to the RTC at the exact second boundary
    if (ntpTimeSyncedFlag) {
        ntpTimeSyncedFlag = false;
        
        struct timeval tv;
        gettimeofday(&tv, NULL);
        
        // Wait until the start of the next second to write exactly at the boundary
        uint32_t delayMs = 1000 - (tv.tv_usec / 1000);
        vTaskDelay(pdMS_TO_TICKS(delayMs));
        
        // Get the time again right after the boundary
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        RTC_Time newTime;
        newTime.year = timeinfo.tm_year + 1900;
        newTime.month = timeinfo.tm_mon + 1;
        newTime.day = timeinfo.tm_mday;
        newTime.week = timeinfo.tm_wday;
        newTime.hour = timeinfo.tm_hour;
        newTime.min = timeinfo.tm_min;
        newTime.sec = timeinfo.tm_sec;
        
        if (xSemaphoreTake(i2cSpiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            rx8900_writeTime(&newTime);
            xSemaphoreGive(i2cSpiMutex);
            Serial.println("RTC accurately synced with NTP at second boundary.");
            
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", 
                     newTime.year, newTime.month, newTime.day, newTime.hour, newTime.min, newTime.sec);
            lastNtpSyncTime = String(timeStr);
        }
    }
}
