# umpProcessor
Processing of UMP messages.

This class allows the application to set up a set of callbacks relevant to the applications needs.

__Example Setup__
```c++
umpProcessor UMPHandler;

UMPHandler.setUtility(utilityCallback);
UMPHandler.setCVM(handleChannelVoiceMessages);
UMPHandler.setSystem(handleSystemMessages);
UMPHandler.setSysEx(processUMPSysex);

UMPHandler.setMidiEndpoint(midiEndpointCallback);
UMPHandler.setFunctionBlock(functionblockCallback);

...
```

## Structs
### umpCVM - UMP Channel Voice Message 
UMP messages of Message Type 0x2 and 0x4 are presented in this format. See below for the value meaning for each status. 
```c++
struct umpCVM{
    uint8_t umpGroup;
    uint8_t messageType;
    uint8_t status;
    uint8_t channel;
    uint8_t note;
    uint32_t value;
    uint16_t index;
    uint8_t bank;
    bool flag1;
    bool flag2;
};
```
### umpGeneric - UMP Generic Structure
UMP messages of Message Type 0x0 and 0x1 are presented in this format. See below for the value meaning for each status.
```c++
struct umpGeneric{
    uint8_t umpGroup;
    uint8_t messageType;
    uint8_t status;
    uint16_t value;
};
```
_Note: Utility Messages (MT 0x0) are groupless and therefore the ```umpGproup``` value is ignored._

### umpData - UMP Data Messages
```c++
struct umpData{
    uint8_t umpGroup;
    uint8_t messageType;
    uint8_t status;
    uint8_t form;
    uint8_t* data;
    uint8_t dataLength;
};
```

## Common Methods
### void processUMP(uint32_t UMP)
Process incoming UMP messages broken up into 32bit words.
### void clearUMP()
Reset the current processing of incoming UMP.

## Utility message Handlers
### inline void setUtility(utilityCallback)
```c++ 
void utilityCallback(struct umpGeneric mess){
    printf("->Utility Message: status %d value: %d", mess.status, mess.value);
} 
```
__Values in the umpGeneric struct:__

| status                   | value                          |
|--------------------------|--------------------------------|
| UTILITY_NOOP             | -                              |
| UTILITY_JRCLOCK          | sender clock time              |
| UTILITY_JRTS             | sender clock timestamp         |
| UTILITY_DELTACLOCKTICK   | # of ticks PQN                 |
| UTILITY_DELTACLOCKSINCE  | # of ticks since last event    |


## Common Channel Voice Message Handler

### inline void setCVM(handleChannelVoiceMessages)
Set the callable function when a Channel Voice Message is processed by ```processUMP```
```c++ 
void handleChannelVoiceMessages(struct umpCVM mess){
    printf("->CVM: Group %d CH %d Note: %d status: %d", mess.umpGroup, mess.channel, mess.status);
} 
```

__Values in the umpCVM struct:__

| status               | note  | value     | bank          | index         | flag1      | flag2  |     
|----------------------|-------|-----------|---------------|---------------|------------|--------|
| NOTE_OFF             | note  | velocity♠ | attributeType | attributeData |            |        |
| NOTE_ON              | note  | velocity♠ | attributeType | attributeData |            |        |
| KEY_PRESSURE         | note  | value\*   |               |               |            |        |
| CC                   |       | value\*   |               | index         |            |        |
| PROGRAM_CHANGE       |       | program   | bank          | index         | bank valid |        |
| CHANNEL_PRESSURE     |       | value\*   |               |               |            |        |
| PITCH_BEND           |       | value\*   |               |               |            |        |
| RPN †♥               |       | value     | bank          | index         |            |        |
| NRPN †♥              |       | value     | bank          | index         |            |        |
| RPN_RELATIVE †       |       | value‡    | bank          | index         |            |        |
| NRPN_RELATIVE †      |       | value‡    | bank          | index         |            |        |
| PITCH_BEND_PERNOTE † | note  | value     |               |               |            |        |
| RPN_PERNOTE †        | note  | value     |               | index         |            |        |
| NRPN_PERNOTE †       | note  | value     |               | index         |            |        |
| PERNOTE_MANAGE †     | note  |           |               |               | detach     | reset  |

