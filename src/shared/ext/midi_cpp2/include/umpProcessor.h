/**********************************************************
 * MIDI 2.0 Library 
 * Author: Andrew Mee
 * 
 * MIT License
 * Copyright 2021 Andrew Mee
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * ********************************************************/
#ifndef UMP_PROCESSOR_H
#define UMP_PROCESSOR_H

#include <cstdint>
#include <array>
#include <functional>

#include "utils.h"

struct umpCVM{
    umpCVM() : umpGroup(255), messageType(255), status(0),channel(255),note(255), value(0),  index(0), bank(0),
         flag1(false), flag2(false) {}
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

struct umpGeneric{
    umpGeneric() : umpGroup(255), status(0),  value(0) {}
    uint8_t umpGroup;
    uint8_t messageType;
    uint8_t status;
    uint16_t value;
};

struct umpData{
    umpData() : umpGroup(255), status(0),  form(0) {}
    uint8_t umpGroup;
    uint8_t messageType;
    uint8_t status;
    uint8_t form;
    uint8_t* data;
    uint8_t dataLength;
};

class umpProcessor{
    
  private:
  
	uint32_t umpMess[4]{};
	uint8_t messPos=0;

    // Message type 0x0  callbacks
    std::function<void(struct umpGeneric mess)> utilityMessage = nullptr;

	// MIDI 1 and 2 CVM  callbacks
    std::function<void(struct umpCVM mess)> channelVoiceMessage = nullptr;
    
   //System Messages  callbacks
   std::function<void(struct umpGeneric mess)> systemMessage = nullptr;

    //Sysex
    std::function<void(struct umpData mess)> sendOutSysex = nullptr;

    // Message Type 0xD  callbacks
    std::function<void(uint8_t group, uint32_t num10nsPQN)> flexTempo = nullptr;
    std::function<void(uint8_t group, uint8_t numerator, uint8_t denominator, uint8_t num32Notes)> flexTimeSig = nullptr;
    std::function<void(uint8_t group, uint8_t numClkpPriCli, uint8_t bAccP1, uint8_t bAccP2, uint8_t bAccP3,
            uint8_t numSubDivCli1, uint8_t numSubDivCli2)> flexMetronome = nullptr;
    std::function<void(uint8_t group, uint8_t addrs, uint8_t channel, uint8_t sharpFlats, uint8_t tonic)> flexKeySig = nullptr;
    std::function<void(uint8_t group, uint8_t addrs, uint8_t channel, uint8_t chShrpFlt, uint8_t chTonic,
            uint8_t chType, uint8_t chAlt1Type, uint8_t chAlt1Deg, uint8_t chAlt2Type, uint8_t chAlt2Deg,
            uint8_t chAlt3Type, uint8_t chAlt3Deg, uint8_t chAlt4Type, uint8_t chAlt4Deg, uint8_t baShrpFlt, uint8_t baTonic,
            uint8_t baType, uint8_t baAlt1Type, uint8_t baAlt1Deg, uint8_t baAlt2Type, uint8_t baAlt2Deg)> flexChord = nullptr;
    std::function<void(struct umpData mess, uint8_t addrs, uint8_t channel)> flexPerformance = nullptr;
    std::function<void(struct umpData mess, uint8_t addrs, uint8_t channel)> flexLyric = nullptr;

    // Message Type 0xF  callbacks
    std::function<void(uint8_t majVer, uint8_t minVer, uint8_t filter)> midiEndpoint = nullptr;
    std::function<void(uint8_t fbIdx, uint8_t filter)> functionBlock = nullptr;
    std::function<void(uint8_t majVer, uint8_t minVer, uint8_t numFuncBlocks, bool m2, bool m1, bool rxjr, bool txjr)>
        midiEndpointInfo = nullptr;
    std::function<void(std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                             std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version)> midiEndpointDeviceInfo = nullptr;
    std::function<void(struct umpData mess)> midiEndpointName = nullptr;
    std::function<void(struct umpData mess)> midiEndpointProdId = nullptr;

    std::function<void(uint8_t protocol, bool jrrx, bool jrtx)> midiEndpointJRProtocolReq = nullptr;
    std::function<void(uint8_t protocol, bool jrrx, bool jrtx)> midiEndpointJRProtocolNotify = nullptr;

    std::function<void(uint8_t fbIdx, bool active,
            uint8_t direction, bool sender, bool recv, uint8_t firstGroup, uint8_t groupLength,
            uint8_t midiCIVersion, uint8_t isMIDI1, uint8_t maxS8Streams)> functionBlockInfo = nullptr;
    std::function<void(struct umpData mess, uint8_t fbIdx)> functionBlockName = nullptr;
    std::function<void()> startOfSeq = nullptr;
    std::function<void()> endOfFile = nullptr;
    
