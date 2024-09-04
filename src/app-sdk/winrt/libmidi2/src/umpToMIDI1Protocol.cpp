/**********************************************************
 * MIDI 2.0 Library
 * Author: Andrew Mee
 *
 * MIT License
 * Copyright 2024 Andrew Mee
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


#include "utils.h"
#include "umpToMIDI1Protocol.h"
#include "umpMessageCreate.h"


void umpToMIDI1Protocol::increaseWrite()
{
    bufferLength++;
    writeIndex++;
    if (writeIndex == UMPTOPROTO1_BUFFER) {
        writeIndex = 0;
    }
}

bool umpToMIDI1Protocol::availableUMP(){
    return bufferLength;
}

uint32_t umpToMIDI1Protocol::readUMP(){
    uint32_t mess = umpMess[readIndex];
    bufferLength--; //	Decrease buffer size after reading
    readIndex++;
    if (readIndex == UMPTOPROTO1_BUFFER) {
        readIndex = 0;
    }

    return mess;
}

void umpToMIDI1Protocol::UMPStreamParse(uint32_t UMP){

    switch(UMPPos){
        case 0: { //First UMP Packet
            //First part of a UMP Message
            mType = UMP >> 28;
            switch (mType) {
                case UMP_UTILITY: //32 bits Utility Messages
                case 0x6: //32 Reserved
                case 0x7: //32 Reserved
                    return;
                    break;
                case UMP_M1CVM: //32 Bits MIDI 1.0 Channel Voice Messages
                case UMP_SYSTEM: { //32 bits System Real Time and System Common Messages (except System Exclusive)
                    umpMess[writeIndex] = UMP;increaseWrite();
                    return;
                    break;
                }
                case UMP_SYSEX7: //64 bits Data Messages (including System Exclusive)
                case UMP_M2CVM: //MIDI2.0 Channel Voice Messages
                    ump64word1 = UMP;
                    UMPPos++;
                    break;
                default:
                    UMPPos++;
                    break;
            }
            break;
        }
        case 1: { //64Bit+ Messages only
            switch (mType) {
                case 0x8: //64 Reserved
                case 0x9: //64 Reserved
                case 0xA: //64 Reserved
                    UMPPos=0;
                    break;
                case UMP_SYSEX7: { //64 bits Data Messages (including System Exclusive) part 2
                    UMPPos = 0;
                    umpMess[writeIndex] = ump64word1;
                    increaseWrite();
                    umpMess[writeIndex] = UMP;
                    increaseWrite();
                    break;
                }
                case UMP_M2CVM:{
                    UMPPos=0;
                    uint8_t status = (ump64word1 >> 16) & 0xFF;
                    uint8_t channel = (ump64word1 >> 16) & 0xF;
                    uint8_t val1 = (ump64word1 >> 8) & 0xFF;
                    uint8_t val2 = ump64word1 & 0xFF;
                    uint8_t group = (ump64word1 >> 24) & 0xF;

                    switch (status)
                    {
                    case NOTE_OFF:
                        {
                            //note off
                            uint8_t velocity = (uint8_t)M2Utils::scaleDown((UMP >> 16), 16, 7);
                            umpMess[writeIndex] = UMPMessage::mt2NoteOff(group, channel, val1, velocity);
                            increaseWrite();
                            break;
                        }
                    case NOTE_ON:
                        {
                            //note on
                            uint8_t velocity = (uint8_t)M2Utils::scaleDown((UMP >> 16), 16, 7);
                            if (velocity == 0)
                            {
                                velocity = 1;
                            }
                            umpMess[writeIndex] = UMPMessage::mt2NoteOn(group, channel, val1, velocity);
                            increaseWrite();
                            break;
                        }
                    case KEY_PRESSURE:
                        {
                            //poly aftertouch
                            uint8_t value = (uint8_t)M2Utils::scaleDown(UMP, 32, 7);
                            umpMess[writeIndex] = UMPMessage::mt2PolyPressure(group, channel, val1, value);
                            increaseWrite();
                            break;
                        }
                    case CC:
                        {
                            //CC
                            uint8_t value = (uint8_t)M2Utils::scaleDown(UMP, 32, 7);
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, val1, value);
                            increaseWrite();
                            break;
                        }
                    case CHANNEL_PRESSURE:
                        {
                            //Channel Pressure
                            uint8_t value = (uint8_t)M2Utils::scaleDown(UMP, 32, 7);
                            umpMess[writeIndex] = UMPMessage::mt2ChannelPressure(group, channel, value);
                            increaseWrite();
                            break;
                        }
                    case NRPN:
                        {
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 99, val1);
                            increaseWrite();
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 98, val2);
                            increaseWrite();
                            uint16_t val14bit = (uint16_t)M2Utils::scaleDown(UMP, 32, 14);
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 6, (val14bit >> 7) & 0x7F);
                            increaseWrite();
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 38, val14bit & 0x7F);
                            increaseWrite();
                            break;
                        }
                    case RPN:
                        {
                            //rpn
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 101, val1);
                            increaseWrite();
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 100, val2);
                            increaseWrite();
                            uint16_t val14bit = (uint16_t)M2Utils::scaleDown(UMP, 32, 14);
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 6, (val14bit >> 7) & 0x7F);
                            increaseWrite();
                            umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 38, val14bit & 0x7F);
                            increaseWrite();
                            break;
                        }
                    case PROGRAM_CHANGE:
                        {
                            //Program change
                            if (ump64word1 & 0x1)
                            {
                                umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 0, (UMP >> 8) & 0x7F);
                                increaseWrite();
                                umpMess[writeIndex] = UMPMessage::mt2CC(group, channel, 32, UMP & 0x7F);
                                increaseWrite();
                            }

                            umpMess[writeIndex] = UMPMessage::mt2ProgramChange(group, channel, (UMP >> 24) & 0x7F);
                            increaseWrite();
                            break;
                        }
                    case PITCH_BEND: //Pitch bend
                        umpMess[writeIndex] = UMPMessage::mt2PitchBend(group, channel, UMP >> 18);
                        increaseWrite();
                        break;
                    }
                    break;
                }
            default:
                UMPPos++;
                break;
            }
            break;
        }
        case 2:{
            switch (mType) {
                case 0xB: //96 Reserved
                case 0xC: //96 Reserved
                    UMPPos = 0;
                    break;
                default:
                    UMPPos++;
                    break;
            }
            break;
        }
        case 3:{
            UMPPos = 0;
            break;
        }
    }
}
