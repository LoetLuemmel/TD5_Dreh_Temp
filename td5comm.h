/*
Td5Comm.h - ESP32 Port (Simplified for RPM + Temperature display)

Original: td5opencomstm32 by Luca Veronesi & BennehBoy
ESP32 Port: 2024

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
*/

#ifndef Td5Comm_h
#define Td5Comm_h

#include <Arduino.h>
#include <Wire.h>
#include "td5defs.h"

#define Td5RequestByteDelay       5
#define Td5RequestDelay           55
#define Td5KeepAliveDelay         4000L

#define READ_ATTEMPTS             7
#define KEEP_ALIVE_TIME           4500

#define INIT_FRAME      0x00
#define START_DIAG      0x01
#define REQ_SEED        0x02
#define SEND_KEY        0x03
#define START_FUELLING  0x04
#define ENGINE_RPM      0x05
#define TEMPERATURES    0x06
#define KEEP_ALIVE      0x07

class Td5Pid;

// Class Td5Comm
class Td5Comm
{
public:
  Td5Comm();
  void init();
  void initComm();
  int8_t getPid(Td5Pid* pid);
  boolean ecuIsConnected();
  boolean newDataIsAvailable();
  unsigned long getLastReceivedPidElapsedTime();
  int getConsecutiveLostFrames();

  boolean connectToEcu();
  void disconnectFromEcu();
  void instrumentCycle();

private:
  byte checksum(byte *data, byte len);
  boolean read_byte(byte * b);
  void write_byte(byte b);

protected:
  unsigned long lastReceivedPidTime;
  unsigned long initTime;
  byte initStep;
  boolean ecuConnection;
  boolean newDataAvailable;
  int lostFrames;
  int8_t consLostFrames;
};

// Class Td5Pid
class Td5Pid
{
public:
  Td5Pid(byte ID, byte reqlen, byte resplen, long cycletime = 0);
  float getfValue(byte index = 0);
  int16_t getlValue(byte index = 0);

  void setRequestByte(byte value, byte pos);
  byte getResponseByte(byte pos);

  byte *requestFrame;
  byte *responseFrame;

  byte id;
  long cycleTime;
  long lastSeenTime;
  byte responseLength;
};

// Key generation
void keyGenerate(keyBytes_t * key);
void retrieve_keys_from_eeprom(uint8_t *seed, uint8_t *key);

// Declare pids
extern Td5Pid pidInitFrame;
extern Td5Pid pidStartDiag;
extern Td5Pid pidRequestSeed;
extern Td5Pid pidSendKey;
extern Td5Pid pidRPM;
extern Td5Pid pidTemperatures;
extern Td5Pid pidStartFuelling;
extern Td5Pid pidKeepAlive;

#define PID_NOT_READY        0
#define PID_LOST_FRAME      -1
#define PID_NEGATIVE_ANSWER -2

#endif