  public:

	void clearUMP();
    void processUMP(uint32_t UMP);

		//-----------------------Handlers ---------------------------
    inline void setUtility(std::function<void(struct umpGeneric mess)> fptr){ utilityMessage = fptr; }
    inline void setCVM(std::function<void(struct umpCVM mess)> fptr ){ channelVoiceMessage = fptr; }
    inline void setSystem(std::function<void(struct umpGeneric mess)> fptr) { systemMessage = fptr; }
    inline void setSysEx(std::function<void(struct umpData mess)> fptr ){sendOutSysex = fptr; }

    //---------- Flex Data
    inline void setFlexTempo(std::function<void(uint8_t group, uint32_t num10nsPQN)> fptr ){ flexTempo = fptr; }
    inline void setFlexTimeSig(std::function<void(uint8_t group, uint8_t numerator, uint8_t denominator, uint8_t num32Notes)> fptr){
        flexTimeSig = fptr; }
    inline void setFlexMetronome(std::function<void(uint8_t group, uint8_t numClkpPriCli, uint8_t bAccP1, uint8_t bAccP2, uint8_t bAccP3,
                          uint8_t numSubDivCli1, uint8_t numSubDivCli2)> fptr){ flexMetronome = fptr; }
    inline void setFlexKeySig(std::function<void(uint8_t group, uint8_t addrs, uint8_t channel, uint8_t sharpFlats, uint8_t tonic)> fptr){
        flexKeySig = fptr; }
    inline void setFlexChord(std::function<void(uint8_t group, uint8_t addrs, uint8_t channel, uint8_t chShrpFlt, uint8_t chTonic,
                      uint8_t chType, uint8_t chAlt1Type, uint8_t chAlt1Deg, uint8_t chAlt2Type, uint8_t chAlt2Deg,
                      uint8_t chAlt3Type, uint8_t chAlt3Deg, uint8_t chAlt4Type, uint8_t chAlt4Deg, uint8_t baShrpFlt, uint8_t baTonic,
                      uint8_t baType, uint8_t baAlt1Type, uint8_t baAlt1Deg, uint8_t baAlt2Type, uint8_t baAlt2Deg)> fptr){
        flexChord = fptr; }
    inline void setFlexPerformance(std::function<void(struct umpData mess, uint8_t addrs, uint8_t channel)> fptr){ flexPerformance = fptr; }
    inline void setFlexLyric(std::function<void(struct umpData mess, uint8_t addrs, uint8_t channel)> fptr){ flexLyric = fptr; }

    //---------- UMP Stream

	inline void setMidiEndpoint(std::function<void(uint8_t majVer, uint8_t minVer, uint8_t filter)> fptr){
        midiEndpoint = fptr; }
	inline void setMidiEndpointNameNotify(std::function<void(struct umpData mess)> fptr){
        midiEndpointName = fptr; }
    inline void setMidiEndpointProdIdNotify(std::function<void(struct umpData mess)> fptr){
        midiEndpointProdId = fptr; }
	inline void setMidiEndpointInfoNotify(std::function<void(uint8_t majVer, uint8_t minVer, uint8_t numOfFuncBlocks, bool m2,
            bool m1, bool rxjr, bool txjr)> fptr){
        midiEndpointInfo = fptr; }
    inline void setMidiEndpointDeviceInfoNotify(std::function<void(std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
            std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version)> fptr){
        midiEndpointDeviceInfo = fptr; }
    inline void setJRProtocolRequest(std::function<void(uint8_t protocol, bool jrrx, bool jrtx)> fptr){ midiEndpointJRProtocolReq = fptr;}
    inline void setJRProtocolNotify(std::function<void(uint8_t protocol, bool jrrx, bool jrtx)> fptr){
        midiEndpointJRProtocolNotify = fptr;}

    inline void setFunctionBlock(std::function<void(uint8_t filter, uint8_t fbIdx)> fptr){ functionBlock = fptr; }
    inline void setFunctionBlockNotify(std::function<void(uint8_t fbIdx, bool active,
                            uint8_t direction, bool sender, bool recv, uint8_t firstGroup, uint8_t groupLength,
                            uint8_t midiCIVersion, uint8_t isMIDI1, uint8_t maxS8Streams)> fptr){
        functionBlockInfo = fptr; }
    inline void setFunctionBlockNameNotify(std::function<void(struct umpData mess, uint8_t fbIdx)> fptr){functionBlockName = fptr; }
    inline void setStartOfSeq(std::function<void()> fptr){startOfSeq = fptr; }
    inline void setEndOfFile(std::function<void()> fptr){endOfFile = fptr; }



};

#endif //UMP_PROCESSOR_H
