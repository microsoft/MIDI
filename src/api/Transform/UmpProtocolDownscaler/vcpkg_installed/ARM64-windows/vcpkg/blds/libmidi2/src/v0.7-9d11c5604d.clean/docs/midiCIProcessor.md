# MIDI-CI

```midiCIProcessor``` handles MIDI-CI messages and tries to reduce complexity in processing messages. It does 
not attempt to solve the logic and handling of that data. It is up to the application to send responses to queries 
from an Initiator.

```midiCIProcessor``` is a forward and backward compatible with different MIDI-CI versions. On incoming MIDI-CI 
messages it will ignore bytes at the end of the message it does not understand.

```c++
midiCIProcessor MIDICIHandler;
MIDICIHandler.setCheckMUID(checkMUID);
MIDICIHandler.setRecvDiscovery(discoveryHandler);
```

## Process Handling Commands
The ```midiCIProcessor``` takes in 1 byte at a time when processing MIDI-CI. This is done so longer complex SysEx does 
not require lots of memory and processing can occur as the  UMP Sysex is delivered.

```c++ 
midiCIProcessor midiciMain;

void processSysex(uint8_t umpGroup, uint8_t *sysex ,uint16_t lengthOfSysex){
  //note sysex argument does not include 0xF0 and 0xF7
  if(sysex[0] == S7UNIVERSAL_NRT && sysex[2] == S7MIDICI){
      if(umpGroup==0) {
          midiciMain.startSysex7(umpGroup, sysex[1]);
          for (int i = 0; i < lengthOfSysex; i++) {
                midiciMain.processMIDICI(sysex[i]);
          }
          midiciMain.endSysex7();
      }
  }
}
```

#### void startSysex7(uint8_t umpGroup, uint8_t deviceId)
```umpGroup``` is the UMP Group this SysEx was received on. It is passed onto the callbacks as part of the MIDI-CI
struct.

```deviceId``` is provided by the Sysex byte containing the Device Id data. In MIDI-CI this is the Source or Destination
byte and represent either:
* 0x00 -0x0F Channel 1-16
* 0x7E The current UMP Group
* 0x7F The entire Function Block.

#### void endSysex7();
#### void processMIDICI(uint8_t s7Byte);

#### inline void setCheckMUID(checkMUIDCallback)
This is a simple check to make sure that the message being processed is meant for this application. 
Return true if it is a match.

```c++
uint32_t localMUID = random(0xFFFFEFF);

bool checkMUIDCallback(uint8_t umpGroup, uint32_t muid){
    return (localMUID==muid);
}
```

_Note: it is recommended that all instances of this class support this callback._

## Common structs in use

#### MIDICI
```c++
struct MIDICI{
    uint8_t umpGroup; //zero-based
    uint8_t deviceId; //0x00-0x0F Channels 1-16, 0x7E Group, 0x7F Function Blocks
    uint8_t ciType;
    uint8_t ciVer; //0x1 - v1.1, 0x2 - v1.2
    uint32_t remoteMUID;
    uint32_t localMUID;
    uint8_t totalChunks; //Total Chunks reported on this Message (PE Only)
    uint8_t numChunk; //Current Chunk reported on this Message (PE Only)
    uint8_t requestId; //Current Request Id (PE Only)
};
```
Most of the callbacks will include a MIDI-CI struct that contain core information from MIDI-CI SysEx.
The ```umpGroup``` variable is set by startSysex7 method.

## Common MIDI-CI SysEx Methods

#### inline void setRecvDiscovery(recvDiscoveryCallback)
```c++
void recvDiscoveryCallback(struct MIDICI ciDetails, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                   std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t remoteciSupport,
                   uint16_t remotemaxSysex, uint8_t outputPathId
){
    //All MIDI-CI Devices shall reply to a Discovery message
    printf("Received Discover on Group %d remote MUID: %d\n", ciDetails.umpGroup, ciDetails.remoteMUID);
    unint8_t sysexBuffer[64];
    int len = CIMessage::sendDiscoveryReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, localMUID, ciDetails.remoteMUID,
                             {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID},
                             {DEVICE_VERSIONID},0,
                             512, outputPathId,
                             0 //fbIdx
    );
    sendOutSysex(ciDetails.umpGroup, sysexBuffer ,len );
}
```

_Note: it is recommended that all instances of this class support this callback._

####  inline void setRecvDiscoveryReply(recvDiscoveryReplyCallback)

