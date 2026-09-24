#include "rx8900ce.h"
#include <Wire.h>

// Helper to convert BCD to Decimal
static uint8_t bcd2dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

// Helper to convert Decimal to BCD
static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

// Write to a single register
static void writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(RX8900CE_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

// Read from a single register
static uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(RX8900CE_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)RX8900CE_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

void rx8900_init() {
    uint8_t flag = readReg(RX8900_FLAG);
    
    // VDET (Bit 1): Voltage Detect, VLF (Bit 1 in some docs, actually VDET is bit 1)
    // Bit 1 = VDET. If 1, RTC lost power and time is invalid.
    if ((flag & 0x02) != 0) {
        Serial.println("RTC Power Loss Detected. Setting default time...");
        
        // Default time: 2024-01-01 12:00:00
        RTC_Time defaultTime;
        defaultTime.year = 2024;
        defaultTime.month = 1;
        defaultTime.day = 1;
        defaultTime.week = 1; // Monday
        defaultTime.hour = 12;
        defaultTime.min = 0;
        defaultTime.sec = 0;
        
        rx8900_writeTime(&defaultTime);
    }
    
    // ALWAYS clear VDET, UF and other flags so the INT pin is released
    writeReg(RX8900_FLAG, 0x00);
    
    // Setup for 1Hz interrupt on /INT pin
    // Extension Register (0x0D): 
    // USEL (Update Interrupt Select) = 0 (1 Hz) or 1 (minute)
    // TE (Timer Enable) = 0
    // Set USEL = 0 (Bit 5 = 0)
    uint8_t ext = readReg(RX8900_EXT);
    ext &= ~(1 << 5); 
    writeReg(RX8900_EXT, ext);
    
    // Control Register (0x0F):
    // UIE (Update Interrupt Enable) = 1 (Bit 5 = 1)
    uint8_t ctrl = readReg(RX8900_CTRL);
    ctrl |= (1 << 5); 
    writeReg(RX8900_CTRL, ctrl);
}

bool rx8900_readTime(RTC_Time *time) {
    if (!time) return false;
    
    Wire.beginTransmission(RX8900CE_I2C_ADDR);
    Wire.write(RX8900_SEC);
    Wire.endTransmission();
    
    Wire.requestFrom((uint8_t)RX8900CE_I2C_ADDR, (uint8_t)7);
    if (Wire.available() == 7) {
        time->sec   = bcd2dec(Wire.read() & 0x7F);
        time->min   = bcd2dec(Wire.read() & 0x7F);
        time->hour  = bcd2dec(Wire.read() & 0x3F);
        time->week  = Wire.read() & 0x7F; // 1 << n format, but usually handled as a value. Keep it as BCD raw or convert. Actually, week is a 7-bit flag (1<<0 to 1<<6). We can just store it.
        time->day   = bcd2dec(Wire.read() & 0x3F);
        time->month = bcd2dec(Wire.read() & 0x1F);
        time->year  = bcd2dec(Wire.read()) + 2000;
        return true;
    }
    return false;
}

bool rx8900_writeTime(const RTC_Time *time) {
    if (!time) return false;
    
    Wire.beginTransmission(RX8900CE_I2C_ADDR);
    Wire.write(RX8900_SEC);
    Wire.write(dec2bcd(time->sec));
    Wire.write(dec2bcd(time->min));
    Wire.write(dec2bcd(time->hour));
    Wire.write(time->week); // Raw week
    Wire.write(dec2bcd(time->day));
    Wire.write(dec2bcd(time->month));
    Wire.write(dec2bcd(time->year % 100));
    Wire.endTransmission();
    
    return true;
}

void rx8900_clearInterrupt() {
    uint8_t flag = readReg(RX8900_FLAG);
    flag &= ~(1 << 5); // Clear UF (Update Flag, bit 5)
    writeReg(RX8900_FLAG, flag);
}
