# MIDI-CI SysEx Creation

These function create the SysEx needed for various MIDI-CI messages. It creates the complete SysEx uint8_t array 
minus the 0xF0 and 0xF7 at beginning and end of the SysEx.

For each message a sysex buffer is needed. The return of each function is the length of the buffer used e.g.:
```c++
uint8_t sysexBuffer[512];
int len = CIMessage::sendDiscoveryReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION,localMUID, ciDetails.remoteMUID,
                             {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID},
                             {DEVICE_VERSIONID},0,
                             512, outputPathId, 0
);
sendOutSysextoUMP(umpGroup, sysexBuffer, len);
```
_Note: It is upto the application to make sure that the buffer is larger that the created message_

The SysEx created will depend on the ```midiCIVer``` value specified. For example if a Discovery Reply message was sent where 
the value is set to __1__, then the message created will not have Output Path Id and Function Block Index.

MIDI-CI Messages that are only available from certain version will return a length of 0 if the ```midiCIVer``` is too low.

#### uint16_t CIMessage::sendDiscoveryRequest(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId, std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t ciSupport, uint32_t sysExMax, uint8_t outputPathId)
#### uint16_t CIMessage::sendDiscoveryReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId, std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t ciSupport, uint32_t sysExMax, uint8_t outputPathId, uint8_t fbIdx)
#### uint16_t CIMessage::sendEndpointInfoRequest(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t status);
#### uint16_t CIMessage::sendEndpointInfoReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t status, uint16_t infoLength, uint8_t* infoData)

#### uint16_t CIMessage::sendACK(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, uint8_t originalSubId, uint8_t statusCode, uint8_t statusData, uint8_t* ackNakDetails, uint16_t messageLength, uint8_t* ackNakMessage)
#### uint16_t CIMessage::sendNAK(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, uint8_t originalSubId, uint8_t statusCode, uint8_t statusData, uint8_t* ackNakDetails, uint16_t messageLength, uint8_t* ackNakMessage)

#### uint16_t CIMessage::sendInvalidateMUID(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t terminateMuid);

## Protocol Negotiation (MIDI-CI 1.1)
Protocol Negotiation is deprecated from MIDI-CI 1.2 onwards. However, Devices that support a later version of MIDI-CI
can still respond and handle Protocol Negotiation. 

#### uint16_t CIMessage::sendProtocolNegotiation(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t authorityLevel, uint8_t numProtocols, uint8_t* protocols, uint8_t* currentProtocol)
#### uint16_t CIMessage::sendProtocolNegotiationReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t authorityLevel, uint8_t numProtocols, uint8_t* protocols)
#### uint16_t CIMessage::sendSetProtocol(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t authorityLevel, uint8_t* protocol)
#### uint16_t CIMessage::sendProtocolTest(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t authorityLevel)
#### uint16_t CIMessage::sendProtocolTestResponder(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t authorityLevel)

## Profile Negotiation
#### uint16_t CIMessage::sendProfileListRequest(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,  uint8_t destination)
#### uint16_t CIMessage::sendProfileListResponse(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, uint8_t profilesEnabledLen, uint8_t* profilesEnabled, uint8_t profilesDisabledLen, uint8_t* profilesDisabled)
```profilesEnabledLen``` and ```profilesDisabledLen``` represent how many Profiles. ```profilesEnabled``` and ```profilesDisabled``` arguments should be 5 times the length of ```profilesEnabledLen``` and ```profilesDisabledLen``` respectively.

```c++
void handleProfileInquiry(uint8_t umpGroup, uint32_t remoteMUID, uint8_t destination){  
  uint8_t profileNone[0] = {};
  uint8_t sysexBuffer[512];
  int len;
  
  // If a Profile Inquiry is received where destination = 0x7F, you should also return 
  // the Profiles on each channel. In this example Destination of 0 = channel 1, so
  // the Profile is also returned for Channel 1 or destination = 0x7F
  if(destination == 0 || destination == 0x7F){
    uint8_t profileDrawBar[5] = {0x7E, 0x40, 0x01, 0x01};
    len = CIMessage::sendProfileListResponse(umpGroup, MIDICI_MESSAGE_FORMAT, remoteMUID, 1, 0, 1, profileDrawBar, 0, profileNone);
    sendOutSysextoUMP(umpGroup, sysexBuffer, len);
  }

  if(destination == 0x7F){
   len = CIMessage::sendProfileListResponse(umpGroup, MIDICI_MESSAGE_FORMAT, remoteMUID, 1, 0x7F, 0, profileNone, 0, profileNone);
   sendOutSysextoUMP(umpGroup, sysexBuffer, len);
  }
}

midi2Processor MIDI2 (0,2,2);
...
MIDI2.setRecvProfileInquiry(profileInquiry);
```

