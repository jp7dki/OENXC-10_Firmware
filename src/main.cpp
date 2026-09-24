#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "pin_config.h"
#include "rx8900ce.h"
#include "settings.h"
#include "web_server.h"

// ---------------------------------------------------------
// FreeRTOS Handles & Globals
// ---------------------------------------------------------
QueueHandle_t displayDataQueue;
SemaphoreHandle_t i2cSpiMutex;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t powerFailTaskHandle = NULL;

// Globals for Web UI Monitoring
volatile float currentTemp = 0.0f;
volatile float currentHum = 0.0f;
volatile float currentPres = 0.0f;
volatile uint16_t currentAmbientLight = 0;

SPIClass hspi(HSPI); // HSPI for BME280
Adafruit_BME280 bme(PIN_SPI_CS_BME280, &hspi);
SPIClass vspi(VSPI); // VSPI for HV509

// ---------------------------------------------------------
// Data Structures
// ---------------------------------------------------------
struct DisplayData {
    uint8_t digitValues[6]; // Values to display on digits
    bool targetPowerOff; // True if display should go into OFF state
    bool triggerHourlyEffect; // True if hourly effect should start
    bool triggerDemoEffect; // True if demo shuffle should start
};

volatile bool triggerDemoFlag = false;

enum PowerState {
    POWER_ON,
    TURNING_OFF_SHUFFLE,
    TURNING_OFF_SEQ,
    POWER_OFF,
    TURNING_ON_SEQ,
    TURNING_ON_SHUFFLE,
    TURNING_ON_RESOLVE,
    HOURLY_SHUFFLE,
    HOURLY_RESOLVE,
    DEMO_SHUFFLE,
    DEMO_RESOLVE
};

// ---------------------------------------------------------
// Task Prototypes & ISR
// ---------------------------------------------------------
void commTask(void *pvParameters);
void displayTask(void *pvParameters);
void sensorTask(void *pvParameters);
void powerFailTask(void *pvParameters);

