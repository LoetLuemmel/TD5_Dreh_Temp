/*
Td5Hmi.cpp - ESP32 Port (Simplified for RPM + Temperature display)

Original: td5opencomstm32 by Luca Veronesi & BennehBoy
ESP32 Port: 2024

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#include "td5hmi.h"
#include "td5comm.h"

extern Adafruit_SSD1306 lcd;

// Ring buffer for temperature history
static float tempHistory[GRAPH_WIDTH];
static int tempHistoryIndex = 0;
static bool historyInitialized = false;

// Update temperature history (call this when new temperature data arrives)
void updateTempHistory(float temp)
{
  // Initialize buffer with first value
  if (!historyInitialized)
  {
    for (int i = 0; i < GRAPH_WIDTH; i++)
    {
      tempHistory[i] = temp;
    }
    historyInitialized = true;
  }

  tempHistory[tempHistoryIndex] = temp;
  tempHistoryIndex = (tempHistoryIndex + 1) % GRAPH_WIDTH;
}

// Draw temperature graph
static void drawTempGraph()
{
  // Draw frame
  lcd.drawRect(GRAPH_X_START, GRAPH_Y_START, GRAPH_WIDTH, GRAPH_HEIGHT, WHITE);

  // Y-axis labels
  lcd.setTextSize(1);
  lcd.setCursor(0, GRAPH_Y_START);
  lcd.print("90");
  lcd.setCursor(0, GRAPH_Y_START + GRAPH_HEIGHT - 8);
  lcd.print(" 0");

  // Draw temperature curve
  for (int i = 0; i < GRAPH_WIDTH - 2; i++)
  {
    int idx1 = (tempHistoryIndex + i) % GRAPH_WIDTH;
    int idx2 = (tempHistoryIndex + i + 1) % GRAPH_WIDTH;

    // Calculate Y positions (invert because screen Y grows downward)
    float temp1 = constrain(tempHistory[idx1], 0, TEMP_MAX);
    float temp2 = constrain(tempHistory[idx2], 0, TEMP_MAX);

    int y1 = GRAPH_Y_START + GRAPH_HEIGHT - 2 -
             (int)(temp1 / TEMP_MAX * (GRAPH_HEIGHT - 4));
    int y2 = GRAPH_Y_START + GRAPH_HEIGHT - 2 -
             (int)(temp2 / TEMP_MAX * (GRAPH_HEIGHT - 4));

    // Constrain to graph area
    y1 = constrain(y1, GRAPH_Y_START + 1, GRAPH_Y_START + GRAPH_HEIGHT - 2);
    y2 = constrain(y2, GRAPH_Y_START + 1, GRAPH_Y_START + GRAPH_HEIGHT - 2);

    lcd.drawLine(GRAPH_X_START + 1 + i, y1, GRAPH_X_START + 2 + i, y2, WHITE);
  }
}

void print_connect_screen()
{
  lcd.clearDisplay();
  lcd.setTextSize(1);
  lcd.setCursor(0, 0);
  lcd.print("TD5 RPM/Temp Monitor");
  lcd.setCursor(0, 24);
  lcd.print("Connecting to ECU...");
  lcd.display();
}

void print_rpm_temp_screen(int rpm, float coolant_temp)
{
  lcd.clearDisplay();

  // RPM display (large font)
  lcd.setTextSize(2);
  lcd.setCursor(0, 0);
  lcd.print("RPM:");

  // Right-align RPM value
  char rpmStr[6];
  sprintf(rpmStr, "%4d", rpm);
  lcd.setCursor(52, 0);
  lcd.print(rpmStr);

  // Temperature display (smaller, right side)
  lcd.setTextSize(1);
  lcd.setCursor(104, 0);
  lcd.print((int)coolant_temp);
  lcd.print("C");

  // Temperature label
  lcd.setCursor(104, 8);
  lcd.print("Temp");

  // Draw temperature graph
  drawTempGraph();

  lcd.display();
}