_† MIDI 2.0 Protocol messages only_  
_\* M1 Values are scaled to 32 bit value_  
_♠ M1 Values are scaled to 16 bit value_  
_♥ Message is only triggered when a MIDI 2.0 RPN message is sent. This is not triggered when a MIDI 1.0 (N)RPN
messages are sent. Those messages are processed using the function set by ```setControlChange```_  
_‡ These values are twos complement and will need to cast e.g:_
```c++
int32_t relativeValue = (int32_t)mess.value;
```

The ```umpProcessor``` makes some distinction between different Protocols. This means that Channel Voice Messages (e.g. 
Note On) handlers are called the same way regardless if is a MIDI 1.0 Channel Voice Message (Message Type 0x2) or a MIDI 
2.0 Channel Voice Message (Message Type 0x4). MIDI 1.0 Channel Voice Message values are scaled to match MIDI 2.0 Messages. 

This allows for ```umpProcessor``` to process both types of Channel Voice Messages simultaneously.

It is up to the application to manage the combination of JR messages and other UMP messages.

## Common System Message Handler

### inline void setSystem(handleSystemMessages)
Set the callable function when a System Message is processed by ```processUMP```
```c++ 
void handleSystemMessages(struct umpGeneric mess){
    printf("->CVM: Group %d status: %d", mess.umpGroup, mess.status);
} 
```

__Values in the umpGeneric struct:__

| status      | value     |
|-------------|-----------|
| TIMING_CODE | time code |
| SPP         | position  |
| SONG_SELECT | song      |
| TUNEREQUEST |           |
| TIMINGCLOCK |           |
| SEQSTART    |           |
| SEQCONT     |           |
| SEQSTOP     |           |
| ACTIVESENSE |           |
| SYSTEMRESET |           |


## Common SysEx Handler

###  inline void setRawSysEx(processUMPSysex)
```c++ 
midiCIProcessor midiciMain1;
bool isProcMIDICI = false;

void processUMPSysex(struct umpData mess){
    //Example of Processing UMP into MIDI-CI processor
    if(mess.form==1 && mess.data[0] == S7UNIVERSAL_NRT && mess.data[2] == S7MIDICI){
        if(mess.umpGroup==0) {
            midiciMain1.startSysex7(mess.umpGroup, mess.data[1]);
            isProcMIDICI = true;
        }
    }
    for (int i = 0; i < mess.dataLength; i++) {
        if(mess.umpGroup==0 && isProcMIDICI){
            midiciMain1.processMIDICI(mess.data[i]);
        }else{
            //Process other SysEx
        }
    }
    if((mess.form==3 || mess.form==0) && isProcMIDICI){
        midiciMain1.endSysex7();
        isProcMIDICI = false;
    }
}
```

## Flex Data Handlers
### inline void setFlexTempo(void (*fptr)(uint8_t group, uint32_t num10nsPQN))
### inline void setFlexTimeSig(void (*fptr)(uint8_t group, uint8_t numerator, uint8_t denominator, uint8_t num32Notes))
### inline void setFlexMetronome(void (*fptr)(uint8_t group, uint8_t numClkpPriCli, uint8_t bAccP1, uint8_t bAccP2, uint8_t bAccP3, uint8_t numSubDivCli1, uint8_t numSubDivCli2))
### inline void setFlexKeySig(void (*fptr)(uint8_t group, uint8_t addrs, uint8_t channel, uint8_t sharpFlats, uint8_t tonic))
### inline void setFlexChord(void (*fptr)(uint8_t group, uint8_t addrs, uint8_t channel, uint8_t chShrpFlt, uint8_t chTonic, uint8_t chType, uint8_t chAlt1Type, uint8_t chAlt1Deg, uint8_t chAlt2Type, uint8_t chAlt2Deg, uint8_t chAlt3Type, uint8_t chAlt3Deg, uint8_t chAlt4Type, uint8_t chAlt4Deg, uint8_t baShrpFlt, uint8_t baTonic, uint8_t baType, uint8_t baAlt1Type, uint8_t baAlt1Deg, uint8_t baAlt2Type, uint8_t baAlt2Deg))
### inline void setFlexPerformance(void (*fptr)(uint8_t group, uint8_t form, uint8_t addrs, uint8_t channel, uint8_t status, uint8_t* text, uint8_t textLength))
### inline void setFlexLyric(void (*fptr)(uint8_t group, uint8_t form, uint8_t addrs, uint8_t channel, uint8_t status, uint8_t* text, uint8_t textLength))

