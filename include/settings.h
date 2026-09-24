#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#define FRAM_I2C_ADDR 0x50
#define FRAM_ADDR_CUMULATIVE_TIME 0x00

// App Modes
enum AppMode {
    APP_MODE_CLOCK = 0,
    APP_MODE_RANDOM_DEMO = 1
};

// Display Modes
enum DisplayMode {
    DISP_MODE_NORMAL = 0,
    DISP_MODE_FADE = 1,
    DISP_MODE_CROSSFADE = 2
};

struct AppSettings {
    // WiFi & Network
    String clientSSID;
    String clientPass;
    bool useDHCP;
    String staticIP;
    String staticGateway;
    String staticMask;
    String ntpServer;

    // Operation Mode
    uint8_t appMode;

    // Display
    uint8_t displayMode;
    uint8_t nixieCurrent; // DAC value 0-255
    bool brightnessAuto;
    uint8_t brightnessLevel; // 0-255
    uint16_t adcMinThreshold; // Default 500
    uint16_t adcMaxThreshold; // Default 2500

    // Power Saving
    bool powerSaveEnable;
    uint8_t powerSaveOnHour;
    uint8_t powerSaveOnMin;
    uint8_t powerSaveOffHour;
    uint8_t powerSaveOffMin;
    
    // Status (not saved to NVS)
    uint32_t cumulativeSeconds; 
};

class SettingsManager {
public:
    SettingsManager();
    
    void begin();
    void load();
    void save();
    
    // FRAM specific
    void loadCumulativeTime();
    void saveCumulativeTime();
    
    // Thread-safe access to settings
    AppSettings get();
    void update(const AppSettings& newSettings);
    
    // Atomically increment cumulative time (call once per second)
    // Returns true if an hour has passed and we should save to FRAM
    bool incrementCumulativeTime();

private:
    Preferences preferences;
    AppSettings currentSettings;
    SemaphoreHandle_t settingsMutex;
    uint16_t secondsSinceLastSave;
};

extern SettingsManager settingsManager;

// Global sensor values
extern volatile float currentTemp;
extern volatile float currentHum;
extern volatile float currentPres;
extern volatile uint16_t currentAmbientLight;

#endif // SETTINGS_H
