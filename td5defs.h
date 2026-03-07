/*
Td5Defs.h - ESP32 Port (Simplified for RPM + Temperature display)

Original: td5opencomstm32 by Luca Veronesi & BennehBoy
ESP32 Port: 2024

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#ifndef Td5Defs_h
#define Td5Defs_h

// Debug mode: enables debug output on serial monitor
#define _DEBUG_

// ============================================
// ESP32 Pin Definitions
// ============================================

// K-Line Communication (Serial2)
#define K_OUT                17    // TX2 - K-Line TX
#define K_IN                 16    // RX2 - K-Line RX

// OLED Display (I2C - SSD1306 128x64)
#define OLED_SDA             21    // I2C SDA
#define OLED_SCL             22    // I2C SCL
#define OLED_ADDR            0x3C  // I2C Address

// LED
#define ledPin               2     // Built-in LED on most ESP32 boards

// Serial for K-Line
#define obdSerial           Serial2

// Key generation structure (for ECU authentication)
typedef union {
    uint16_t keyword;
    struct {
        uint8_t low_byte;
        uint8_t high_byte;
    };
} keyBytes_t;

#endif