void IRAM_ATTR lowVoltageISR() {
    digitalWrite(PIN_HV_EN, LOW); // Immediate disable
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (powerFailTaskHandle != NULL) {
        vTaskNotifyGiveFromISR(powerFailTaskHandle, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR rtcInterruptISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (sensorTaskHandle != NULL) {
        vTaskNotifyGiveFromISR(sensorTaskHandle, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ---------------------------------------------------------
// Helper: Shift out data to HV509 via VSPI
// ---------------------------------------------------------
void shiftOutHV509(uint16_t data) {
    digitalWrite(PIN_HV509_LE, LOW);
    delayMicroseconds(1); // Setup time for LE
    
    // Lower to 500kHz (matching previous bit-bang speed) for maximum stability
    vspi.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    vspi.transfer(data >> 8);   // MSB
    vspi.transfer(data & 0xFF); // LSB
    vspi.endTransaction();
    
    delayMicroseconds(1); // Ensure last clock edge has settled before latching
    digitalWrite(PIN_HV509_LE, HIGH);
    delayMicroseconds(1); // Latch pulse width
}

// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("Starting OENXC-10 Firmware...");

    // 1. Initialize Mutex and Queue
    i2cSpiMutex = xSemaphoreCreateMutex();
    displayDataQueue = xQueueCreate(10, sizeof(DisplayData));

    if (i2cSpiMutex == NULL || displayDataQueue == NULL) {
        Serial.println("Failed to create FreeRTOS objects!");
        while(1) { delay(100); }
    }

    // 2. Initialize GPIOs based on hardware_spec.md
    pinMode(PIN_UNUSED_0, OUTPUT);
    digitalWrite(PIN_UNUSED_0, LOW);

    // Anodes (Active High)
    pinMode(PIN_DIGIT1, OUTPUT); digitalWrite(PIN_DIGIT1, LOW);
    pinMode(PIN_DIGIT2, OUTPUT); digitalWrite(PIN_DIGIT2, LOW);
    pinMode(PIN_DIGIT3, OUTPUT); digitalWrite(PIN_DIGIT3, LOW);
    pinMode(PIN_DIGIT4, OUTPUT); digitalWrite(PIN_DIGIT4, LOW);
    pinMode(PIN_DIGIT5, OUTPUT); digitalWrite(PIN_DIGIT5, LOW);
    pinMode(PIN_DIGIT6, OUTPUT); digitalWrite(PIN_DIGIT6, LOW);

    // Cathode Driver HV509 (CLK and DOUT are handled by VSPI)
    pinMode(PIN_HV509_CLK, OUTPUT); digitalWrite(PIN_HV509_CLK, LOW);
    pinMode(PIN_HV509_DOUT, OUTPUT); digitalWrite(PIN_HV509_DOUT, LOW);

    // Power / Current Control
    pinMode(PIN_HV_EN, OUTPUT); 
    digitalWrite(PIN_HV_EN, LOW); // High Voltage Disabled initially
    
    dacWrite(PIN_NIXIE_CUR_CTRL, 23);

    // Inputs
    pinMode(PIN_RTC_INT, INPUT_PULLUP); // RTC /INT is open-drain, active low
    pinMode(PIN_LOW_VOLT_DET, INPUT_PULLUP); // Active Low
    
    // Configure ADC Reference Voltage to 3.3V (11dB Attenuation)
    analogSetPinAttenuation(PIN_AMBIENT_LIGHT, ADC_11db);
    analogReadResolution(12); // 12-bit resolution (0-4095)
    
    // 3. Initialize I2C and SPI
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    hspi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1); // HSPI (Do not map SS, Adafruit library handles CS via GPIO)
    
    // Ensure BME280 CS is OUTPUT and HIGH
    pinMode(PIN_SPI_CS_BME280, OUTPUT);
    digitalWrite(PIN_SPI_CS_BME280, HIGH);
    
    vspi.begin(PIN_HV509_CLK, -1, PIN_HV509_DOUT, -1); // VSPI (No MISO needed)
    
    // CRITICAL FIX: PIN_HV509_POL_N (GPIO19) and PIN_HV509_LE (GPIO5) are default VSPI MISO and SS pins.
    // vspi.begin() may silently claim them. We MUST set them as OUTPUT *after* vspi.begin() to detach them from the SPI peripheral.
    pinMode(PIN_HV509_POL_N, OUTPUT); 
    digitalWrite(PIN_HV509_POL_N, HIGH); // POL_N High = No inversion
    
    pinMode(PIN_HV509_LE, OUTPUT); 
    digitalWrite(PIN_HV509_LE, HIGH);    // LE High = Latch disabled/transparent
    
    // Ensure all cathodes are H (Nixie OFF) immediately at boot
    shiftOutHV509(0xFFFF);

    // Attach Interrupts
    attachInterrupt(digitalPinToInterrupt(PIN_RTC_INT), rtcInterruptISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_LOW_VOLT_DET), lowVoltageISR, FALLING);

    // 4. Create Tasks on specified Cores
    xTaskCreatePinnedToCore(
        powerFailTask,
        "PowerFailTask",
        2048,
        NULL,
        configMAX_PRIORITIES - 1, // Highest priority
        &powerFailTaskHandle,
        1                       // Core ID (Core 1)
    );
    xTaskCreatePinnedToCore(
        commTask,               // Task function
        "CommTask",             // Task name
        8192,                   // Stack size (bytes) - increased for Web/WiFi/JSON
        NULL,                   // Task parameters
        3,                      // Priority
        NULL,                   // Task handle
        0                       // Core ID (Core 0)
    );

    xTaskCreatePinnedToCore(
        displayTask,
        "DisplayTask",
        3072,
        NULL,
        2,                      // Higher Priority on Core 1 for display timing
        NULL,
        1                       // Core ID (Core 1)
    );

    xTaskCreatePinnedToCore(
        sensorTask,
        "SensorTask",
        3072,
        NULL,
        1,                      // Lower Priority
        &sensorTaskHandle,      // Save handle for ISR
        1                       // Core ID (Core 1)
    );
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ---------------------------------------------------------
// Task Implementations
// ---------------------------------------------------------

/**
 * @brief Communication and Protocol Task (Core 0)
 */
void commTask(void *pvParameters) {
    // Wait for mutex to be created
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize settings (Preferences)
    settingsManager.begin();
    
    // Initialize Web Server & WiFi
    web_server_init();

    for (;;) {
        // Process Captive Portal DNS requests and NTP logic
        web_server_loop();
        
        // Yield briefly to prevent watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Helper to set Anode pins
void setAnodesOff() {
    digitalWrite(PIN_DIGIT1, LOW);
    digitalWrite(PIN_DIGIT2, LOW);
    digitalWrite(PIN_DIGIT3, LOW);
    digitalWrite(PIN_DIGIT4, LOW);
    digitalWrite(PIN_DIGIT5, LOW);
    digitalWrite(PIN_DIGIT6, LOW);
}

void setAnodeOn(uint8_t digit) {
    switch(digit) {
        case 0: digitalWrite(PIN_DIGIT1, HIGH); break;
        case 1: digitalWrite(PIN_DIGIT2, HIGH); break;
        case 2: digitalWrite(PIN_DIGIT3, HIGH); break;
        case 3: digitalWrite(PIN_DIGIT4, HIGH); break;
        case 4: digitalWrite(PIN_DIGIT5, HIGH); break;
        case 5: digitalWrite(PIN_DIGIT6, HIGH); break;
    }
}

uint16_t getCathodeData(uint8_t digitValue) {
    uint16_t cathodeData = 0xFFFF; // Default all OFF (1 = driver off)
    if (digitValue <= 9) {
        uint8_t bitIndex = (digitValue == 0) ? 9 : (digitValue - 1);
        cathodeData &= ~(1 << bitIndex); // Set target bit to 0 (driver on -> Nixie ON)
    }
    return cathodeData;
}

/**
 * @brief Nixie Tube Dynamic Display Task (Core 1)
 */
void displayTask(void *pvParameters) {
    DisplayData incomingData = {0};
    uint8_t currentDigitValues[6] = {0,0,0,0,0,0};
    uint8_t targetDigitValues[6] = {0,0,0,0,0,0};
    float fadeProgress = 1.0; // 1.0 means transition is complete

    PowerState pwrState = POWER_OFF; // Start OFF to trigger turn-on animation at boot
    uint32_t stateStartTime = 0;
    uint32_t lastShuffleTime = 0;
    uint8_t shuffleDigits[6] = {0,0,0,0,0,0};
    uint8_t actualTargetValues[6] = {0,0,0,0,0,0}; // The final values given to display logic
    uint8_t lockedTargetValues[6] = {0,0,0,0,0,0}; // Frozen target values for effects
    
    // --- Initialization Sequence ---
    setAnodesOff();
    
    // HV Enable is now controlled by the state machine. Ensure it's off during boot.
    digitalWrite(PIN_HV_EN, LOW);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Shift out all 1s (1 = Driver OFF / Cathode High = Nixie OFF)
    shiftOutHV509(0xFFFF);
    
    const TickType_t xFrequency = pdMS_TO_TICKS(3); // 3ms x 6 = 18ms (approx 55Hz)
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t currentDigit = 0;

    for (;;) {
        // Check for new display data
        if (xQueueReceive(displayDataQueue, &incomingData, 0) == pdTRUE) {
            // Check for Power State transition requests
            if (pwrState == POWER_ON && incomingData.targetPowerOff) {
                pwrState = TURNING_OFF_SHUFFLE;
                stateStartTime = millis();
            } else if (pwrState == POWER_OFF && !incomingData.targetPowerOff) {
                pwrState = TURNING_ON_SEQ;
                stateStartTime = millis();
                digitalWrite(PIN_HV_EN, HIGH);
            } else if (pwrState == POWER_ON && incomingData.triggerHourlyEffect) {
                pwrState = HOURLY_SHUFFLE;
                stateStartTime = millis();
                
                // Pre-calculate target time 6 seconds after 00:00:00 (3s shuffle + 3s resolve)
                lockedTargetValues[0] = incomingData.digitValues[0];
                lockedTargetValues[1] = incomingData.digitValues[1];
                lockedTargetValues[2] = 0;
                lockedTargetValues[3] = 0;
                lockedTargetValues[4] = 0;
                lockedTargetValues[5] = 6;
            } else if (pwrState == POWER_ON && incomingData.triggerDemoEffect) {
                pwrState = DEMO_SHUFFLE;
                stateStartTime = millis();
                for (int i=0; i<6; i++) lockedTargetValues[i] = incomingData.digitValues[i];
            }
            
            bool changed = false;
            for (int i=0; i<6; i++) {
                if (incomingData.digitValues[i] != targetDigitValues[i]) {
                    changed = true;
                    break;
                }
            }
            if (changed) {
                // Start a new transition
                for (int i=0; i<6; i++) {
                    currentDigitValues[i] = targetDigitValues[i];
                    targetDigitValues[i] = incomingData.digitValues[i];
                }
                fadeProgress = 0.0;
            }
        }
        
        // --- State Machine Update ---
        uint32_t now = millis();
        // Update shuffle digits every 2ms
        if (now - lastShuffleTime >= 2) {
            lastShuffleTime = now;
            for (int i=0; i<6; i++) {
                shuffleDigits[i] = random(0, 10);
            }
        }
        
        if (pwrState == TURNING_OFF_SHUFFLE) {
            if (now - stateStartTime >= 1000) {
                pwrState = TURNING_OFF_SEQ;
                stateStartTime = now;
            }
        } else if (pwrState == TURNING_OFF_SEQ) {
            // 6 digits, 100ms each = 600ms
            if (now - stateStartTime >= 600) {
                pwrState = POWER_OFF;
                digitalWrite(PIN_HV_EN, LOW);
            }
        } else if (pwrState == TURNING_ON_SEQ) {
            if (now - stateStartTime >= 600) {
                pwrState = TURNING_ON_SHUFFLE;
                stateStartTime = now;
            }
        } else if (pwrState == TURNING_ON_SHUFFLE) {
            if (now - stateStartTime >= 1000) {
                pwrState = TURNING_ON_RESOLVE;
                stateStartTime = now;
            }
        } else if (pwrState == TURNING_ON_RESOLVE) {
            if (now - stateStartTime >= 600) {
                pwrState = POWER_ON;
                fadeProgress = 1.0;
                for (int i=0; i<6; i++) currentDigitValues[i] = targetDigitValues[i];
            }
        } else if (pwrState == HOURLY_SHUFFLE) {
            if (now - stateStartTime >= 3000) {
                pwrState = HOURLY_RESOLVE;
                stateStartTime = now;
            }
        } else if (pwrState == HOURLY_RESOLVE) {
            // 6 digits, 500ms each = 3000ms
            if (now - stateStartTime >= 3000) {
                pwrState = POWER_ON;
                fadeProgress = 1.0;
                for (int i=0; i<6; i++) currentDigitValues[i] = targetDigitValues[i];
            }
        } else if (pwrState == DEMO_SHUFFLE) {
            if (now - stateStartTime >= 5000) {
                pwrState = DEMO_RESOLVE;
                stateStartTime = now;
            }
        } else if (pwrState == DEMO_RESOLVE) {
            // 6 digits, 500ms each = 3000ms
            if (now - stateStartTime >= 3000) {
                pwrState = POWER_ON;
                fadeProgress = 1.0;
                for (int i=0; i<6; i++) currentDigitValues[i] = targetDigitValues[i];
            }
        }
        
        // Determine actualTargetValues based on state
        if (pwrState == POWER_ON) {
            for (int i=0; i<6; i++) actualTargetValues[i] = targetDigitValues[i];
        } else if (pwrState == TURNING_OFF_SHUFFLE || pwrState == TURNING_ON_SHUFFLE) {
            for (int i=0; i<6; i++) actualTargetValues[i] = shuffleDigits[i];
        } else if (pwrState == TURNING_OFF_SEQ) {
            uint32_t elapsed = now - stateStartTime;
            int numOff = elapsed / 100; // 0 to 5
            for (int i=0; i<6; i++) {
                // index 5 is rightmost. we turn off rightmost first.
                // so if numOff is 1, index 5 is off.
                // if numOff is 2, index 5 and 4 are off.
                if (5 - i < numOff) actualTargetValues[i] = 0xFF; // OFF
                else actualTargetValues[i] = shuffleDigits[i];
            }
        } else if (pwrState == TURNING_ON_SEQ) {
            uint32_t elapsed = now - stateStartTime;
            int numOn = elapsed / 100; // 0 to 5
            for (int i=0; i<6; i++) {
                // turn on rightmost first.
                if (5 - i <= numOn) actualTargetValues[i] = shuffleDigits[i];
                else actualTargetValues[i] = 0xFF; // OFF
            }
        } else if (pwrState == TURNING_ON_RESOLVE) {
            uint32_t elapsed = now - stateStartTime;
            int numResolved = elapsed / 100; // 0 to 5
            for (int i=0; i<6; i++) {
                // stop shuffling from rightmost first and lock into target digit.
                if (5 - i <= numResolved) actualTargetValues[i] = targetDigitValues[i];
                else actualTargetValues[i] = shuffleDigits[i];
            }
        } else if (pwrState == HOURLY_SHUFFLE || pwrState == DEMO_SHUFFLE) {
            for (int i=0; i<6; i++) actualTargetValues[i] = shuffleDigits[i];
        } else if (pwrState == HOURLY_RESOLVE || pwrState == DEMO_RESOLVE) {
            uint32_t elapsed = now - stateStartTime;
            int numResolved = elapsed / 500; // 0 to 5
            for (int i=0; i<6; i++) {
                // stop shuffling from rightmost first and lock into PREDICTED target digit.
                if (5 - i <= numResolved) actualTargetValues[i] = lockedTargetValues[i];
                else actualTargetValues[i] = shuffleDigits[i];
            }
        } else {
            // POWER_OFF
            for (int i=0; i<6; i++) actualTargetValues[i] = 0xFF;
        }

        AppSettings s = settingsManager.get();

        // Advance fade progress (progress per digit slot)
        if (fadeProgress < 1.0) {
            if (s.displayMode == DISP_MODE_NORMAL) {
                fadeProgress = 1.0;
            } else {
                // 18ms per full cycle of 6 digits.
                // 0.01 progress per slot = 0.06 per cycle. 1.0 / 0.06 = ~16 cycles = ~300ms transition
                fadeProgress += 0.01;
                if (fadeProgress > 1.0) fadeProgress = 1.0;
            }
        }

        // --- Auto Brightness Logic ---
        static float smoothAutoBrightness = 255.0f;
        if (s.brightnessAuto) {
            // Map currentAmbientLight (0-4095) to target brightness (20-255)
            // Adjust thresholds from Web UI settings:
            float target = 255.0f;
            float minThresh = (float)s.adcMinThreshold;
            float maxThresh = (float)s.adcMaxThreshold;
            
            // Prevent division by zero if configured incorrectly
            if (maxThresh <= minThresh) maxThresh = minThresh + 1.0f;
            
            if (currentAmbientLight < s.adcMinThreshold) {
                target = 20.0f;
            } else if (currentAmbientLight > s.adcMaxThreshold) {
                target = 255.0f;
            } else {
                target = 20.0f + ((float)(currentAmbientLight - s.adcMinThreshold) * (255.0f - 20.0f)) / (maxThresh - minThresh);
            }
            
            // Apply low-pass filter to prevent flickering from ADC noise or passing shadows
            smoothAutoBrightness += (target - smoothAutoBrightness) * 0.01f;
            s.brightnessLevel = (uint8_t)smoothAutoBrightness;
        } else {
            smoothAutoBrightness = s.brightnessLevel; // Keep synced for when Auto is turned on
        }

        // Calculate max ON time based on brightness level (0-255).
        // Slot is 3000us. Leave at least 200us for blanking/overhead.
        // maxOnTime ranges from approx 100us to 2800us.
        uint32_t maxOnTime = 100 + ((uint32_t)s.brightnessLevel * 2700) / 255; 

        uint32_t oldOnTime = 0;
        uint32_t newOnTime = 0;

        if (pwrState != POWER_ON) {
            // Bypass crossfading during power transition effects
            newOnTime = maxOnTime;
        } else if (fadeProgress >= 1.0 || currentDigitValues[currentDigit] == targetDigitValues[currentDigit]) {
            newOnTime = maxOnTime;
        } else {
            if (s.displayMode == DISP_MODE_FADE) {
                // Fade out old, then fade in new
                if (fadeProgress < 0.5) {
                    oldOnTime = (uint32_t)(maxOnTime * (1.0 - (fadeProgress * 2.0)));
                } else {
                    newOnTime = (uint32_t)(maxOnTime * ((fadeProgress - 0.5) * 2.0));
                }
            } else if (s.displayMode == DISP_MODE_CROSSFADE) {
                // Crossfade
                oldOnTime = (uint32_t)(maxOnTime * (1.0 - fadeProgress));
                newOnTime = (uint32_t)(maxOnTime * fadeProgress);
            }
        }

        // --- Hardware Output Sequence ---
        // 1. All anodes OFF (Blanking)
        setAnodesOff();

        // 2. Output Old Digit (Only if normal running state)
        if (oldOnTime > 0 && pwrState == POWER_ON) {
            shiftOutHV509(getCathodeData(currentDigitValues[currentDigit]));
            setAnodeOn(currentDigit);
            delayMicroseconds(oldOnTime);
            setAnodesOff(); // Blank between old and new
        }

        // 3. Output New/Effect Digit
        if (newOnTime > 0) {
            uint8_t dispVal = (pwrState == POWER_ON) ? targetDigitValues[currentDigit] : actualTargetValues[currentDigit];
            if (dispVal != 0xFF) { // 0xFF means completely off
                shiftOutHV509(getCathodeData(dispVal));
                setAnodeOn(currentDigit);
                delayMicroseconds(newOnTime);
                setAnodesOff();
            }
        }

        // Move to next digit
        currentDigit = (currentDigit + 1) % 6;

        // Wait exact period for next digit slot (3ms)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Sensor read task (Core 1)
 */
void sensorTask(void *pvParameters) {
    bool bmeStatus = false;
    
    // 1. Initialize RTC, BME280, and FRAM Cumulative Time
    if (xSemaphoreTake(i2cSpiMutex, portMAX_DELAY) == pdTRUE) {
        rx8900_init();
        
        // Attempt first BME280 initialization
        bmeStatus = bme.begin();
        if (!bmeStatus) {
            Serial.println("Initial BME280 begin() failed. Will retry.");
        } else {
            Serial.println("BME280 Sensor initialized successfully.");
        }
        
        settingsManager.loadCumulativeTime();
        
        xSemaphoreGive(i2cSpiMutex);
    }
    
    uint8_t bmeTick = 10; // Set to 10 to trigger reading immediately on first loop

    for (;;) {
        // 2. Wait for 1-second interrupt notification from ISR (with 1.5s fallback timeout)
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1500));
        
        if (notified == 0) {
            Serial.println("RTC Interrupt Timeout! Polling fallback...");
        }
        
        RTC_Time time;
        bool readSuccess = false;
        
        AppSettings s = settingsManager.get();
        static int lastHourProcessed = -1;

        if (xSemaphoreTake(i2cSpiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            readSuccess = rx8900_readTime(&time);
            // Always attempt to clear interrupt flag to release the INT pin if it was stuck
            rx8900_clearInterrupt(); 
            xSemaphoreGive(i2cSpiMutex);
            
            // Power Saving Logic
            bool powerIsOff = false;
            bool triggerHourly = false;
            
            if (s.powerSaveEnable && readSuccess) {
                uint16_t currentMins = time.hour * 60 + time.min;
                uint16_t onMins = s.powerSaveOnHour * 60 + s.powerSaveOnMin;
                uint16_t offMins = s.powerSaveOffHour * 60 + s.powerSaveOffMin;
                
                if (onMins < offMins) {
                    // e.g. ON at 07:00, OFF at 23:00.
                    if (currentMins < onMins || currentMins >= offMins) powerIsOff = true;
                } else if (onMins > offMins) {
                    // e.g. ON at 18:00, OFF at 06:00.
                    if (currentMins >= offMins && currentMins < onMins) powerIsOff = true;
                }
            }

            if (readSuccess) {
                // Check for top-of-hour effect (only if power is ON)
                if (!powerIsOff) {
                    if (time.min == 0 && time.sec == 0) {
                        if (lastHourProcessed != time.hour) {
                            triggerHourly = true;
                            lastHourProcessed = time.hour;
                        }
                    } else if (time.min != 0) {
                        // Reset when it's no longer the 0th minute to prepare for next hour
                        lastHourProcessed = -1;
                    }
                }

                // Increment cumulative time and save to FRAM if needed (every hour)
                if (settingsManager.incrementCumulativeTime()) {
                    settingsManager.saveCumulativeTime();
                }

                DisplayData data;
                
                if (s.appMode == APP_MODE_RANDOM_DEMO) {
                    static uint8_t currentDemoDigits[6] = {0,0,0,0,0,0};
                    if (triggerDemoFlag) {
                        triggerDemoFlag = false;
                        data.triggerDemoEffect = true;
                        for (int i=0; i<6; i++) currentDemoDigits[i] = random(0, 10);
                    } else {
                        data.triggerDemoEffect = false;
                    }
                    for (int i=0; i<6; i++) data.digitValues[i] = currentDemoDigits[i];
                    data.triggerHourlyEffect = false;
                } else {
                    // Format HHMMSS (e.g. 12:34:56 -> 1,2,3,4,5,6)
                    data.digitValues[0] = time.hour / 10;
                    data.digitValues[1] = time.hour % 10;
                    data.digitValues[2] = time.min / 10;
                    data.digitValues[3] = time.min % 10;
                    data.digitValues[4] = time.sec / 10;
                    data.digitValues[5] = time.sec % 10;
                    data.triggerHourlyEffect = triggerHourly;
                    data.triggerDemoEffect = false;
                }
                data.targetPowerOff = powerIsOff;

                // Send to display task
                xQueueSend(displayDataQueue, &data, pdMS_TO_TICKS(10));
            } else {
                Serial.println("Failed to read RTC time via I2C");
            }
        }
        
        // --- Apply Settings (DAC, Power Save) ---
        // DAC Current Control (Max 50 to prevent damage)
        uint8_t dacVal = s.nixieCurrent;
        if (dacVal > 50) dacVal = 50;
        dacWrite(PIN_NIXIE_CUR_CTRL, dacVal);
        
        // 3. Read Ambient Light ADC
        uint16_t rawAdc = analogRead(PIN_AMBIENT_LIGHT);
        currentAmbientLight = rawAdc;
        
        // 4. Read BME280 every 10 seconds (or retry initialization)
        bmeTick++;
        if (bmeTick >= 10) {
            bmeTick = 0;
            if (xSemaphoreTake(i2cSpiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!bmeStatus) {
                    bmeStatus = bme.begin();
                }
                
                if (bmeStatus) {
                    float temp = bme.readTemperature();
                    float hum = bme.readHumidity();
                    float pres = bme.readPressure() / 100.0F; // hPa
                    
                    if (!isnan(temp)) {
                        currentTemp = temp;
                        currentHum = hum;
                        currentPres = pres;
                    }
                }
                xSemaphoreGive(i2cSpiMutex);
            }
        }
        
        // Debug Output every second
        Serial.printf("[SensorTask] BME_OK: %d, Temp: %.2f, Hum: %.2f, Pres: %.2f, ADC: %d\n", 
            bmeStatus, currentTemp, currentHum, currentPres, currentAmbientLight);
    }
}
void powerFailTask(void *pvParameters) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Power is failing. Try to take the I2C mutex.
        // If another task has it, priority inheritance will boost them to finish quickly.
        if (xSemaphoreTake(i2cSpiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            settingsManager.saveCumulativeTime();
            xSemaphoreGive(i2cSpiMutex);
        }
        
        // Loop forever after saving, waiting for actual power loss or hard reset
        while(true) {
            vTaskDelay(portMAX_DELAY);
        }
    }
}

