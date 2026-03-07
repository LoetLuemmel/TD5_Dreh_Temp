/*
Td5Hmi.h - ESP32 Port (Simplified for RPM + Temperature display)

Original: td5opencomstm32 by Luca Veronesi & BennehBoy
ESP32 Port: 2024

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#ifndef Td5Hmi_h
#define Td5Hmi_h

#include <Adafruit_SSD1306.h>
#include "td5defs.h"

// Graph configuration
#define GRAPH_WIDTH     108   // Pixel width for temperature curve
#define GRAPH_HEIGHT     40   // Pixel height
#define GRAPH_X_START    18   // X position (space for labels)
#define GRAPH_Y_START    22   // Y position
#define TEMP_MAX         90   // Maximum temperature for scaling

// Functions
void print_connect_screen();
void print_rpm_temp_screen(int rpm, float coolant_temp);
void updateTempHistory(float temp);

#endif