#### uint16_t CIMessage::sendProfileAdd(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile)
#### uint16_t CIMessage::sendProfileRemove(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile)

#### uint16_t CIMessage::sendProfileOn(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile, uint8_t numberOfChannels)
#### uint16_t CIMessage::sendProfileOff(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile)
#### uint16_t CIMessage::sendProfileEnabled(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile, uint8_t numberOfChannels)
#### uint16_t CIMessage::sendProfileDisabled(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile, uint8_t numberOfChannels)

#### uint16_t CIMessage::sendProfileSpecificData(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile, uint16_t datalen, uint8_t*  data)
#### uint16_t CIMessage::sendProfileDetailsInquiry(uint8_t* sysex, uint8_t midiCIVer,  uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile, uint8_t InquiryTarget)
#### uint16_t CIMessage::sendProfileDetailsReply(uint8_t* sysex, uint8_t midiCIVer,  uint32_t srcMUID, uint32_t destMuid, uint8_t destination, std::array<uint8_t, 5> profile, uint8_t InquiryTarget, uint16_t datalen, uint8_t*  data)

## Property Exchange
#### uint16_t CIMessage::sendPECapabilityRequest(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,  uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer)
#### uint16_t CIMessage::sendPECapabilityReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,  uint8_t numSimulRequests, uint8_t majVer, uint8_t minVer)

#### uint16_t CIMessage::sendPEGet(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header)
#### uint16_t CIMessage::sendPEGetReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header, uint16_t numberOfChunks, uint16_t numberOfThisChunk, uint16_t bodyLength , uint8_t* body )
A complete reply to an Inquiry: Get Property Data can be split over one or more Reply to Get Property Data 
messages. Here is an examples of how this can be done.

__Example Sending a JSON string in 512 byte Sysex message chunks__
```c++
void returnPE(uint8_t umpGroup, uint32_t remoteMUID, uint8_t requestId, char *propertyData, 
                    unint32_t propertyDataLength){
  uint8_t sysexBuffer[512];
  std::string header = "{\"status\":200}";
  int totalChunks = ceil((double)(strL + header.length())/480); //480 is 512 minus the bytes used for heading etc.
  for (int chunk = 1; chunk <= totalChunks; chunk ++){
    int hLen = chunk == 1 ? header.length() : 0;
    int bodyLen = 480 - hLen;
    if (bodyLen > propertyDataLength) bodyLen = propertyDataLength;

    int len = CIMessage::sendPEGetReply(sysexBuffer, MIDICI_MESSAGE_FORMAT, localMUID, remoteMUID, requestId, hLen, 
		                     (uint8_t*)header.c_str(), totalChunks, chunk, bodyLen, 
		                     (uint8_t*)(propertyData + (((chunk - 1) * 480) - (chunk == 1 ? 0 : hLen))));
    sendOutSysextoUMP(umpGroup, sysexBuffer, len);
    propertyDataLength -= bodyLen ;
  }
}
```

#### uint16_t CIMessage::sendPESet(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header, uint16_t numberOfChunks, uint16_t numberOfThisChunk, uint16_t bodyLength , uint8_t* body)

#### uint16_t CIMessage::sendPESetReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header)

#### uint16_t CIMessage::sendPESub(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header, uint16_t numberOfChunks, uint16_t numberOfThisChunk, uint16_t bodyLength , uint8_t* body)


#### uint16_t CIMessage::sendPESubReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header)
#### uint16_t CIMessage::sendPENotify(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t requestId, uint16_t headerLen, uint8_t* header)


## Process Inquiry
#### uint16_t CIMessage::sendPICapabilityRequest(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid)
#### uint16_t CIMessage::sendPICapabilityReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid,  uint8_t supportedFeatures)
#### uint16_t CIMessage::sendPIMMReport(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, uint8_t MDC,  uint8_t systemBitmap, uint8_t chanContBitmap, uint8_t chanNoteBitmap)
#### uint16_t CIMessage::sendPIMMReportReply(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination, uint8_t systemBitmap, uint8_t chanContBitmap, uint8_t chanNoteBitmap)
#### uint16_t CIMessage::sendPIMMReportEnd(uint8_t* sysex, uint8_t midiCIVer, uint32_t srcMUID, uint32_t destMuid, uint8_t destination)