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

#ifndef UMPBS_H
#define UMPBS_H


#include <cstdint>
#include "utils.h"

#define UMPTOBS_BUFFER 12

class umpToBytestream{

	private:
        uint8_t mType;
        uint32_t ump64word1;

        uint8_t UMPPos=0;
        uint8_t bsOut[UMPTOBS_BUFFER];
		int readIndex = 0;
		int writeIndex = 0;
		int bufferLength = 0;

        uint8_t lastRunningStatus = 255;
        uint8_t lastRPN_MSB[16];
        uint8_t lastRPN_LSB[16];
        uint8_t lastNRPN_MSB[16];
        uint8_t lastNRPN_LSB[16];

		void increaseWrite(){
			bufferLength++;
			writeIndex++;
			if (writeIndex == UMPTOBS_BUFFER) {
				writeIndex = 0;
			}
		}

        void checkRunningStatusAndAddByte(uint8_t stsCh)
		{
		    if(!enableRunningStatus || lastRunningStatus != stsCh)
		    {
		        bsOut[writeIndex] = stsCh;
		        lastRunningStatus = stsCh;
		        increaseWrite();
		    }
		}

	public:
        uint8_t group;

        uint8_t filterByGroup = 255;

        bool enableRunningStatus = false;

        umpToBytestream(){
            resetBuffer();
        }

        void setFilterGroup(uint8_t fGr)
        {
            filterByGroup = fGr;
        }

        void resetBuffer(){
            using M2Utils::clear;
            clear(bsOut,0,UMPTOBS_BUFFER);
            UMPPos=0;
            readIndex = 0;
            writeIndex = 0;
            bufferLength = 0;
            lastRunningStatus = 255;
            for (int ch = 0; ch < 16; ch++) {
                lastRPN_MSB[ch] = 255;
                lastRPN_LSB[ch] = 255;
                lastNRPN_MSB[ch] = 255;
                lastNRPN_LSB[ch] = 255;
            }
        }

		bool availableBS(){
			return bufferLength;
		}

		uint8_t readBS(){
			uint8_t mess = bsOut[readIndex];
			bufferLength--;	 //	Decrease buffer size after reading
			readIndex++;
			if (readIndex == UMPTOBS_BUFFER) {
				readIndex = 0;
			}
			return mess;
		}

