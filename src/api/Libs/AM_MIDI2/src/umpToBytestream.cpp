/**********************************************************
 * MIDI 2.0 Library
 * Author: Andrew Mee
 *
 * MIT License
 * Copyright 2022 Andrew Mee
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


#include "../include/utils.h"
#include "../include/umpToBytestream.h"

umpToBytestream::umpToBytestream(){}

bool umpToBytestream::availableBS(){
    return bsOutLength;
}

uint8_t umpToBytestream::readBS(){
    uint8_t mess = bsOut[0];
    for(uint8_t i=0;i<bsOutLength;i++){
        bsOut[i]=bsOut[i+1];
    }
    bsOutLength--;
    return mess;
}

void umpToBytestream::UMPStreamParse(uint32_t UMP){
    uint8_t byPos = 0;
    switch(UMPPos){
        case 0: { //First UMP Packet
            //First part of a UMP Message
            mType = UMP >> 28;
            group = UMP >> 24 & 0xF;
            switch (mType) {
                case UMP_UTILITY: //32 bits Utility Messages
                case 0x6: //32 Reserved
                case 0x7: //32 Reserved
                    return;
                    break;
                case UMP_SYSTEM: { //32 bits System Real Time and System Common Messages (except System Exclusive)
                    bsOutLength = 1;
                    bsOut[0] = UMP >> 16 & 0xFF;
                    if (bsOut[0] == 0xF1 || bsOut[0] == 0xF2 || bsOut[0] == 0xF3) {
                        bsOut[1] = UMP >> 8 & 0x7F;
                        bsOutLength++;
                    }
                    if (bsOut[0] == 0xF2) {
                        bsOut[2] = UMP & 0x7F;
                        bsOutLength++;
                    }
                    return;
                    break;
                }
                case UMP_M1CVM: {//32 Bits MIDI 1.0 Channel Voice Messages
                    bsOutLength = 2;
                    bsOut[0] = UMP >> 16 & 0xFF;
                    bsOut[1] = UMP >> 8 & 0x7F;
                    if (bsOut[0] >> 4 != 0xC && bsOut[0] >> 4 != 0xD) {
                        bsOut[2] = UMP & 0x7F;
                        bsOutLength++;
                    }
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
                    uint8_t status = (ump64word1 >> 20) & 0xF;
                    uint8_t numSysexbytes = (ump64word1 >> 16) & 0xF;

                    bsOutLength = 0;


                    if (status <= 1) {
                        bsOut[byPos++] = SYSEX_START;
                        bsOutLength++;
                    }
                    if (numSysexbytes > 0) {
                        bsOut[byPos++] = ump64word1 >> 8 & 0x7F;
                        bsOutLength++;
                    }
                    if (numSysexbytes > 1) {
                        bsOut[byPos++] = ump64word1 & 0x7F;
                        bsOutLength++;
                    }
                    if (numSysexbytes > 2) {
                        bsOut[byPos++] = UMP >> 24 & 0x7F;
                        bsOutLength++;
                    }
                    if (numSysexbytes > 3) {
                        bsOut[byPos++] = UMP >> 16 & 0x7F;
                        bsOutLength++;
                    }
                    if (numSysexbytes > 4) {
                        bsOut[byPos++] = UMP >> 8 & 0x7F;
                        bsOutLength++;
                    }
                    if (numSysexbytes > 5) {
                        bsOut[byPos++] = UMP & 0x7F;
                        bsOutLength++;
                    }
                    if (status == 0 || status == 3) {
                        bsOut[byPos++] = SYSEX_STOP;
                        bsOutLength++;
                    }
                    break;
                }
                case UMP_M2CVM:{
                    UMPPos=0;
                    uint8_t status = ump64word1 >> 16 & 0xF0;
                    uint8_t channel = ump64word1 >> 16 & 0xF;
                    uint8_t val1 = ump64word1 >> 8 & 0xFF;
                    uint8_t val2 = ump64word1 & 0xFF;

                    bsOutLength = 0;

                    switch (status) {
                        case NOTE_OFF: //note off
                        case NOTE_ON: { //note on
                            bsOut[byPos++] = ump64word1 >> 16 & 0xFF;bsOutLength++;
                            bsOut[byPos++] = val1; bsOutLength++;

                            uint8_t velocity = (uint8_t) M2Utils::scaleDown((UMP >> 16), 16, 7);
                            if (velocity == 0 && status == NOTE_ON) {
                                velocity = 1;
                            }
                            bsOut[byPos++] = velocity; bsOutLength++;

                            break;
                        }
                        case KEY_PRESSURE: //poly aftertouch
                        case CC: {//CC
                            bsOut[byPos++] = ump64word1 >> 16 & 0xFF;bsOutLength++;
                            bsOut[byPos++] = val1; bsOutLength++;
                            uint8_t value = (uint8_t)M2Utils::scaleDown(UMP , 32, 7);
                            bsOut[byPos++] = value; bsOutLength++;
                            break;
                        }
                        case CHANNEL_PRESSURE: { //Channel Pressure
                            bsOut[byPos++] = ump64word1 >> 16 & 0xFF;bsOutLength++;
                            uint8_t value = (uint8_t) M2Utils::scaleDown(UMP, 32, 7);
                            bsOut[byPos++] = value; bsOutLength++;
                            break;
                        }
                        case RPN: {//rpn
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 101;bsOutLength++;
                            bsOut[byPos++] = val1;bsOutLength++;
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 100;bsOutLength++;
                            bsOut[byPos++] = val2;bsOutLength++;

                            uint16_t val14bit = (uint16_t)M2Utils::scaleDown(UMP , 32, 14);
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 6;bsOutLength++;
                            bsOut[byPos++] = (val14bit >> 7) & 0x7F;bsOutLength++;
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 38;bsOutLength++;
                            bsOut[byPos++] = val14bit & 0x7F;bsOutLength++;

                            break;
                        }
                        case NRPN: { //nrpn
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 99;bsOutLength++;
                            bsOut[byPos++] = val1;bsOutLength++;
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 98;bsOutLength++;
                            bsOut[byPos++] = val2;bsOutLength++;

                            uint16_t val14bit = (uint16_t)M2Utils::scaleDown(UMP, 32, 14);
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 6;bsOutLength++;
                            bsOut[byPos++] = (val14bit >> 7) & 0x7F;bsOutLength++;
                            bsOut[byPos++] = CC + channel;bsOutLength++;
                            bsOut[byPos++] = 38;bsOutLength++;
                            bsOut[byPos++] = val14bit & 0x7F;bsOutLength++;
                            break;
                        }
                        case PROGRAM_CHANGE: { //Program change
                            if (ump64word1 & 0x1) {
                                bsOut[byPos++] = CC + channel;
                                bsOutLength++;
                                bsOut[byPos++] = 0;
                                bsOutLength++;
                                bsOut[byPos++] = (UMP >> 8) & 0x7F;
                                bsOutLength++;

                                bsOut[byPos++] = CC + channel;
                                bsOutLength++;
                                bsOut[byPos++] = 32;
                                bsOutLength++;
                                bsOut[byPos++] = UMP & 0x7F;
                                bsOutLength++;
                            }
                            bsOut[byPos++] = PROGRAM_CHANGE + channel;
                            bsOutLength++;
                            bsOut[byPos++] = (UMP >> 24) & 0x7F;
                            bsOutLength++;
                            break;
                        }
                        case PITCH_BEND: //Pitch bend
                            bsOut[byPos++] = (ump64word1 >> 16) & 0xFF;bsOutLength++;
                            bsOut[byPos++] = (UMP >> 18) & 0x7F;bsOutLength++;
                            bsOut[byPos++] = (UMP >> 25) & 0x7F;bsOutLength++;
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
