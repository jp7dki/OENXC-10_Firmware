#ifndef RX8900CE_H
#define RX8900CE_H

#include <Arduino.h>

#define RX8900CE_I2C_ADDR 0x32

// Register Map
#define RX8900_SEC      0x00
#define RX8900_MIN      0x01
#define RX8900_HOUR     0x02
#define RX8900_WEEK     0x03
#define RX8900_DAY      0x04
#define RX8900_MONTH    0x05
#define RX8900_YEAR     0x06

#define RX8900_RAM      0x07
#define RX8900_MIN_ALM  0x08
#define RX8900_HR_ALM   0x09
#define RX8900_WK_ALM   0x0A

#define RX8900_TIM_CNT0 0x0B
#define RX8900_TIM_CNT1 0x0C

#define RX8900_EXT      0x0D
#define RX8900_FLAG     0x0E
#define RX8900_CTRL     0x0F

// RTC Time Structure
struct RTC_Time {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t week; // 0-6
    uint8_t day;
    uint8_t month;
    uint16_t year; // e.g. 2024
};

// Initialize the RTC, check for power loss, set default time if lost, and enable 1Hz INT
void rx8900_init();

// Read time from RTC
bool rx8900_readTime(RTC_Time *time);

// Write time to RTC
bool rx8900_writeTime(const RTC_Time *time);

// Clear the Update Interrupt Flag
void rx8900_clearInterrupt();

#endif // RX8900CE_H
