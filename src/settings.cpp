#include "settings.h"

SettingsManager settingsManager;

SettingsManager::SettingsManager() {
    settingsMutex = xSemaphoreCreateMutex();
    secondsSinceLastSave = 0;
}

void SettingsManager::begin() {
    load();
    // Usually I2C is initialized in main.cpp, but we need to ensure the bus is free.
    // We will call loadCumulativeTime from sensorTask which owns the I2C mutex.
}

void SettingsManager::load() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        preferences.begin("nixie_settings", true); // read-only

        currentSettings.clientSSID = preferences.getString("ssid", "");
        currentSettings.clientPass = preferences.getString("pass", "");
        currentSettings.useDHCP = preferences.getBool("dhcp", true);
        currentSettings.staticIP = preferences.getString("ip", "192.168.1.100");
        currentSettings.staticGateway = preferences.getString("gw", "192.168.1.1");
        currentSettings.staticMask = preferences.getString("mask", "255.255.255.0");
        currentSettings.ntpServer = preferences.getString("ntp", "pool.ntp.org");

        currentSettings.appMode = preferences.getUChar("appMode", APP_MODE_CLOCK);
        currentSettings.displayMode = preferences.getUChar("dispMode", DISP_MODE_NORMAL);
        currentSettings.nixieCurrent = preferences.getUChar("current", 23); // Default DAC value for 3mA
        currentSettings.brightnessAuto = preferences.getBool("brightAuto", true);
        currentSettings.brightnessLevel = preferences.getUChar("brightLv", 255);
        currentSettings.adcMinThreshold = preferences.getUShort("adcMin", 500);
        currentSettings.adcMaxThreshold = preferences.getUShort("adcMax", 2500);

        currentSettings.powerSaveEnable = preferences.getBool("psEnable", false);
        currentSettings.powerSaveOnHour = preferences.getUChar("psOnHr", 7);
        currentSettings.powerSaveOnMin = preferences.getUChar("psOnMin", 0);
        currentSettings.powerSaveOffHour = preferences.getUChar("psOffHr", 23);
        currentSettings.powerSaveOffMin = preferences.getUChar("psOffMin", 0);

        preferences.end();
        xSemaphoreGive(settingsMutex);
    }
}

void SettingsManager::save() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        preferences.begin("nixie_settings", false); // read-write

        preferences.putString("ssid", currentSettings.clientSSID);
        preferences.putString("pass", currentSettings.clientPass);
        preferences.putBool("dhcp", currentSettings.useDHCP);
        preferences.putString("ip", currentSettings.staticIP);
        preferences.putString("gw", currentSettings.staticGateway);
        preferences.putString("mask", currentSettings.staticMask);
        preferences.putString("ntp", currentSettings.ntpServer);

        preferences.putUChar("appMode", currentSettings.appMode);
        preferences.putUChar("dispMode", currentSettings.displayMode);
        preferences.putUChar("current", currentSettings.nixieCurrent);
        preferences.putBool("brightAuto", currentSettings.brightnessAuto);
        preferences.putUChar("brightLv", currentSettings.brightnessLevel);
        preferences.putUShort("adcMin", currentSettings.adcMinThreshold);
        preferences.putUShort("adcMax", currentSettings.adcMaxThreshold);

        preferences.putBool("psEnable", currentSettings.powerSaveEnable);
        preferences.putUChar("psOnHr", currentSettings.powerSaveOnHour);
        preferences.putUChar("psOnMin", currentSettings.powerSaveOnMin);
        preferences.putUChar("psOffHr", currentSettings.powerSaveOffHour);
        preferences.putUChar("psOffMin", currentSettings.powerSaveOffMin);

        preferences.end();
        xSemaphoreGive(settingsMutex);
    }
}

// NOTE: Must be called with i2cSpiMutex held
void SettingsManager::loadCumulativeTime() {
    Wire.beginTransmission(FRAM_I2C_ADDR);
    Wire.write(FRAM_ADDR_CUMULATIVE_TIME);
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom(FRAM_I2C_ADDR, 4);
        if (Wire.available() == 4) {
            uint32_t val = 0;
            val |= Wire.read();
            val |= ((uint32_t)Wire.read() << 8);
            val |= ((uint32_t)Wire.read() << 16);
            val |= ((uint32_t)Wire.read() << 24);
            
            // If completely uninitialized (0xFFFFFFFF), set to 0
            if (val == 0xFFFFFFFF) val = 0;
            
            if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
                currentSettings.cumulativeSeconds = val;
                xSemaphoreGive(settingsMutex);
            }
        }
    }
}

// NOTE: Must be called with i2cSpiMutex held
void SettingsManager::saveCumulativeTime() {
    uint32_t val = 0;
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        val = currentSettings.cumulativeSeconds;
        xSemaphoreGive(settingsMutex);
    }
    
    Wire.beginTransmission(FRAM_I2C_ADDR);
    Wire.write(FRAM_ADDR_CUMULATIVE_TIME);
    Wire.write(val & 0xFF);
    Wire.write((val >> 8) & 0xFF);
    Wire.write((val >> 16) & 0xFF);
    Wire.write((val >> 24) & 0xFF);
    Wire.endTransmission();
    
    secondsSinceLastSave = 0;
}

AppSettings SettingsManager::get() {
    AppSettings s;
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        s = currentSettings;
        xSemaphoreGive(settingsMutex);
    }
    return s;
}

void SettingsManager::update(const AppSettings& newSettings) {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        uint32_t cumTime = currentSettings.cumulativeSeconds; // Preserve
        currentSettings = newSettings;
        currentSettings.cumulativeSeconds = cumTime;
        xSemaphoreGive(settingsMutex);
    }
    save(); // Persist to NVS
}

bool SettingsManager::incrementCumulativeTime() {
    bool shouldSave = false;
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        currentSettings.cumulativeSeconds++;
        secondsSinceLastSave++;
        // 3600 seconds = 1 hour
        if (secondsSinceLastSave >= 3600) {
            shouldSave = true;
        }
        xSemaphoreGive(settingsMutex);
    }
    return shouldSave;
}
