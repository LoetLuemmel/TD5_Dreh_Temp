/*
Td5Comm.cpp - ESP32 Port (Simplified for RPM + Temperature display)

Original: td5opencomstm32 by Luca Veronesi & BennehBoy
ESP32 Port: 2024

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "td5comm.h"
#include "keygen.h"

// ESP32: Use Serial2 for K-Line (defined in td5defs.h)

const unsigned char pid_0x00[] = { 0x81, 0x13, 0xF7, 0x81, 0x0C };        // INIT_FRAME
const unsigned char pid_0x01[] = { 0x02, 0x10, 0xA0, 0x00 };              // START_DIAG
const unsigned char pid_0x02[] = { 0x02, 0x27, 0x01, 0x00 };              // REQ_SEED
const unsigned char pid_0x03[] = { 0x04, 0x27, 0x02, 0x00, 0x00, 0x00 };  // SEND_KEY
const unsigned char pid_0x04[] = { 0x02, 0x21, 0x20, 0x00 };              // START_FUELLING
const unsigned char pid_0x05[] = { 0x02, 0x21, 0x09, 0x00 };              // ENGINE_RPM
const unsigned char pid_0x06[] = { 0x02, 0x21, 0x1A, 0x00 };              // TEMPERATURES
const unsigned char pid_0x07[] = { 0x02, 0x3E, 0x01, 0x00 };              // KEEP_ALIVE

const byte *td5_pids[] =
{
  pid_0x00, pid_0x01, pid_0x02, pid_0x03, pid_0x04, pid_0x05, pid_0x06, pid_0x07
};

Td5Pid pidInitFrame(INIT_FRAME, 5, 5);
Td5Pid pidStartDiag(START_DIAG, 4, 3);
Td5Pid pidRequestSeed(REQ_SEED, 4, 6);
Td5Pid pidSendKey(SEND_KEY, 6, 4);
Td5Pid pidRPM(ENGINE_RPM, 4, 6, 250);
Td5Pid pidTemperatures(TEMPERATURES, 4, 20, 1000);
Td5Pid pidStartFuelling(START_FUELLING, 4, 8);
Td5Pid pidKeepAlive(KEEP_ALIVE, 4, 3, KEEP_ALIVE_TIME);

///////////////////////////////////////////////////
//              Generic functions                //
///////////////////////////////////////////////////
void debug_log_byte(byte b)
{
  if (b < 16)
  {
    Serial.print('0');
  }
  Serial.print(b, HEX);
  Serial.print(' ');
}

void debug_log_frame(byte *datasent, byte sentlen, byte *datarecv, byte recvlen)
{
  byte i;

  for(i=0; i<sentlen; i++)
    debug_log_byte(datasent[i]);

  Serial.print(' ');

  for(i=0; i<recvlen; i++)
    debug_log_byte(datarecv[i]);

  Serial.println();
}

void retrieve_keys_from_eeprom(uint8_t *seed, uint8_t *key)
{
  keyBytes_t myKey;

  myKey.low_byte = seed[1];
  myKey.high_byte = seed[0];

  keyGenerate(&myKey);

  key[1] = myKey.low_byte;
  key[0] = myKey.high_byte;
}

void keyGenerate(keyBytes_t * key) {

    uint16_t seed = key->keyword;
    uint16_t tmp = 0;
    uint8_t count = 0;
    uint8_t idx;
    uint8_t tap = 0;

    count = ((seed >> 0xC & 0x8) | (seed >> 0x5 & 0x4) | (seed >> 0x3 & 0x2) | (seed & 0x1)) + 1;

    for (idx = 0; idx < count; idx++) {

        tap = ((seed >> 1 ) ^ (seed >> 2 ) ^ (seed >> 8 ) ^ (seed >> 9 )) & 1;

        tmp = ((seed >> 1) | (tap << 0xF));

        if ((seed >> 0x3 & 1) && (seed >> 0xD & 1)) {
            seed = tmp & ~1;
        } else {
            seed = tmp | 1;
        }
    }
    key->keyword = seed;
}


///////////////////////////////////////////////////
//                 Class Td5Comm                 //
///////////////////////////////////////////////////
Td5Comm::Td5Comm()
{
  lastReceivedPidTime = 0;
  initStep = 0;
  lostFrames = 0;
  consLostFrames = 0;
  initTime = 0;
  ecuConnection = false;
  newDataAvailable = false;
}

void Td5Comm::init()
{
  // init pinouts
  pinMode(ledPin, OUTPUT);
  pinMode(K_OUT, OUTPUT);
  pinMode(K_IN, INPUT);
}

boolean Td5Comm::read_byte(byte * b)
{
  int readData;
  boolean success = true;
  byte t=0;

  while(t != READ_ATTEMPTS  && (readData=obdSerial.read())==-1)
  {
    delay(1);
    t++;
  }

  if (t >= READ_ATTEMPTS)
  {
    success = false;
  }

  if (success)
  {
    *b = (byte) readData;
  }

  return success;
}

void Td5Comm::write_byte(byte b)
{
  obdSerial.write(b);
  delay(Td5RequestByteDelay);
}

int8_t Td5Comm::getPid(Td5Pid* pid)
{
  boolean gotData = false;
  byte responseIndex = 0;
  byte dataCaught = '\0';
  byte dataLen = 0;

  if (pid->id != INIT_FRAME)
    dataLen = pid->requestFrame[0] + 2;
  else
    dataLen = 5;

  long currentTime = millis();

  if ((currentTime >= (lastReceivedPidTime + Td5RequestDelay)) && (currentTime >= (pid->lastSeenTime + pid->cycleTime)))
  {
    // Send the message
    pid->requestFrame[dataLen-1] = checksum(pid->requestFrame, dataLen-1);

    for (byte i = 0; i < dataLen; i++)
    {
      write_byte(pid->requestFrame[i]);
    }

    // Skip echo bytes (K-Line interface echoes all transmitted bytes back)
    for (byte i = 0; i < dataLen; i++)
    {
      if (!read_byte(&dataCaught))
      {
        #ifdef _DEBUG_
        Serial.println("Echo timeout");
        #endif
        break;
      }
    }

    // Now read the actual ECU response
    long waitResponseTime = millis() + 300;
    do
    {
      while(read_byte(&dataCaught) && (responseIndex < pid->responseLength))
      {
        gotData = true;
        pid->responseFrame[responseIndex] = dataCaught;
        responseIndex++;
      }
    }
    while (millis() <= waitResponseTime && !gotData);

    if (gotData && (responseIndex > 1))
    {
      lastReceivedPidTime = millis();
      pid->lastSeenTime = lastReceivedPidTime;
      consLostFrames = 0;
      #ifdef _DEBUG_
      debug_log_frame(pid->requestFrame, dataLen, pid->responseFrame, responseIndex);
      #endif
      if(checksum(pid->responseFrame, responseIndex-1) == pid->responseFrame[responseIndex-1])
      {
        if(pid->responseFrame[1] != 0x7F)
        {
          return responseIndex;
        }
        else
        {
          return PID_NEGATIVE_ANSWER;
        }
      }
    }
  }
  else
  {
    return PID_NOT_READY;
  }

  #ifdef _DEBUG_
  Serial.println("Lost frame detected");
  #endif
  lostFrames += 1;
  consLostFrames += 1;
  return PID_LOST_FRAME;
}

boolean Td5Comm::ecuIsConnected()
{
  return ecuConnection;
}

boolean Td5Comm::newDataIsAvailable()
{
  if(newDataAvailable)
  {
    newDataAvailable = false;
    return true;
  }
  else
    return false;
}

unsigned long Td5Comm::getLastReceivedPidElapsedTime()
{
  return (millis() - lastReceivedPidTime);
}

int Td5Comm::getConsecutiveLostFrames()
{
  return consLostFrames;
}

byte Td5Comm::checksum(byte *data, byte len)
{
  byte crc=0;
  for(byte i=0; i<len; i++)
    crc=crc+data[i];
  return crc;
}

void Td5Comm::initComm()
{
  unsigned long currentTime = millis();

  switch (initStep)
  {
  case 0:
    // KWP2000 Fast Init
    {
      ecuConnection = false;

      #ifdef _DEBUG_
      Serial.println("Init: Fast Init (KWP2000)");
      #endif

      obdSerial.end();
      pinMode(K_OUT, OUTPUT);

      // Idle HIGH for 300ms
      digitalWrite(K_OUT, HIGH);
      delay(300);

      // Fast init: 25ms LOW pulse
      digitalWrite(K_OUT, LOW);
      delay(25);

      // 25ms HIGH
      digitalWrite(K_OUT, HIGH);
      delay(25);

      #ifdef _DEBUG_
      Serial.println("Init: Fast init pulse complete");
      #endif

      // Start Serial at 10400 baud
      obdSerial.begin(10400, SERIAL_8N1, K_IN, K_OUT);
      delay(5);
      while(obdSerial.available()) obdSerial.read();

      lastReceivedPidTime = 0;
      pidInitFrame.lastSeenTime = 0;

      initTime = millis() + 50;
      initStep = 1;
    }
    break;

  case 1:
    // Send INIT_FRAME and handle echo
    if (millis() >= initTime)
    {
      #ifdef _DEBUG_
      Serial.println("Init: Sending INIT_FRAME");
      #endif

      // Manually send INIT_FRAME
      byte initFrame[] = {0x81, 0x13, 0xF7, 0x81, 0x0C};

      for (int i = 0; i < 5; i++) {
        obdSerial.write(initFrame[i]);
        delay(5);
      }

      // Wait and read all bytes (echo + response)
      delay(100);

      byte buffer[20];
      int count = 0;
      while (obdSerial.available() && count < 20) {
        buffer[count++] = obdSerial.read();
      }

      #ifdef _DEBUG_
      Serial.printf("Init: Received %d bytes: ", count);
      for (int i = 0; i < count; i++) {
        Serial.printf("%02X ", buffer[i]);
      }
      Serial.println();
      #endif

      if (count >= 5) {
        if (count > 5) {
          #ifdef _DEBUG_
          Serial.println("Init: Got response after echo!");
          #endif
          lastReceivedPidTime = millis();
          initTime = millis() + Td5RequestDelay;
          initStep = 17;  // Continue with START_DIAG
        } else {
          #ifdef _DEBUG_
          Serial.println("Init: Only echo received, trying START_DIAG");
          #endif
          lastReceivedPidTime = millis();
          initTime = millis() + Td5RequestDelay;
          initStep = 17;
        }
      } else {
        #ifdef _DEBUG_
        Serial.println("Init: No response, trying slow init");
        #endif
        initStep = 10;
      }
    }
    break;

  // Slow init fallback (5-baud) - cases 10-16
  case 10:
    {
      #ifdef _DEBUG_
      Serial.println("Init: Slow init (5-baud)");
      #endif

      obdSerial.end();
      pinMode(K_OUT, OUTPUT);

      // Disable task watchdog during slow init
      esp_task_wdt_delete(NULL);

      // Idle HIGH for 300ms
      digitalWrite(K_OUT, HIGH);
      delay(300);

      // 5-baud init: Address 0x10 = 0b00010000, sent LSB first
      const uint8_t bits[] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 1};  // Start, D0-D7, Stop

      for (int i = 0; i < 10; i++) {
        digitalWrite(K_OUT, bits[i] ? HIGH : LOW);
        delay(200);
      }

      // Re-enable task watchdog
      esp_task_wdt_add(NULL);

      #ifdef _DEBUG_
      Serial.println("Init: 5-baud sequence complete");
      #endif

      obdSerial.begin(10400, SERIAL_8N1, K_IN, K_OUT);
      delay(5);
      while(obdSerial.available()) obdSerial.read();

      initTime = millis() + 300;
      initStep = 12;
    }
    break;

  case 12:
    // Wait for Sync byte (0x55) from ECU
    {
      if (obdSerial.available())
      {
        byte syncByte = obdSerial.read();
        #ifdef _DEBUG_
        Serial.printf("Init: Received byte: 0x%02X\n", syncByte);
        #endif

        if (syncByte == 0x55)
        {
          #ifdef _DEBUG_
          Serial.println("Init: Sync OK!");
          #endif
          initTime = currentTime + 100;
          initStep++;
        }
      }
      else if (currentTime >= initTime)
      {
        #ifdef _DEBUG_
        Serial.println("Init: Timeout - no sync");
        #endif
        initStep = 0;
      }
    }
    break;

  case 13:
    // Wait for Key Byte 1
    {
      if (obdSerial.available())
      {
        byte keyByte1 = obdSerial.read();
        #ifdef _DEBUG_
        Serial.printf("Init: Received key1: 0x%02X\n", keyByte1);
        #endif
        initTime = currentTime + 100;
        initStep++;
      }
      else if (currentTime >= initTime)
      {
        #ifdef _DEBUG_
        Serial.println("Init: Timeout waiting for key byte 1");
        #endif
        initStep = 0;
      }
    }
    break;

  case 14:
    // Wait for Key Byte 2, then send inverted
    {
      if (obdSerial.available())
      {
        byte keyByte2 = obdSerial.read();
        #ifdef _DEBUG_
        Serial.printf("Init: Received key2: 0x%02X\n", keyByte2);
        #endif

        delay(30);

        byte invertedKey2 = ~keyByte2;
        obdSerial.write(invertedKey2);
        #ifdef _DEBUG_
        Serial.printf("Init: Sent inverted key2: 0x%02X\n", invertedKey2);
        #endif

        initTime = currentTime + 100;
        initStep++;
      }
      else if (currentTime >= initTime)
      {
        #ifdef _DEBUG_
        Serial.println("Init: Timeout waiting for key byte 2");
        #endif
        initStep = 0;
      }
    }
    break;

  case 15:
    // Wait for inverted address from ECU
    {
      if (obdSerial.available())
      {
        byte invertedAddr = obdSerial.read();
        #ifdef _DEBUG_
        Serial.printf("Init: Received inverted addr: 0x%02X\n", invertedAddr);
        #endif

        if (invertedAddr == 0xEF)
        {
          #ifdef _DEBUG_
          Serial.println("Init: Slow init handshake complete!");
          #endif
          lastReceivedPidTime = currentTime;
          initTime = currentTime + Td5RequestDelay;
          initStep++;
        }
        else
        {
          initStep = 0;
        }
      }
      else if (currentTime >= initTime)
      {
        #ifdef _DEBUG_
        Serial.println("Init: Timeout waiting for inverted address");
        #endif
        initStep = 0;
      }
    }
    break;

  case 16:
    // Send INIT_FRAME for diagnostic session
    if (currentTime >= initTime)
    {
      if (getPid(&pidInitFrame) <= 0)
      {
        #ifdef _DEBUG_
        Serial.println("Init: No response to INIT_FRAME");
        #endif
        initStep = 0;
        break;
      }

      #ifdef _DEBUG_
      Serial.println("Init: Got INIT_FRAME response");
      #endif
      lastReceivedPidTime = currentTime;
      initTime = currentTime + Td5RequestDelay;
      initStep++;
    }
    break;

  case 17:
    if (currentTime >= initTime)
    {
      if (getPid(&pidStartDiag) <= 0)
      {
        initStep = 0;
        break;
      }

      lastReceivedPidTime = currentTime;
      initTime = currentTime + Td5RequestDelay;
      initStep++;
    }
    break;

  case 18:
    if (currentTime >= initTime)
    {
      if (getPid(&pidRequestSeed) <= 0)
      {
        initStep = 0;
        break;
      }

      lastReceivedPidTime = currentTime;
      initTime = currentTime + Td5RequestDelay;
      initStep++;
    }
    break;

  case 19:
    if (currentTime >= initTime)
    {
      uint8_t seed[2], key[2];
      seed[0] = pidRequestSeed.getResponseByte(3);
      seed[1] = pidRequestSeed.getResponseByte(4);

      if (seed[0] == 0x00 && seed[1] == 0x00)
      {
        #ifdef _DEBUG_
        Serial.println("ECU already unlocked (seed=0)");
        #endif
      }
      else
      {
        retrieve_keys_from_eeprom(seed, key);
        pidSendKey.setRequestByte(key[0], 3);
        pidSendKey.setRequestByte(key[1], 4);

        if (getPid(&pidSendKey) <= 0)
        {
          initStep = 0;
          break;
        }
      }

      lastReceivedPidTime = currentTime;
      initTime = currentTime + Td5RequestDelay;

      delay(55);
      ecuConnection = true;
      digitalWrite(ledPin, HIGH);
      initStep = 0;
      #ifdef _DEBUG_
      Serial.println("Init: ECU Connected!");
      #endif
    }
    break;
  }
}

boolean Td5Comm::connectToEcu()
{
  #ifdef _DEBUG_
  Serial.println("Connecting to ECU...");
  #endif

  initStep = 0;
  int attempts = 0;
  const int maxAttempts = 50;

  do
  {
    initComm();
    delay(100);
    attempts++;

    #ifdef _DEBUG_
    if (initStep > 0) {
      Serial.printf("Init step: %d\n", initStep);
    }
    #endif

    if (attempts >= maxAttempts)
    {
      #ifdef _DEBUG_
      Serial.println("Connection timeout");
      #endif
      return false;
    }
  }
  while (initStep != 0);

  initTime = 0;

  #ifdef _DEBUG_
  if (ecuConnection) {
    Serial.println("ECU Connected!");
  }
  #endif

  return ecuConnection;
}

void Td5Comm::disconnectFromEcu()
{
  obdSerial.end();
  lostFrames = 0;
  digitalWrite(ledPin, LOW);
  ecuConnection = false;
}

void Td5Comm::instrumentCycle()
{
  if(getPid(&pidRPM) > 0)
  {
    newDataAvailable = true;
    return;
  }

  if(getPid(&pidTemperatures) > 0)
  {
    newDataAvailable = true;
    return;
  }
}


///////////////////////////////////////////////////
//                 Class Td5Pid                  //
///////////////////////////////////////////////////
Td5Pid::Td5Pid(byte ID, byte reqlen, byte resplen, long cycletime)
{
  id = ID;
  cycleTime = cycletime;
  lastSeenTime = 0;

  responseLength = resplen;

  requestFrame= (byte *)malloc(sizeof(byte) * reqlen);
  memcpy(requestFrame, td5_pids[id],reqlen);
  responseFrame = (byte *)malloc(sizeof(byte) * resplen);
}

float Td5Pid::getfValue(byte index)
{
  uint16_t value;

  switch(id)
  {
    case TEMPERATURES:
      value = (uint16_t)(((responseFrame[3 + (index * 4)] * 256L) + responseFrame[4 + (index * 4)]) - 2732L);
      return ((float) value / 10.0);

    default:
      return 0.0;
  }
}

int16_t Td5Pid::getlValue(byte index)
{
  switch(id)
  {
    case ENGINE_RPM:
      return(int16_t)((responseFrame[3] * 256L) + responseFrame[4]);

    default:
      return 0L;
  }
}

void Td5Pid::setRequestByte(byte value, byte pos)
{
  requestFrame[pos] = value;
}

byte Td5Pid::getResponseByte(byte pos)
{
  return responseFrame[pos];
}
