/*
 * TD5_Dreh_Temp - RPM + Temperatur Anzeige
 *
 * Land Rover TD5 ECU Diagnosewerkzeug
 * Zeigt Drehzahl, Kühlmitteltemperatur und Temperaturverlaufskurve
 *
 * Original: td5opencomstm32 by Luca Veronesi & BennehBoy
 * ESP32 Port: 2024
 *
 * Hardware:
 * - ESP32 DevKit
 * - SSD1306 128x64 OLED (I2C)
 * - K-Line Interface
 *
 * Connections:
 * - GPIO16 (RX2) -> K-Line RX
 * - GPIO17 (TX2) -> K-Line TX
 * - GPIO21 (SDA) -> OLED SDA
 * - GPIO22 (SCL) -> OLED SCL
 *
 * This code is in the public domain.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "td5defs.h"
#include "td5comm.h"
#include "td5hmi.h"

#define MODE_STARTUP        0
#define MODE_CONNECTING     1
#define MODE_INSTRUMENT     2

// OLED Display - I2C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 lcd(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Global objects
Td5Comm td5;

// Global variables
static byte deviceMode = MODE_STARTUP;

void setup()
{
  // Initialize Serial for debug
  Serial.begin(115200);
  delay(100);

  #ifdef _DEBUG_
  Serial.println();
  Serial.println("================================");
  Serial.println("  TD5 RPM/Temp Monitor");
  Serial.println("================================");
  Serial.println();
  #endif

  // Initialize I2C for OLED
  Wire.begin(OLED_SDA, OLED_SCL);

  // Initialize OLED display
  if(!lcd.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed!");
    while(1);
  }

  lcd.clearDisplay();
  lcd.setTextColor(WHITE);
  lcd.setTextSize(1);
  lcd.display();

  // Initialize OBD communication
  td5.init();

  #ifdef _DEBUG_
  Serial.println("Initialization complete");
  #endif
}

void loop()
{
  switch(deviceMode)
  {
    case MODE_STARTUP:
      modeStartup();
      break;

    case MODE_CONNECTING:
      modeConnecting();
      break;

    case MODE_INSTRUMENT:
      modeInstrument();
      break;
  }

  // Keep ECU alive if connected
  if(td5.ecuIsConnected())
  {
    if(td5.getLastReceivedPidElapsedTime() > KEEP_ALIVE_TIME)
    {
      td5.getPid(&pidKeepAlive);
    }

    // Reconnect on too many lost frames
    if (td5.getConsecutiveLostFrames() > 3)
    {
      #ifdef _DEBUG_
      Serial.println("Connection lost - reconnecting...");
      #endif
      lcd.clearDisplay();
      lcd.setCursor(0, 24);
      lcd.print("Connection lost...");
      lcd.display();
      delay(1000);
      td5.disconnectFromEcu();
      deviceMode = MODE_CONNECTING;
    }
  }
}


///////////////  Mode Startup ///////////////
void modeStartup()
{
  // Show startup message briefly
  lcd.clearDisplay();
  lcd.setTextSize(1);
  lcd.setCursor(10, 20);
  lcd.print("TD5 RPM/Temp");
  lcd.setCursor(25, 35);
  lcd.print("Monitor");
  lcd.display();
  delay(1500);

  deviceMode = MODE_CONNECTING;
}

//////////////  Mode Connecting //////////////
void modeConnecting()
{
  print_connect_screen();

  if(td5.connectToEcu())
  {
    #ifdef _DEBUG_
    Serial.println("Connected to ECU");
    #endif
    lcd.setCursor(0, 40);
    lcd.print("Connected!");
    lcd.display();
    td5.getPid(&pidStartFuelling);
    delay(500);
    deviceMode = MODE_INSTRUMENT;
  }
  else
  {
    #ifdef _DEBUG_
    Serial.println("Connection failed - retrying...");
    #endif
    lcd.setCursor(0, 40);
    lcd.print("Failed - retrying...");
    lcd.display();
    delay(2000);
  }
}

//////////////  Mode Instrument //////////////
void modeInstrument()
{
  if(td5.ecuIsConnected())
  {
    td5.instrumentCycle();

    if(td5.newDataIsAvailable())
    {
      int rpm = pidRPM.getlValue();
      float coolantTemp = pidTemperatures.getfValue(0);

      // Update temperature history for graph
      updateTempHistory(coolantTemp);

      // Display RPM, temperature, and graph
      print_rpm_temp_screen(rpm, coolantTemp);
    }
  }
}
