#include <Arduino.h>
#include "USBAPI.h"
#include "PluggableUSB.h"
#include "MIDIUSB.h"
#include "USBCDC.h"

//USBCDC USBSerial;
HardwareSerial USBSerial(PA3, PA2);   // RX2, TX2 Black Pill
bool toggle = true, active = false, received = false, ok = true;
unsigned long now = 0, then = 0, wait;
char hex[20];
uint8_t value = 50;
midiEventPacket_t rx; // https://docs.arduino.cc/libraries/midiusb/

void LEDb4()
{
  digitalWrite(PC13, toggle ? HIGH : LOW);
  toggle = !toggle;
  now = millis();
} 

void setup()
{
  pinMode(PC13, OUTPUT);    // stm32f411 LED
  MidiUSB.available();
  USBSerial.begin(9600);
  USB_Begin();
  LEDb4();
  while (!USB_Running())
  {
    // wait until usb connected
    delay(50);
    LEDb4();
  }
/*
  while (!USBSerial)
  {
    // wait for Serial port connection
    delay(500);    // syncopated blink
    LEDb4();
    delay(250);
    LEDb4();
    delay(750);
    LEDb4();
  }
 */
  USBSerial.println("\r\nMIDI.ino connect");
}

// Parameter 0 (header) is the event type (0x0B = control change).
// Parameter 1 (byte1) is the event type, combined with the channel.
// Parameter 2 (byte2) is the control number number (0-119).
// Parameter 3 (byte3) is the control value (0-127).
void controlChange(byte value) {
  rx.byte3 = value;
  MidiUSB.sendMIDI(rx);
//MidiUSB.flush();
} 

uint8_t i = 0;
void loop()
{
//if (MidiUSB.available())
  {
//  MidiUSB.flush();
    for (rx = MidiUSB.read(); 0 != rx.header; rx = MidiUSB.read())
    {
      then = millis();
      active = received = true;
      if (7 < ++i)
      {
        i = 0;
        rx.byte3 = 51;
        MidiUSB.sendMIDI(rx);
        controlChange(17);
      }
      if (!ok)
          continue;
      sprintf(hex, "%02X%02X%02X%02X\r\n", rx.header, rx.byte1,
              rx.byte2, rx.byte3);
      size_t m = 0, l = strlen(hex);
      if (l < USBSerial.availableForWrite())
      {
        m = USBSerial.write(hex, l);
        ok = (l == m);
      } else ok = false;
    }
  }

  if (active)
    wait = 100;
  else if (received)
    wait = 300;
  else wait = toggle ? 100 : 1900;
  if (millis() > now + wait)
    LEDb4();

  if (active && (millis() > (then + 4000)))
  {
    if (received)
    {
      USBSerial.println("active timeout");
      active = false;
      controlChange(value++);
      value &= 127;
    }
    then = millis();
  }

  if (0 < USBSerial.available())
  {
    value = USBSerial.read();
    USBSerial.print(value);
    controlChange(value++);
    value &= 127;
    active = true;
//  MidiUSB.flush();
    then = millis();
  }
}