## UMP Stream Messages

### inline void setMidiEndpoint(midiEndpointCallback)

__Example Processing of Endpoint Get:__
```c++ 
void midiEndpointCallback(uint8_t majVer, uint8_t minVer, uint8_t filter){
    //Upon Recieving the filter it is important to return the information requested
    if(filter & 0x1){ //Endpoint Info Notification
        std::array<uint32_t, 4> ump = mtFMidiEndpointInfoNotify(3, false, true, false, false);
        sendUMP(ump.data(),4);
    }

    if(filter & 0x2) {
        std::array<uint32_t, 4> ump = mtFMidiEndpointDeviceInfoNotify({DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID}, {DEVICE_VERSIONID});
        sendUMP( ump.data(), 4);
    }

    if(filter & 0x4) {
        uint8_t friendlyNameLength = strlen(DEVICE_MIDIENPOINTNAME);
        for(uint8_t offset=0; offset<friendlyNameLength; offset+=14) {
            std::array<uint32_t, 4> ump = mtFMidiEndpointTextNotify(MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_MIDIENPOINTNAME,friendlyNameLength);
            sendUMP(ump.data(),4);
        }
    }
    
    if(filter & 0x8) {
        int8_t piiLength = strlen(PRODUCT_INSTANCE_ID);

        for(uint8_t offset=0; offset<piiLength; offset+=14) {
            std::array<uint32_t, 4> ump = mtFMidiEndpointTextNotify(PRODUCT_INSTANCE_ID, offset, (uint8_t *) buff,piiLength);
            sendUMP(ump.data(),4);
        }
    }
    
    if(filter & 0x10){
        std::array<uint32_t, 4> ump = mtFNotifyProtocol(0x1,false,false);
        sendUMP(ump.data(),4);
    }
} 
```
### inline void setMidiEndpointNameNotify(void (\*fptr)(struct umpData mess))
### inline void setMidiEndpointProdIdNotify(void (\*fptr)(struct umpData mess))
### inline void setMidiEndpointInfoNotify(void (\*fptr)(uint8_t majVer, uint8_t minVer, uint8_t numOfFuncBlocks, bool m2, bool m1, bool rxjr, bool txjr))
### inline void setMidiEndpointDeviceInfoNotify(void (\*fptr)(std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId, std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version))
### inline void setJRProtocolRequest(void (\*fptr)(uint8_t protocol, bool jrrx, bool jrtx))
### inline void setJRProtocolNotify(void (\*fptr)(uint8_t protocol, bool jrrx, bool jrtx))
### inline void setFunctionBlock(void (\*fptr)(uint8_t filter, uint8_t fbIdx))
### inline void setFunctionBlockNotify(void (\*fptr)(uint8_t fbIdx, bool active, uint8_t direction, uint8_t firstGroup, uint8_t groupLength, bool midiCIValid, uint8_t midiCIVersion, uint8_t isMIDI1, uint8_t maxS8Streams))
### inline void setFunctionBlockNotify(void (\*fptr)(struct umpData mess, uint8_t fbIdx))

### inline void setStartOfSeq(void (*fptr)()){startOfSeq = fptr; }
### inline void setEndOfFile(void (*fptr)()){endOfFile = fptr; }