        void UMPStreamParse(uint32_t UMP) {
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
                            if(filterByGroup<16 && group != filterByGroup)
                            {
                                return;
                            }

                            uint8_t sysByte = UMP >> 16 & 0xFF;
                            if (
                                sysByte== 0xF4 || sysByte == 0xF5 || sysByte == 0xFD || sysByte == 0xF9
                                || sysByte == 0xF7 || sysByte <= 0xF0
                                ) {
                                return;
                            }

                            bsOut[writeIndex] = sysByte;
                            increaseWrite();
                            if (sysByte== 0xF1 ||sysByte == 0xF2 || sysByte == 0xF3) {
                                bsOut[writeIndex] = (UMP >> 8) & 0x7F;
                                increaseWrite();
                            }
                            if (sysByte == 0xF2) {
                                bsOut[writeIndex] = UMP & 0x7F;
                                increaseWrite();
                            }
                            return;
                            break;
                        }
                        case UMP_M1CVM: {//32 Bits MIDI 1.0 Channel Voice Message
                            if(filterByGroup<16 && group != filterByGroup)
                            {
                                return;
                            }
                            uint8_t stsCh = UMP >> 16 & 0xFF;

                            checkRunningStatusAndAddByte(stsCh);

                            bsOut[writeIndex] = UMP >> 8 & 0x7F;
                            increaseWrite();
                            if (stsCh >> 4 != 0xC && stsCh >> 4 != 0xD) {
                                bsOut[writeIndex] = UMP & 0x7F;
                                increaseWrite();
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
                                if(filterByGroup<16 && group != filterByGroup)
                                {
                                    return;
                                }

                            uint8_t status = (ump64word1 >> 20) & 0xF;
                            uint8_t numSysexbytes = (ump64word1 >> 16) & 0xF;


                            if (status <= 1) {
                                bsOut[writeIndex] = SYSEX_START;
                                increaseWrite();
                            }
                            if (numSysexbytes > 0) {
                                bsOut[writeIndex] = ump64word1 >> 8 & 0x7F;
                                increaseWrite();
                            }
                            if (numSysexbytes > 1) {
                                bsOut[writeIndex] = ump64word1 & 0x7F;
                                increaseWrite();
                            }
                            if (numSysexbytes > 2) {
                                bsOut[writeIndex] = (UMP >> 24) & 0x7F;
                                increaseWrite();
                            }
                            if (numSysexbytes > 3) {
                                bsOut[writeIndex] = (UMP >> 16) & 0x7F;
                                increaseWrite();
                            }
                            if (numSysexbytes > 4) {
                                bsOut[writeIndex] = (UMP >> 8) & 0x7F;
                                increaseWrite();
                            }
                            if (numSysexbytes > 5) {
                                bsOut[writeIndex] = UMP & 0x7F;
                                increaseWrite();
                            }
                            if (status == 0 || status == 3) {
                                bsOut[writeIndex] = SYSEX_STOP;
                                increaseWrite();
                            }
                            break;
                        }
                        case UMP_M2CVM:{
                            UMPPos=0;
                                if(filterByGroup<16 && group != filterByGroup)
                                {
                                    return;
                                }
                            uint8_t status = ump64word1 >> 16 & 0xF0;
                            uint8_t channel = ump64word1 >> 16 & 0xF;
                            uint8_t val1 = ump64word1 >> 8 & 0x7F;
                            uint8_t val2 = ump64word1 & 0x7F;
                            uint8_t stsCh = status + channel;

                            switch (status) {
                                case NOTE_OFF: //note off
                                case NOTE_ON: { //note on
                                    checkRunningStatusAndAddByte(stsCh);

                                    bsOut[writeIndex] = val1; increaseWrite();

                                    uint8_t velocity = (uint8_t) M2Utils::scaleDown((UMP >> 16), 16, 7);
                                    if (velocity == 0 && status == NOTE_ON) {
                                        velocity = 1;
                                    }
                                    bsOut[writeIndex] = velocity; increaseWrite();

                                    break;
                                }
                                case KEY_PRESSURE: //poly aftertouch
                                case CC: {//CC
                                    checkRunningStatusAndAddByte(stsCh);
                                    bsOut[writeIndex] = val1; increaseWrite();
                                    uint8_t value = (uint8_t)M2Utils::scaleDown(UMP , 32, 7);
                                    bsOut[writeIndex] = value; increaseWrite();
                                    break;
                                }
                                case CHANNEL_PRESSURE: { //Channel Pressure
                                        checkRunningStatusAndAddByte(stsCh);
                                    uint8_t value = (uint8_t) M2Utils::scaleDown(UMP, 32, 7);
                                    bsOut[writeIndex] = value; increaseWrite();
                                    break;
                                }
                                case RPN: {//rpn
                                        stsCh = CC + channel;
                                        if(lastRPN_MSB[channel] != val1 || lastRPN_LSB[channel] != val2)
                                        {
                                            checkRunningStatusAndAddByte(stsCh);
                                            bsOut[writeIndex] = 101;increaseWrite();
                                            bsOut[writeIndex] = val1;increaseWrite();
                                            checkRunningStatusAndAddByte(stsCh);
                                            bsOut[writeIndex] = 100;increaseWrite();
                                            bsOut[writeIndex] = val2;increaseWrite();
                                            lastRPN_MSB[channel] = val1;
                                            lastRPN_LSB[channel] = val2;
                                            lastNRPN_MSB[channel] = 255;
                                            lastNRPN_LSB[channel] = 255;
                                        }


                                        uint16_t val14bit = (uint16_t)M2Utils::scaleDown(UMP , 32, 14);

                                        //bsOut[writeIndex] = CC + channel;increaseWrite();
                                        checkRunningStatusAndAddByte(stsCh);
                                        bsOut[writeIndex] = 6;increaseWrite();
                                        bsOut[writeIndex] = (val14bit >> 7) & 0x7F;increaseWrite();
                                        //bsOut[writeIndex] = CC + channel;increaseWrite();
                                        checkRunningStatusAndAddByte(stsCh);
                                        bsOut[writeIndex] = 38;increaseWrite();
                                        bsOut[writeIndex] = val14bit & 0x7F;increaseWrite();

                                        break;
                                    }
                                case NRPN: { //nrpn
                                        stsCh = CC + channel;
                                        if(lastNRPN_MSB[channel] != val1 || lastNRPN_LSB[channel] != val2)
                                        {
                                            checkRunningStatusAndAddByte(stsCh);
                                            bsOut[writeIndex] = 99;increaseWrite();
                                            bsOut[writeIndex] = val1;increaseWrite();
                                            checkRunningStatusAndAddByte(stsCh);
                                            bsOut[writeIndex] = 98;increaseWrite();
                                            bsOut[writeIndex] = val2;increaseWrite();
                                            lastRPN_MSB[channel] = 255;
                                            lastRPN_LSB[channel] = 255;
                                            lastNRPN_MSB[channel] = val1;
                                            lastNRPN_LSB[channel] = val2;
                                        }

                                        uint16_t val14bit = (uint16_t)M2Utils::scaleDown(UMP, 32, 14);

                                        checkRunningStatusAndAddByte(stsCh);
                                        bsOut[writeIndex] = 6;increaseWrite();
                                        bsOut[writeIndex] = (val14bit >> 7) & 0x7F;increaseWrite();
                                        checkRunningStatusAndAddByte(stsCh);
                                        bsOut[writeIndex] = 38;increaseWrite();
                                        bsOut[writeIndex] = val14bit & 0x7F;increaseWrite();
                                        break;
                                    }
                                case PROGRAM_CHANGE: { //Program change
                                    if (ump64word1 & 0x1) {
                                        checkRunningStatusAndAddByte(CC + channel);
                                        bsOut[writeIndex] = 0;
                                        increaseWrite();
                                        bsOut[writeIndex] = (UMP >> 8) & 0x7F;
                                        increaseWrite();

                                        checkRunningStatusAndAddByte(CC + channel);
                                        bsOut[writeIndex] = 32;
                                        increaseWrite();
                                        bsOut[writeIndex] = UMP & 0x7F;
                                        increaseWrite();
                                    }
                                    checkRunningStatusAndAddByte( PROGRAM_CHANGE + channel);
                                    bsOut[writeIndex] = (UMP >> 24) & 0x7F;
                                    increaseWrite();
                                    break;
                                }
                                case PITCH_BEND: //Pitch bend
                                    checkRunningStatusAndAddByte((ump64word1 >> 16) & 0xFF);
                                    bsOut[writeIndex] = (UMP >> 18) & 0x7F;increaseWrite();
                                    bsOut[writeIndex] = (UMP >> 25) & 0x7F;increaseWrite();
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
};

#endif