```c++
void recvDiscoveryReplyCallback(struct MIDICI ciDetails, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                   std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t remoteciSupport,
                   uint16_t remotemaxSysex, uint8_t outputPathId, uint8_t functionBlockIdx
){
    printf("Received Discover Reply on Group %d remote MUID: %d\n", ciDetails.umpGroup, ciDetails.remoteMUID);
}
```

#### inline void setRecvNAK(recvNAKCallback)
```c++
void recvNAKCallback(struct MIDICI ciDetails, uint8_t origSubID, uint8_t statusCode, uint8_t statusData, 
        uint8_t* ackNakDetails, uint16_t messageLength, uint8_t* ackNakMessage
){
    printf("Received NAK on Group %d remote MUID: %d\n", ciDetails.umpGroup, ciDetails.remoteMUID);
}
```

#### inline void setRecvInvalidateMUID(void (\*fptr)(uint8_t umpGroup, uint32_t remoteMuid, uint32_t terminateMuid))

#### inline void setRecvUnknownMIDICI(void (\*fptr)(MIDICI ciDetails, uint8_t s7Byte))
#### inline void setRecvEndpointInfo(void (\*fptr)(MIDICI ciDetails, uint8_t status))

#### inline void setRecvEndpointInfoReply(void (\*fptr)(MIDICI ciDetails, uint8_t status, uint16_t infoLength, uint8_t\* infoData)

### Protocol Negotiation (MIDI-CI 1.1)
Protocol Negotiation is deprecated from MIDI-CI 1.2 onwards. However, Devices that support a later version of MIDI-CI
can still respond and handle Protocol Negotiation.

####  inline void setRecvProtocolAvailable(recvProtocolAvailableCallback)
This callback is triggered upon receiving a Reply to Protocol Negotiation. The ```midiCIProcessor``` triggers off 
this callback for every Protocol returned.
```c++
void recvProtocolAvailableCallback(struct MIDICI ciDetails, uint8_t authorityLevel, uint8_t* protocol){
    printf("Received Protocol Available on Group %d remote MUID: %d\n", ciDetails.umpGroup, ciDetails.remoteMUID);
}
```

####  inline void setRecvSetProtocol(recvSetProtocolCallback)
See _setRecvProtocolAvailable_ for the callback structure

####  inline void setRecvSetProtocolTest(void (\*fptr)(MIDICI ciDetails, uint8_t authorityLevel, bool testDataAccurate)
####  inline void setRecvSetProtocolConfirm(void (\*fptr)(MIDICI ciDetails, uint8_t authorityLevel))

### Profile Configuration 
#### inline void setRecvProfileInquiry(profileInquiryCallback)
Upon receiving a Profile Inquiry message the application should return a result. Please read the MIDI-CI v1.2 specifications
for more information.

```c++
void profileInquiryCallback(struct MIDICI ciDetails){

  printf("->profile Inquiry: remoteMuid %d Destination: %d",ciDetails.remoteMUID, destination);

  uint8_t profileNone[0] = {};
  unint8_t sysexBuffer[64];
  int len;
  if(ciDetails.deviceId == 0 || ciDetails.deviceId == 0x7F){
    uint8_t profileDrawBar[5] = {0x7E, 0x40, 0x01, 0x01};
    //return 1 enabled profile
    len = CIMessage::sendProfileListResponse(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, localMUID, ciDetails.remoteMUID, 
                                  0, 1, profileDrawBar, 0, profileNone);
    sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
  }

  if(ciDetails.deviceId == 0x7F){
   //return no profiles   
   len = CIMessage::sendProfileListResponse(sysexBuffer, 0x02, localMUID, ciDetails.remoteMUID, 
                                 0x7F, 0, profileNone, 0, profileNone);
   sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
  }
}
```

#### inline void setRecvProfileEnabled(profileEnabledCallback)
This callback can be triggered in 2 ways:
* Upon receiving a Reply to Profile Inquiry the ```midiCIProcessor``` triggers off this callback for every enabled profile
* Upon receiving a Profile Enabled Message

```c++
void profileEnabledCallback(struct MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t numberOfChannels){
  printf("->profile Enabled: remoteMuid %d Destination: %d",ciDetails.remoteMUID, destination);
}
```
_Note: ```numberOfChannels``` is set to 0 if this is triggered by Reply to Profile Inquiry or MIDI-CI 1.1 Profile 
Enabled Message._

#### inline void setRecvProfileDisabled(profileDisabledCallback)
See setRecvProfileEnabled for the callback structure.

This callback can be triggered in 3 ways:
* Upon receiving a Reply to Profile Inquiry the ```midiCIProcessor``` triggers off this callback for every disabled profile
* Upon receiving a Profile Disabled message
* Upon receiving a Profile Added Report message (MIDI-CI v1.2 and higher)

