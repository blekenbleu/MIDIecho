#include <Arduino.h>
#include "USBAPI.h"
#include "PluggableUSB.h"
#include "MIDIUSB.h"
#include "USBCDC.h"

//USBCDC USBSerial;
HardwareSerial USBSerial(PA3, PA2);   // RX2, TX2 Black Pill
bool toggle = true, recent = false, received = false, ok = true, echo = true;
unsigned long now = 0, then = 0, wait;
long later = 0;
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

  for (wait = 0; !USBSerial; wait++)
  {
    // wait for Serial port connection
    delay(500);    // syncopated blink
    LEDb4();
    delay(250);
    LEDb4();
    delay(750);
    LEDb4();
  }

  USBSerial.print("\r\nMIDI.ino connect;  wait count "); USBSerial.print(wait);
  USBSerial.println(echo ? ";  echo on" : ";  no echo");
  wait = 0;
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
void do_echo()
{
  if (0 < i && millis() > then + later)
  {
    i = 0;
//  USBSerial.println("do_echo");
    rx.byte1 = 0xB1;
    rx.byte3 = 51;
    MidiUSB.sendMIDI(rx);
//  controlChange(17);
//  controlChange(34);	// sometimes breaks
//  controlChange(68);  // often breaks
//  controlChange(85);	// always breaks
//  controlChange(102);
//  controlChange(119);
    MidiUSB.flush();		// seemingly no impact..?
  }
}

void loop()
{
//if (MidiUSB.available())		// seemingly never true..?
  {
//  USBSerial.println("MidiUSB.available");
    for (rx = MidiUSB.read(); 0 != rx.header; rx = MidiUSB.read())
    {
      then = millis();
      recent = received = true;
      i = 1;
      if (ok && echo)
      {
        sprintf(hex, "%02X%02X%02X%02X\r\n", rx.header, rx.byte1,
                rx.byte2, rx.byte3);
        size_t l = strlen(hex);
        if (l < USBSerial.availableForWrite())	// MIDI faster than 9600
          ok = (l == USBSerial.write(hex, l));
//      else ok = false;
      }
    }
  }

  if (echo)
    do_echo();
  if (recent)
    wait = 100;
  else if (received)
    wait = 300;
  else wait = toggle ? 100 : 1900;
  if (millis() > now + wait)
    LEDb4();

  if (recent && (millis() > (then + 4000)))
  {
    if (received)
    {
      value &= 127;
      USBSerial.print("recent timeout; sending "); USBSerial.println(value);
      rx.byte1 = 0xB2;				// not seen
      controlChange(value);
      recent = false;
    }
    then = millis();
  }

  if (0 < USBSerial.available())  // keypad echo control
  {
    value = USBSerial.read();
    USBSerial.print(value);
    if (32 == value) // space
    {
      echo = !echo;
      USBSerial.println(echo ? "; echo on" : "; no echo");
    } else {					// NUM lock
      bool changed = true;
      if (43 == value)	// plus
        later += 5;
      else if (126 == value) // dot
        later -= 1;
      else if (45 == value) // minus
        later -= 4;
      else if (126 == value) // one
        later += 1;
      else if (48 == value) // zero
        later = 0;
       else if (0 > later)
         later = 0;
       else changed = false;
       if (changed)
         USBSerial.print("; later = "); USBSerial.println(later);
     }
     recent = true;
//  MidiUSB.flush();
    then = millis();
  }
}
