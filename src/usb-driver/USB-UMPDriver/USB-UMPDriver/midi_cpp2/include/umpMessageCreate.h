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

#ifndef MESSAGE_CREATE_H
#define MESSAGE_CREATE_H
#include <array>
#include <vector>
#include <cstdint>

namespace UMPMessage {
    uint32_t mt0NOOP();

    uint32_t mt0JRClock(uint16_t clockTime);

    uint32_t mt0JRTimeStamp(uint16_t timestamp);

    uint32_t mt0DeltaClockTick(uint16_t ticksPerQtrNote);

    uint32_t mt0DeltaTicksSinceLast(uint16_t noTicksSince);


    uint32_t mt1MTC(uint8_t group, uint8_t timeCode);

    uint32_t mt1SPP(uint8_t group, uint16_t position);

    uint32_t mt1SongSelect(uint8_t group, uint8_t song);

    uint32_t mt1TuneRequest(uint8_t group);

    uint32_t mt1TimingClock(uint8_t group);

    uint32_t mt1SeqStart(uint8_t group);

    uint32_t mt1SeqCont(uint8_t group);

    uint32_t mt1SeqStop(uint8_t group);

    uint32_t mt1ActiveSense(uint8_t group);

    uint32_t mt1SystemReset(uint8_t group);


    uint32_t mt2NoteOn(uint8_t group, uint8_t channel, uint8_t noteNumber, uint8_t velocity);

    uint32_t mt2NoteOff(uint8_t group, uint8_t channel, uint8_t noteNumber, uint8_t velocity);

    uint32_t mt2PolyPressure(uint8_t group, uint8_t channel, uint8_t noteNumber, uint8_t pressure);

    uint32_t mt2CC(uint8_t group, uint8_t channel, uint8_t index, uint8_t value);

    uint32_t mt2ProgramChange(uint8_t group, uint8_t channel, uint8_t program);

    uint32_t mt2ChannelPressure(uint8_t group, uint8_t channel, uint8_t pressure);

    uint32_t mt2PitchBend(uint8_t group, uint8_t channel, uint16_t value);

    std::array<uint32_t, 2> mt3Sysex7(uint8_t group, uint8_t status, uint8_t numBytes, std::array<uint8_t, 6> sx);


    std::array<uint32_t, 2> mt4NoteOn(uint8_t group, uint8_t channel, uint8_t noteNumber, uint16_t velocity,
                                      uint8_t attributeType, uint16_t attributeData);

    std::array<uint32_t, 2> mt4NoteOff(uint8_t group, uint8_t channel, uint8_t noteNumber, uint16_t velocity,
                                       uint8_t attributeType, uint16_t attributeData);

    std::array<uint32_t, 2> mt4CPolyPressure(uint8_t group, uint8_t channel, uint8_t noteNumber, uint32_t pressure);

    std::array<uint32_t, 2> mt4PitchBend(uint8_t group, uint8_t channel, uint32_t pitch);

    std::array<uint32_t, 2> mt4CC(uint8_t group, uint8_t channel, uint8_t index, uint32_t value);

    std::array<uint32_t, 2> mt4RPN(uint8_t group, uint8_t channel, uint8_t bank, uint8_t index, uint32_t value);

    std::array<uint32_t, 2> mt4NRPN(uint8_t group, uint8_t channel, uint8_t bank, uint8_t index, uint32_t value);

    std::array<uint32_t, 2> mt4RelativeRPN(uint8_t group, uint8_t channel, uint8_t bank, uint8_t index, int32_t value);

    std::array<uint32_t, 2> mt4RelativeNRPN(uint8_t group, uint8_t channel, uint8_t bank, uint8_t index, int32_t value);

    std::array<uint32_t, 2> mt4ChannelPressure(uint8_t group, uint8_t channel, uint32_t pressure);

    std::array<uint32_t, 2> mt4ProgramChange(uint8_t group, uint8_t channel, uint8_t program, bool bankValid,
                                             uint8_t bank, uint8_t index);


    std::array<uint32_t, 4> mtFMidiEndpoint(uint8_t filter);

    std::array<uint32_t, 4> mtFMidiEndpointInfoNotify(uint8_t numOfFuncBlock, bool m2, bool m1, bool rxjr, bool txjr);

    std::array<uint32_t, 4>
    mtFMidiEndpointDeviceInfoNotify(std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                                    std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version);

    std::array<uint32_t, 4>
    mtFMidiEndpointTextNotify(uint16_t replyType, uint8_t offset, uint8_t *text, uint8_t textLen);

    std::array<uint32_t, 4> mtFFunctionBlock(uint8_t fbIdx, uint8_t filter);

    std::array<uint32_t, 4>
    mtFFunctionBlockInfoNotify(uint8_t fbIdx, bool active, uint8_t direction, bool sender, bool recv,
                               uint8_t firstGroup, uint8_t groupLength,
                               uint8_t midiCISupport, uint8_t isMIDI1, uint8_t maxS8Streams);

    std::array<uint32_t, 4> mtFFunctionBlockNameNotify(uint8_t fbIdx, uint8_t offset, uint8_t *text, uint8_t textLen);

    std::array<uint32_t, 4> mtFStartOfSeq();

    std::array<uint32_t, 4> mtFEndOfFile();

    std::array<uint32_t, 4> mtFRequestProtocol(uint8_t protocol, bool jrrx, bool jrtx);

    std::array<uint32_t, 4> mtFNotifyProtocol(uint8_t protocol, bool jrrx, bool jrtx);

}
#endif

