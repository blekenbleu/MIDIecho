#include <Arduino.h>
#include "USBAPI.h"
#include "PluggableUSB.h"
#include "MIDIUSB.h"
#include "USBCDC.h"

//USBCDC USBserial;
HardwareSerial USBserial(PA3, PA2);   // RX2, TX2 Black Pill

bool toggle = true, recent = false, received = false, ok = true, echo = true, do_flush = true;
unsigned long now = 0, then = 0, wait;
long later = 0;
uint8_t count = 1;
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
//HWserial.begin(19200);
//HWserial.println("MIDI.ino: HWserial begun");
  USBserial.begin(19200);
  USB_Begin();
  LEDb4();
  while (!USB_Running())
  {
    // wait until usb connected
    delay(50);
    LEDb4();
  }
//HWserial.println("MIDI.ino: USB_Running");
  for (wait = 0; !USBserial; wait++)
  {
    // wait for Serial port connection
    delay(500);    // syncopated blink
    LEDb4();
    delay(250);
    LEDb4();
    delay(750);
    LEDb4();
  }

  USBserial.print("\r\nMIDI.ino connect;  count = ");  USBserial.print(count);
  USBserial.print(";  delay = ");  USBserial.print(later);
  if (do_flush)
    USBserial.print(";  do_flush");
  USBserial.println(echo ? ";  echo on" : ";  no echo");
  wait = 0;
}

// Parameter 0 (header) is the event type (0x0B = control change).
// Parameter 1 (byte1) is the event type, combined with the channel.
// Parameter 2 (byte2) is the control number number (0-119).
// Parameter 3 (byte3) is the control value (0-127).
void controlChange(int value) {
  rx.byte3 = (byte)(127 & value);
  MidiUSB.sendMIDI(rx);
//MidiUSB.flush();
} 

uint8_t i = 0;
void do_echo(uint8_t b)
{
//USBserial.println("do_echo");
  rx.byte1 = b;
  for (uint8_t c = 1; c <= count; c++)
    controlChange(17 * c);
  if (do_flush)
    MidiUSB.flush();		// seemingly no impact..?
}

void loop()
{
//if (MidiUSB.available())		// seemingly never true..?
  {
//  USBserial.println("MidiUSB.available");
    rx = MidiUSB.read();
    if (0 != rx.header)
    {
      then = millis();
      recent = received = true;
      i++;
      if (ok && echo)
      {
        sprintf(hex, "%02X%02X%02X%02X\r\n", rx.header, rx.byte1,
                rx.byte2, rx.byte3);
        size_t l = strlen(hex);
        if (l < USBserial.availableForWrite())	// MIDI faster than 19200
          ok = (l == USBserial.write(hex, l));
      }
    }

    if (7 < i)
    {
      i = 0;
      do_echo(0xB1);  
    }
  }

  if (echo && 4 == i)
  {
    if (0 < later)
      delay(later);
    do_echo(0xB2);
  }

  if (recent)
    wait = 100;
  else if (received)
    wait = 300;
  else wait = toggle ? 100 : 1900;
  if (millis() > now + wait)
    LEDb4();

  if (recent && (millis() > (then + 50)))
  {
    if (received)
    {
      USBserial.print("50 msec recent timeout; later = "); USBserial.print(later);
      USBserial.print(";  count = "); USBserial.println(count);
      rx.byte1 = 0xB3;				// channel 4 not seen until 50 <= later
      MidiUSB.sendMIDI(rx);
      recent = false;
    }
    then = millis();
  } else if (millis() > (then + 10000)) {
    USBserial.println("10 second timeout; sending channel 5");
    rx.byte1 = 0xB4;              // channel 5 rarely seen (coinciding with input)
    controlChange(value);
    then = millis();
  }

  if (0 < USBserial.available())  // keypad echo control
  {
    USBserial.print(value = USBserial.read());
    if (32 == value) // space
    {
      echo = !echo;
      USBserial.println(echo ? "; echo on" : "; no echo");
    } else if (50 == value)	{ // two
      do_flush = !do_flush;
      USBserial.println(do_flush ? "; do_flush" : "; don't flush");
    } else {				// NUM lock
      bool changed = true;
      if (43 == value)		// plus
        later += 10;
      else if (45 == value) // minus
        later -= 8;
      else if (48 == value) // zero
        later = 0;
      else changed = false;
      if (0 > later)
         later = 0;
      if (changed)
         USBserial.print("; delay = "); USBserial.println(later);

      changed = true;
      if (49 == value) 		// one
        count += 1;
      else if (46 == value)	// dot
        count -=1 ;
      else changed = false;
      if (0 > count)
        count = 0;
      if (changed)
        USBserial.print("; count = "); USBserial.println(count);
    }
    recent = true;
    then = millis();
  }
}
