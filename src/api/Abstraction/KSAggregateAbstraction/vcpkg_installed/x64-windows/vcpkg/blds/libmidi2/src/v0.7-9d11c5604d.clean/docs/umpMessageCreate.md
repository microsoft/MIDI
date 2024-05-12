# UMP Message Create
Please refer to the UMP and MIDI 2.0 Protocol version 1.1 on the meaning of each of these
MIDI messages.

## Utillty Messages (Message Type 0x0)
#### uint32_t mt0NOOP();
#### uint32_t mt0JRClock(uint16_t clockTime);
#### uint32_t mt0JRTimeStamp(uint16_t timestamp);
#### uint32_t mt0DeltaClockTick( uint16_t ticksPerQtrNote);
#### uint32_t mt0DeltaTicksSinceLast( uint16_t noTicksSince);


## System Messages (Message Type 0x1)
#### uint32_t mt1MTC(uint8_t umpGroup, uint8_t timeCode);
#### uint32_t mt1SPP(uint8_t umpGroup, uint16_t position);
#### uint32_t mt1SongSelect(uint8_t umpGroup, uint8_t song);
#### uint32_t mt1TuneRequest(uint8_t umpGroup);
#### uint32_t mt1TimingClock(uint8_t umpGroup);
#### uint32_t mt1SeqStart(uint8_t umpGroup);
#### uint32_t mt1SeqCont(uint8_t umpGroup);
#### uint32_t mt1SeqStop(uint8_t umpGroup);
#### uint32_t mt1ActiveSense(uint8_t umpGroup);
#### uint32_t mt1SystemReset(uint8_t umpGroup);



## MIDI 1.0 Channel Voice Messages (Message Type 0x2)
#### uint32_t mt2NoteOn(uint8_t umpGroup, uint8_t channel, uint8_t noteNumber, uint16_t velocity);
#### uint32_t mt2NoteOff(uint8_t umpGroup, uint8_t channel, uint8_t noteNumber, uint16_t velocity);
#### uint32_t mt2PolyPressure(uint8_t umpGroup, uint8_t channel, uint8_t noteNumber, uint32_t pressure);
#### uint32_t mt2CC(uint8_t umpGroup, uint8_t channel, uint8_t index, uint32_t value);
#### uint32_t mt2ProgramChange(uint8_t umpGroup, uint8_t channel, uint8_t program);
#### uint32_t mt2ChannelPressure(uint8_t umpGroup, uint8_t channel, uint32_t pressure);
#### uint32_t mt2PitchBend(uint8_t umpGroup, uint8_t channel, uint32_t value);

## System Exclusive 7 (Message Type 0x3)
System Exclusive 7 messages return ```std::array<uint32_t, 2>```.

#### std::array<uint32_t, 2> mt3Sysex7(uint8_t umpGroup, uint8_t status, uint8_t numBytes, std::array<uint8_t, 6> sx);
It is up to the application to convert Sysex and generate the correct status in this function. Below is example code to
output UMP Sysex from a complete array of SysEx bytes.

```c++
void sendOutSysex(uint8_t umpGroup, uint8_t *sysex, uint32_t length ){
    std::array<uint8_t, 6> sx = {0,0,0,0,0,0};
    uint8_t sxPos=0;

    for (int i = 0; i < length; i++) {
        sx[sxPos++]=sysex[i] & 0x7F;
        if(sxPos == 6){
            std::array<uint32_t, 2> ump = mt3Sysex7(umpGroup, i < 6 ? 1 : i==length-1 ? 3 : 2, 6, sx);
            sendUMP(ump.data(),2);
            sxPos=0;
        }
    }
    if (sxPos) {
        std::array<uint32_t, 2> ump = mt3Sysex7(umpGroup, length < 7 ? 0 : 3, sxPos, sx);
        sendUMP(ump.data(),2);
    }

}
```


## MIDI 2.0 Channel voice messages (Message Type 0x4)
All MIDI 2.0 Channel voice messages return ```std::array<uint32_t, 2>```.

#### std::array<uint32_t, 2> mt4NoteOn(uint8_t umpGroup, uint8_t channel, uint8_t noteNumber, uint16_t velocity, uint8_t attributeType, uint16_t attributeData);
#### std::array<uint32_t, 2> mt4NoteOff(uint8_t umpGroup, uint8_t channel, uint8_t noteNumber, uint16_t velocity, uint8_t attributeType, uint16_t attributeData);
#### std::array<uint32_t, 2> mt4CPolyPressure(uint8_t umpGroup, uint8_t channel, uint8_t noteNumber, uint32_t pressure);
#### std::array<uint32_t, 2> mt4PitchBend(uint8_t umpGroup, uint8_t channel, uint32_t pitch);
#### std::array<uint32_t, 2> mt4CC(uint8_t umpGroup, uint8_t channel, uint8_t index, uint32_t value);
#### std::array<uint32_t, 2> mt4RPN(uint8_t umpGroup, uint8_t channel,uint8_t bank,  uint8_t index, uint32_t value);
#### std::array<uint32_t, 2> mt4NRPN(uint8_t umpGroup, uint8_t channel,uint8_t bank,  uint8_t index, uint32_t value);
#### std::array<uint32_t, 2> mt4RelativeRPN(uint8_t umpGroup, uint8_t channel,uint8_t bank,  uint8_t index, int32_t value);
#### std::array<uint32_t, 2> mt4RelativeNRPN(uint8_t umpGroup, uint8_t channel,uint8_t bank,  uint8_t index, int32_t value);
#### std::array<uint32_t, 2> mt4ChannelPressure(uint8_t umpGroup, uint8_t channel,uint32_t pressure);
#### std::array<uint32_t, 2> mt4ProgramChange(uint8_t umpGroup, uint8_t channel, uint8_t program, bool bankValid, uint8_t bank, uint8_t index);


## Flex Data Messages (Message Type 0x5)
MIDI 2.0 Channel voice messages return ```std::array<uint32_t, 4>```.

_This section is in progress_


## UMP Stream messages (Message Type 0xF)
UMP Stream messages return ```std::array<uint32_t, 4>```.

#### std::array<uint32_t, 4> mtFMidiEndpoint(uint8_t filter);

#### std::array<uint32_t, 4> mtFMidiEndpointInfoNotify(uint8_t numOfFuncBlock, bool m2, bool m1, bool rxjr, bool txjr);
#### std::array<uint32_t, 4> mtFMidiEndpointDeviceInfoNotify(std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId, std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version);
#### std::array<uint32_t, 4> mtFMidiEndpointTextNotify(uint16_t replyType, uint8_t offset, uint8_t* text, uint8_t textLen);

#### std::array<uint32_t, 4> mtFFunctionBlock(uint8_t fbIdx, uint8_t filter);
#### std::array<uint32_t, 4> mtFFunctionBlockInfoNotify(uint8_t fbIdx, bool active, uint8_t direction,  bool sender, bool recv, uint8_t firstGroup, uint8_t groupLength, uint8_t midiCISupport, uint8_t isMIDI1, uint8_t maxS8Streams);
#### std::array<uint32_t, 4> mtFFunctionBlockNameNotify(uint8_t fbIdx, uint8_t offset, uint8_t* text, uint8_t textLen);

#### std::array<uint32_t, 4> mtFStartOfSeq();
#### std::array<uint32_t, 4> mtFEndOfFile();
#### std::array<uint32_t, 4> mtFRequestProtocol(uint8_t protocol, bool jrrx, bool jrtx);
#### std::array<uint32_t, 4> mtFNotifyProtocol( uint8_t protocol, bool jrrx, bool jrtx);
	