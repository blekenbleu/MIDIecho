#include <Arduino.h>
#include "USBAPI.h"
#include "PluggableUSB.h"
#include "USBCDC.h"
#include "MIDIUSB.h"

#define TRYCDC 0
#if TRYCDC
USBCDC USBserial;
HardwareSerial HWserial(PA3, PA2);
#define HWb(a) HWserial.begin(a)
#define HWp(a) HWserial.println(a)
#else
HardwareSerial USBserial(PA3, PA2);   // RX2, TX2 Black Pill
#define HWb
#define HWp
#endif

bool toggle = true, recent = false, received = false, ok = true, echo = true, do_flush = true;
bool first = true, sent = false;

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
  HWb(19200);
  HWp("MIDI.ino: HWserial.begun(19200)");
  // HWp(MidiUSB ? "MidiUSB true" : "MidiUSB false");	// operator bool(); declared, not implemented
  uint32_t Mavail = MidiUSB.available();		// from USBLibrarySTM32/examples/MIDI/src
  char MavailS[100];
  sprintf(MavailS, "USB_EP_SIZE %u", USB_EP_SIZE);
  HWp(MavailS);
  sprintf(MavailS, "MidiUSB.available returned %u", Mavail);
  HWp(MavailS);
  USBserial.begin(19200);
  USB_Begin();
  LEDb4();
  while (!USB_Running())
  {
    // wait until usb connected
    delay(50);
    LEDb4();
  }
  HWp("MIDI.ino: USB_Running");
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

  HWp("MIDI.ino: USBCDC connection");
  
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
void controlChange(uint8_t b1, int value) {
  midiEventPacket_t cc = { 0x0B, b1, rx.byte2, (byte)(127 & value) };
  USBserial.print("controlChange: "); slog(cc);  
  MidiUSB.sendMIDI(cc);
  sent = true;
} 

uint8_t i = 0;
void do_echo(uint8_t b)
{
//USBserial.println("do_echo");
  for (uint8_t c = 1; c <= count; c++)
    controlChange(b, 17 * c);
  if (do_flush)
    MidiUSB.flush();
}

void slog(midiEventPacket_t m)
{
  if (!ok)
    return;

  sprintf(hex, "%02X%02X%02X%02X\r\n", m.header, m.byte1, m.byte2, m.byte3);
  size_t l = strlen(hex);
  if (l < USBserial.availableForWrite())	// MIDI faster than 19200
    ok = (l == USBserial.write(hex, l));
}

midiEventPacket_t sx = {0, 0, 0, 0};
void loop()
{
  midiEventPacket_t rx;
  do
  {
    rx = MidiUSB.read();

    if (0 != rx.header)	// something to echo
    {
      sx = rx;
	  if (first)
        first = false;
      then = millis();
      recent = received = true;
      i++;
      if (echo)
      {
		slog(rx);
        MidiUSB.sendMIDI(rx);
      }

      if (7 < i)
      {
        i = 0;
        do_echo(0xB1);  
      } else if (echo && 4 == i) {
        if (0 < later)
          delay(later);
        do_echo(0xB2);
      }
    }

    if (recent)
      wait = 100;
    else if (received)
      wait = 300;
    else wait = toggle ? 100 : 1900;
    if (millis() > now + wait)
      LEDb4();

    if (recent && (millis() > (then + 500)) && received && 0 != sx.header)
    {
      USBserial.print("500 msec timeout; later = "); USBserial.print(later);
      USBserial.print(";  count = "); USBserial.println(count);
      sx.byte1 = 0xB3;
      MidiUSB.sendMIDI(sx);
      sent = true;
      USBserial.print("sent "); slog(sx);
      recent = false;
      then = millis();
    } else if (millis() > (then + 10000)) {
      USBserial.print("10 second timeout ");
      value = (127 & (1 + value));
      controlChange(0xB4, value);		// channel 5
      then = millis();
    }
    if (do_flush && sent)
    {
      sent = false;
      MidiUSB.flush();					// seemingly no impact..?
    }
  } while (rx.header != 0);

  if (0 < USBserial.available())  // keypad echo control
  {
    ok = true;
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
  }						// USBserial.available
}
