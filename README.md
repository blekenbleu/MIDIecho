# [MIDI echo example](MIDIecho.ino)
 &emsp; *for testing* [USBLibrarySTM32](https://blekenbleu.github.io/static/USBLibrarySTM32/)

echo MIDI in to out
*handling* `MIDI.read()` *in a* `loop()`  
Faster LED flash rate for 4 seconds after activity.  

### Cannot simultaneously write from MIDI-OX and read from MidiView
Cannot write to `BLACKPILL_F411CE HID in FS Mode` from e.g. [MIDI-OX](http://www.midiox.com/)  
when connected to read from `BLACKPILL_F411CE HID in FS Mode` with e.g. [MidiView](https://hautetechnique.com/midi/midiview/);  
MIDI-OX pops up:  
![](BlackPillMIDImemory.png)  

Instead, test reading and writing from [MIDI-OX](http://www.midiox.com/):  
![](MIDI-OXdevices.png)  
- routes nanoKONTROL2 CC2 to both Black Pill and [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html)  
  - *loopMIDI feeds [MidiView](https://hautetechnique.com/midi/midiview/)*

USBDeview shows `COM3` "Drive Letter" while only MIDI is configured...
![](USBDeview.png)  
[USB Device Tree View report](UDBdevTreeView.txt) 

**`MidiUSB.sendMIDI()` works *only* during `MidiUSB.read()` with non-0 `rx.header`**

Must reopen devices in MIDI-OX after resetting/reloading Black Pill.

## Composite MIDI + CDC fails
Just instancing `USBSerial.begin(115200);` is enough to kill all Black Pill USB...  
Windows 11 is unhappy enough that DFU subsequently fails;  must use ST-Link.

### MIDI + HardwareSerial
`HardwareSerial USBSerial(PA3, PA2);` instead of `USBCDC USBSerial;`
- `MidiUSB.available()` appears to never return non-zero.
- MIDI-OX sees CCs echoed (by MIDIecho) when MIDIecho `MidiUSB.read()`s them...!
   - is MIDI thru configured?
- only `sendMIDI` packets (CCs) <b>during</b> `MidiUSB.read()` are seen by MIDI-OX
- `MidiUSB.flush()` seemingly no effect..?

### MIDIecho serial terminal control
`MIDIecho.ino` *always* writes a Channel 4 CC 100 milliseconds after most recent `MidiUSB.read()`;  
`do_echo()` writes `count` Channel 2 CCs immediately after every 8th `MidiUSB.read()`,  
 &emsp; then another `count` Channel 3 CCs after `delay(later)`  
By pausing processing, `do_echo()` *with long enough* 'delay(later)` enables Channel 3 CC..   

 &emsp; (NUM pad) Keystroke handling:
- [space] toggles echo
- [2] toggles `MidiUSB.flush()`  
- [+] increases later 10 msec
- [-] decreases later 8 msec
- [0] sets later = 0
- [.] decrements count
- [1] increments count

Slider move echo for count=3 and delay(later=52) *eventually* dies.