Profile Added Report messages always declare that the Profile is added to the list of disabled Profiles.

_Note: ```numberOfChannels``` is set to 0 if this is triggered by Reply to Profile Inquiry or MIDI-CI 1.1 Profile
  Disabled Message._


#### inline void setRecvSetProfileRemoved(profileRemovedCallback)
This callback is triggered upon receiving a Profile Remove Report
```c++
void profileRemovedCallback(struct MIDICI ciDetails, std::array<uint8_t, 5> profile){
  printf("->profile Removed: remoteMuid %d Destination: %d",ciDetails.remoteMUID, destination);
}
```

#### inline void setRecvProfileOn(profileOnCallback)
This callback is triggered upon receiving a Set Profile On message
```c++
void profileOnCallback(struct MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t numberOfChannels){
  printf("->profile On: remoteMuid %d Destination: %d",ciDetails.remoteMUID, destination);
}
```

#### inline void setRecvProfileOff(profileOffCallback)
This callback is triggered upon receiving a Set Profile Off message
```c++
void profileOffCallback(struct MIDICI ciDetails, std::array<uint8_t, 5> profile){
  printf("->profile Off: remoteMuid %d Destination: %d",ciDetails.remoteMUID, destination);
}
```

#### inline void setRecvProfileDetailsInquiry(void (\*fptr)(MIDICI ciDetails, std::array<uint8_t, 5> profile,   uint8_t InquiryTarget))
#### inline void setRecvProfileDetailsReply(void (\*fptr)(MIDICI ciDetails, std::array<uint8_t, 5> profile, uint8_t InquiryTarget, uint16_t datalen, uint8_t\*  data))

#### inline void setRecvProfileSpecificData(profileSpecificDataCallback)
This callback is triggered upon receiving a Profile Specific Data message.

A long Profile Specific Data message may have a length longer than __S7_BUFFERLEN__. As such this callback may be triggered 
multiple times for the same message. The ```lastByteOfSet``` bool is set to true if this is the last time this
callback is triggered.

```c++
void profileSpecificDataCallback(struct MIDICI ciDetails, std::array<uint8_t, 5> profile, uint16_t datalen, uint8_t\*  data, uint16_t part, bool lastByteOfSet){
  printf("->profile Off: remote MUID %d Destination: %d",ciDetails.remoteMUID, destination);
}
```

_Note: While data length field in the SysEx can declare a potential length of 268435455 bytes, it is extremely unlikely
that this is feasible, so it is kept as an uin16_t value._

### Property Exchange

This library provides the Header received on each Property Exchange message as a std::string. This is to allow the 
developer to choose their own JSON parser that best suits the environment. 

#### inline void setPECapabilities(PECapabilityCallback)
Upon receiving an Inquiry: Property Exchange Capabilities message the application should send back a Reply to Property 
Exchange Capabilities message.
```c++
void PECapabilityCallback(struct MIDICI ciDetails, uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer){
	printf("->PE Capability: remote MUID %d", ciDetails.remoteMUID);
    uint8_t sysexBuffer[64];
	int len = CIMessage::sendPECapabilityReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, localMUID, ciDetails.remoteMUID, 4, 0, 0);
    sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
}
```

#### inline void setPECapabilitiesReply(PECapabilityReplyCallback)
See setPECapabilities for the structure of PECapabilityReplyCallback

#### inline void setRecvPEGetInquiry(PEGetInquiryCallback)
This callback is triggered upon receiving an Inquiry: Get Property Data message.
```c++
void PEGetInquiryCallback( struct MIDICI ciDetails, std::string header){
  printf("->PE GET Inquiry: remote MUID %d header %s", ciDetails.remoteMUID, 
         ciDetails.requestId, header.c_str());
  
  if (header.contains("LocalOn")){
	string header = "{\"status\":200}";
    uint8_t sysexBuffer[128];
	int len = CIMessage::sendPEGetReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, localMUID, ciDetails.remoteMUID,\
        ciDetails.requestId, header.length(), (uint8_t*)header.c_str(), 1, 1, 4, (uint8_t*)"true");
    sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
  }
}
  
```
#### inline void setRecvPESetInquiry(PESetCallback)

This callback is triggered upon receiving an Inquiry: Set Property Data message.

A long Inquiry: Set Property Data message is usually sent over many chunks and each chunck may have a length 
longer than __S7_BUFFERLEN__. As such this callback may be triggered multiple times for the same set of messages.

