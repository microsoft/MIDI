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

#ifndef BSUMP_H
#define BSUMP_H
#include <cstdint>

#define BSTOUMP_BUFFER 4

#include "utils.h"

class bytestreamToUMP{

	private:
		uint8_t d0;
		uint8_t d1;
		
		uint8_t sysex7State = 0;
		uint8_t sysex7Pos = 0;
		
		uint8_t sysex[6] = {0,0,0,0,0,0};
	    uint32_t umpMess[BSTOUMP_BUFFER] = {0,0,0,0};
	    
	    //Channel Based Data
		uint8_t bankMSB[16];
		uint8_t bankLSB[16];
		bool rpnMode[16];
		uint8_t rpnMsbValue[16];
		uint8_t rpnMsb[16];
		uint8_t rpnLsb[16];
		int readIndex = 0;
		int writeIndex = 0;
		int bufferLength = 0;
	    	
		void bsToUMP(uint8_t b0, uint8_t b1, uint8_t b2){
		  uint8_t status = b0 & 0xF0;

		   if(b0 >= TIMING_CODE){
			  umpMess[writeIndex] = ((UMP_SYSTEM << 4) + defaultGroup + 0L) << 24;
			  umpMess[writeIndex] +=  (b0 + 0L) << 16;
			  umpMess[writeIndex] +=  b1  << 8;
			  umpMess[writeIndex] +=  b2;
		   	  increaseWrite();
		   }else if(status>=NOTE_OFF && status<=PITCH_BEND){
			  umpMess[writeIndex] = ((UMP_M1CVM << 4) + defaultGroup + 0L) << 24;
			  umpMess[writeIndex] +=  (b0 + 0L) << 16;
			  umpMess[writeIndex] +=  b1  << 8;
			  umpMess[writeIndex] +=  b2;
		   	  increaseWrite();
		  }
		}

		void increaseWrite(){
			bufferLength++;
			writeIndex++;
			if (writeIndex == BSTOUMP_BUFFER) {
				writeIndex = 0;
			}
		}

	public:
		uint8_t defaultGroup = 0;
		
		bytestreamToUMP(){
			clearAll();
		}
		
		bool availableUMP(){
			return bufferLength;
		}

        void clearAll(){
            using M2Utils::clear;
            clear(bankMSB, 255, sizeof(bankMSB));
            clear(bankLSB, 255, sizeof(bankLSB));
            clear(rpnMsbValue, 255, sizeof(rpnMsbValue));
            clear(rpnMsb, 255, sizeof(rpnMsb));
            clear(rpnLsb, 255, sizeof(rpnLsb));
        }

        void resetBuffer(){
            d1 = 255;
            for (int i = 0; i < BSTOUMP_BUFFER; i++) {
                umpMess[i] = 0;
            }
            readIndex = 0;
            writeIndex = 0;
            bufferLength = 0;
        }
		
		uint32_t readUMP(){
			uint32_t mess = umpMess[readIndex];
			bufferLength--;	 //	Decrease buffer size after reading
			readIndex++;
			if (readIndex == BSTOUMP_BUFFER) {
				readIndex = 0;
			}
			return mess;
		}
		
		void bytestreamParse(uint8_t midi1Byte){
			if (midi1Byte == TUNEREQUEST
                || midi1Byte ==  TIMINGCLOCK
                || midi1Byte ==  SEQSTART
                || midi1Byte ==  SEQCONT
                || midi1Byte ==  SEQSTOP
                || midi1Byte ==  ACTIVESENSE
                || midi1Byte ==  SYSTEMRESET
                ) {
				bsToUMP(midi1Byte,0,0);
				return;
			}

			if (midi1Byte & NOTE_OFF) { // Status byte received
				d0 = midi1Byte;
				d1 = 255;
				if (midi1Byte == SYSEX_START){
					sysex7State = 1;
					sysex7Pos = 0;
				}
                else if (midi1Byte == SYSEX_STOP){
                    umpMess[writeIndex] = ((UMP_SYSEX7 << 4) + defaultGroup + 0L) << 24;
                    umpMess[writeIndex] +=  ((sysex7State == 1?0:3) + 0L) << 20;
                    umpMess[writeIndex] +=  ((sysex7Pos + 0L) << 16) ;
                    umpMess[writeIndex] += (sysex[0] << 8) + sysex[1];
                    increaseWrite();
                    umpMess[writeIndex] = ((sysex[2] + 0L) << 24) + ((sysex[3] + 0L)<< 16) + (sysex[4] << 8) + sysex[5];
                    increaseWrite();
                    sysex7State = 0;
                    M2Utils::clear(sysex, 0, sizeof(sysex));
                }
			} else if(sysex7State >= 1){
				if(sysex7Pos%6 == 0 && sysex7Pos !=0){
                    umpMess[writeIndex] = ((UMP_SYSEX7 << 4) + defaultGroup + 0L) << 24;
					umpMess[writeIndex] +=  (sysex7State + 0L) << 20;
					umpMess[writeIndex] +=  6L << 16;
					umpMess[writeIndex] += (sysex[0] << 8) + sysex[1];
					increaseWrite();
					umpMess[writeIndex] = ((sysex[2] + 0L) << 24) + ((sysex[3] + 0L)<< 16) + (sysex[4] << 8) + sysex[5] + 0L;
					increaseWrite();
					M2Utils::clear(sysex, 0, sizeof(sysex));
					sysex7State=2;
					sysex7Pos=0;
				}
                sysex[sysex7Pos++] = midi1Byte;
			}
            else if (d1 != 255) { // Second byte
                    bsToUMP(d0, d1, midi1Byte);
                    d1 = 255;
            }
            else if (d0){ // status byte set
                if (
                        (d0 & 0xF0) == PROGRAM_CHANGE
                        || (d0 & 0xF0) == CHANNEL_PRESSURE
                        || d0 == TIMING_CODE
                        || d0 == SONG_SELECT
                        ) {
                    bsToUMP(d0, midi1Byte, 0);
                } else if (d0 == 0xF4 || d0 == 0xF5 || d0 == 0xFD || d0==0xF9) {
                    resetBuffer();

                } else if (d0 < SYSEX_START || d0 == SPP) { // First data byte
                    d1=midi1Byte;
                }
            }
		}
};

#endif
