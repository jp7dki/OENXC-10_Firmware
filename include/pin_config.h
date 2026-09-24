#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------
// GPIO Pin Mapping for OENXC-10 ESP32
// Based on docs/hardware_spec.md
// ---------------------------------------------------------

// Unused (Set to 0)
#define PIN_UNUSED_0        0

// Nixie Tube Digit Output (Anode Control)
#define PIN_DIGIT1          2
#define PIN_DIGIT2          4
#define PIN_DIGIT3          16
#define PIN_DIGIT4          17
#define PIN_DIGIT5          21
#define PIN_DIGIT6          22

// Nixie Tube Cathode Driver (HV509)
#define PIN_HV509_LE        5   // Latch Enable (Active Low)
#define PIN_HV509_CLK       18  // Clock
#define PIN_HV509_POL_N     19  // Polarity (Active Low)
#define PIN_HV509_DOUT      23  // Data Out

// SPI Bus for BME280
#define PIN_SPI_MISO        12
#define PIN_SPI_MOSI        13
#define PIN_SPI_SCK         14
#define PIN_SPI_CS_BME280   15  // Chip Select (Active High)

// I2C Bus for RTC(RX8900CE) and FRAM(FM24CL64J)
#define PIN_I2C_SCL         32
#define PIN_I2C_SDA         33

// Nixie Tube Power & Current Control
#define PIN_NIXIE_CUR_CTRL  26 // Analog output (DAC) for current control
#define PIN_HV_EN           27 // High Voltage Enable (Active High)

// Other Sensors / Signals
#define PIN_RTC_INT         25 // Interrupt signal for RTC (Active High)
#define PIN_LOW_VOLT_DET    34 // Low voltage detect signal (Active Low)
#define PIN_AMBIENT_LIGHT   36 // Ambient light sensor (Analog input)

#endif // PIN_CONFIG_H