The ```lastByteOfChunk``` bool is set to true if this is the last body data in a single chunk message.
The ```lastByteOfSet``` bool is set to true if this is the last time this callback is triggered for a set of messages.

```c++
mcoded7Decode m7d;

void PESetCallback(struct MIDICI ciDetails,  std::string header, uint16_t bodyLen, uint8_t*  body, 
        bool lastByteOfChunk, bool lastByteOfSet){
  if (header.contains("State")){
      //This code assumes the data is using Mcoded7
      if (requestDetails.numChunk == 1 && requestDetails.partialChunkCount == 1){
          file.open(fullPath,  O_RDWR | O_TRUNC | O_CREAT);
          m7d.reset();
      }
      for(uint16_t i=0; i < bodyLen;i++){
          m7d.parseS7Byte(body[i]);
          if(m7d.currentPos() == 7){
              file.write(m7d.dump, 7);
              m7d.reset();
          }
      }

      if(lastByteOfSet){
          file.write(m7d.dump,m7d.currentPos());
          file.close();
          string header = "{\"status\":200}";
          uint8_t sysexBuffer[128];
          int len = CIMessage::sendPESetReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, localMUID, ciDetails.remoteMUID, requestDetails.requestId,
                       header.length, (uint8_t*)header.c_str());
          sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
      }
    }
}
```

#### inline void setRecvPESetReply(PESetReplyCallback)
See _setRecvPEGetInquiry_ for the callback structure

#### inline void setRecvPESubInquiry(PESubInquiryCallback)
```c++
void PESubInquiryCallback(struct MIDICI ciDetails,  std::string header, uint16_t bodyLen, uint8_t*  body,
                   bool lastByteOfChunk, bool lastByteOfSet);
```

#### inline void setRecvPESubReply(PESubReplyCallback)
See _setRecvPEGetInquiry_ for the callback structure

#### inline void setRecvPENotify(PENotifyCallback)
See _setRecvPEGetInquiry_ for the callback structure

### Process Inquiry (MIDI-CI 1.2 onwards)

#### inline void setRecvPICapabilities(PICapabilitiesCallback)

```c++
void PICapabilitiesCallback(struct MIDICI ciDetails){
    printf("MIDI Process Inquiry Capabilities");
    uint8_t sysexBuffer[128];
    int len = CIMessage::sendPICapabilityReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, ciDetails.localMUID, ciDetails.remoteMUID,  1);
    sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
}
```

#### inline void setRecvPICapabilitiesReply(PICapabilitiesReplyCallback)
See _setRecvPICapabilities_ for the callback structure

#### inline void setRecvPIMMReport(PIMMReportCallback)

```c++
void PIMMReportCallback(struct MIDICI ciDetails, uint8_t MDC, uint8_t systemBitmap,
                                                    uint8_t chanContBitmap, uint8_t chanNoteBitmap){
    uint8_t sysexBuffer[128];
    int len;
    len = CIMessage::sendPIMMReportReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, ciDetails.localMUID, ciDetails.remoteMUID, ciDetails.deviceId,
                              0, 0b10010 &  chanContBitmap, 0);
    sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);

    if(
        (MDC & 0b1) &&
        (chanContBitmap & 0b10) &&
        (midiChannel == ciDetails.deviceId || ciDetails.deviceId == 0x7f)
        ){
        
        sendControlChange(7, volume>>3, midiChannel);
        sendControlChange(14, osc1Volume>>3, midiChannel);
        sendControlChange(94, map(osc1Detune, -24, 24, 0, 127), midiChannel);
        sendControlChange(74, map(filterCutoff, 0, 255, 0, 127), midiChannel);
        sendControlChange(73, attackTime>>3, midiChannel);
        sendControlChange(76, decayTime>>3, midiChannel);
        sendControlChange(77, sustainLevel>>3, midiChannel);
        sendControlChange(72, releaseTime>>3, midiChannel);
    }
    if(
        (MDC & 0b1) &&
        (chanContBitmap & 0b10000) &&
        (midiChannel == ciDetails.deviceId  || ciDetails.deviceId == 0x7f)
        ){
        sendProgramChange((uint8_t)patchLoaded,midiChannel);
    }
    len = CIMessage::sendPIMMReportEnd(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, ciDetails.localMUID, ciDetails.remoteMUID, ciDetails.deviceId);
    sendOutSysex(ciDetails.umpGroup, sysexBuffer, len);
}

```

#### inline void setRecvPIMMReportReply(PIMMReportReplyCallback)
See _setRecvPIMMReport_ for the callback structure

#### inline void setRecvPIMMEnd(void (\*fptr)(MIDICI ciDetails))
