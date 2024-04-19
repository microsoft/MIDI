# AM MIDI 2.0 Lib 
A MIDI 2.0 Library

This is a general purposes Library for building MIDI 2.0 Devices and Applications. This library aims to work on 
everything from Arduino's through to large scale applications. It provides the building blocks, processing and 
translations needed for most MIDI 2.0 Devices and Applications

## IMPORTANT! Please read first
I hope that this library is useful to everyone making MIDI 2.0 Devices.
If you do use this library please let me know! I am keen to see all the MIDI 2.0 Projects. This code is also available 
for DIY and commercial use (MIT Licence). Please note that use of this library is at your own risk.

THIS LIBRARY IS CURRENTLY UNDER DEVELOPMENT - 
The code is still being adjusted as it is being prototyped and changes do occur, however the WIKI attempts to reflect 
the latest code.

If you see code here that :
* could be styled/structured better
* could be written better
* could use less resources
* has memory leaks, bugs, 
* is fundamentally flawed
* has spelling mistakes and grammatical errors

then please submit PR's and/or issues - but PR's preferred. 

## What does this do?
Please read the MIDI 2.0 specification on https://midi.org/specifications to understand the following.

This library can:
* Convert MIDI 1.0 Byte stream to UMP and back
* Process and send UMP Streams
* Process and Send MIDI-CI Messages
* Build UMP 32 bit Words to send out

This library is designed to use a small footprint. It does this by processing each UMP packet (or MIDI 1.0 Byte stream) 
one at a time. This way large data is handled in small chunks to keep memory small.

This set of files allows you to pick and choose which parts of MIDI 2.0 you wish to include. For example if you are 
only working with MIDI 1.0 Byte streams and just need a MIDI-CI handler? Then great just pull in that header :)

Note it is upto the application to:
 * Store Remote MIDI-CI Device details
 * Upon receiving MIDI-CI Message to interpret the Messages data structure (e.g. Profile Id bytes, Note On Articulation 
etc.)
 * Handle logic and NAK sending and receiving.

This means the overheads for a simple MIDI 2.0 device is down to a compiled size of around 10k (possibly less?), with a 
memory footprint of around 1k.

## Documentation
Can be found on the [docs folder](https://github.com/midi2-dev/AM_MIDI2.0Lib/tree/main/docs)

### Example: Translate MIDI 1.0 Byte stream to UMP

Here is a quick example

```C++
#include "bytestreamUMP.h"

bytestreamToUMP BS2UMP;

void setup()
{
  Serial.begin(31250);
  
  //Produce MIDI 2.0 Channel Voice Message (Message Type 0x4)
  //Default (false) will return MIDI 1.0 Channel Voice Messages (Message Type 0x2)
  BS2UMP.outputMIDI2 = true; 
  
  //Set the UMP group of the output UMP message. By default this is set to Group 1
  //defaultGroup value is 0 based
  BS2UMP.defaultGroup = 0; //Group 1
}

void loop()
{
  uint8_t inByte = 0;
  // if there is a serial MIDI byte:
  if (Serial.available() > 0) {
    // get incoming byte:
    inByte = Serial.read();
    if(inByte == 0xFE) return; //Skip ActiveSense 
    
    BS2UMP.midi1BytestreamParse(inByte);
    while(BS2UMP.availableUMP()){
      uint32_t ump = BS2UMP.readUMP();
      //ump contains a ump 32 bit value. UMP messages that have 64bit will produce 2 UMP words
    }
  }
}
```

### Example: Process UMP Streams
UMP Streams accepts a series of 32 bit values. UMP messages that have 64bit will provide 2 UMP words.

```C++

#include "umpProcessor.h"
umpProcessor UMPProcess; 

void noteOff(uint8_t group,  uint8_t mt, uint8_t channel, uint8_t noteNumber, unsigned int velocity, int attributeType, unsigned int attributeData){
//Process incoming MIDI note Off event.
}

void noteOn(uint8_t group,  uint8_t mt, uint8_t channel, uint8_t noteNumber, unsigned int velocity, int attributeType, unsigned int attributeData){ 
}


void cc(uint8_t group,  uint8_t mt, uint8_t channel, uint8_t index, uint32_t value){  
}

void rpn(uint8_t group, uint8_t channel, uint8_t bank,  uint8_t index, uint32_t value){  
}

void setup()
{
    UMPProcess.setNoteOff(noteOff);
    UMPProcess.setNoteOn(noteOn);
    UMPProcess.setControlChange(cc);
    UMPProcess.setRPN(rpn);
}

void loop()
{
...
  while(uint32_t ump = readSomeUMP()){
      UMPProcess.processUMP(ump);
  }
...  
}

```

### Example: Process MIDI-CI Messages
MIDI-CI requires a lot of SysEx messages. This library abstracts the complexity of building and parsing most MIDI-CI Messages.
```C++

#include "midiCIProcessor.h"
midi2Processor midiCIProcess; 
uint32_t localMUID;
uint8_t sysexId[3] = {0x00 , 0x02, 0x22};
uint8_t famId[2] = {0x7F, 0x00};
uint8_t modelId[2] = {0x7F, 0x00};
uint8_t ver[4];
unint8_t sysexBuffer[512];

bool checkMUID(uint8_t group, uint32_t muid){
	return (localMUID==muid);  
}

void recvDiscovery(uint8_t group, struct MIDICI ciDetails, uint8_t* remotemanuId, uint8_t* remotefamId, uint8_t* remotemodelId, uint8_t *remoteverId, uint8_t remoteciSupport, uint16_t remotemaxSysex){
	Serial.print("->Discovery: remoteMuid ");Serial.println(ciDetails.remoteMUID);
    uint16_t sBuffLen = sendDiscoveryReply(sysexBuffer, localMUID, ciDetails.remoteMUID, sysexId, famId, modelId, ver, 0b11100, 512);
    sendSysExOutOfDevice(sysexBuffer, sBuffLen);
}

void setup()
{
  localMUID = random(0xFFFFEFF);
  
  midiCIProcess.setRecvDiscovery(recvDiscovery);
  midiCIProcess.setCheckMUID(checkMUID);
  
  uint16_t sBuffLen = sendDiscoveryRequest(sysexBuffer,1, sysexId, famId, modelId, ver,12, 512);
  sendSysExOutOfDevice(sysexBuffer, sBuffLen);
}

void loop()
{
...
  while(uint8_t sysexByte = getNextSysexByte()){
    midiCIProcess.processUMP(sysexByte);
  }
...  
}

```

---



