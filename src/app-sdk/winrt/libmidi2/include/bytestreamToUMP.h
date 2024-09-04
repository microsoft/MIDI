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

//#include <cstdio>

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
			  if(outputMIDI2){
				  uint8_t channel = b0 & 0xF;

				  if(status==NOTE_ON && b2==0){
					 status=NOTE_OFF;
		             b2 = 0x40;
				  }

				  umpMess[writeIndex] = ((UMP_M2CVM << 4) + defaultGroup + 0L) << 24;
				  umpMess[writeIndex] += (status + channel + 0L)<<16;

				  if(status==NOTE_ON || status==NOTE_OFF){
					umpMess[writeIndex] += (b1 + 0L) <<8;
		  			increaseWrite();
		  			umpMess[writeIndex] = (M2Utils::scaleUp(b2,7,16) << 16);
		  			increaseWrite();
				  } else if (status == KEY_PRESSURE){
					umpMess[writeIndex] += (b1 + 0L) <<8;
		  			increaseWrite();
		  			umpMess[writeIndex] = (M2Utils::scaleUp(b2,7,16) << 16);
		  			increaseWrite();
				  } else if (status == PITCH_BEND){
		  			increaseWrite();
		  			umpMess[writeIndex] = M2Utils::scaleUp((b2<<7) + b1,14,32);
		  			increaseWrite();
				  } else if (status == PROGRAM_CHANGE){
					if(bankMSB[channel]!=255 && bankLSB[channel]!=255){
						umpMess[writeIndex] += 1;
						increaseWrite();
						umpMess[writeIndex] = (bankMSB[channel] <<8) + bankLSB[channel];
					}else{
						increaseWrite();
						umpMess[writeIndex] = 0;
					}
					umpMess[writeIndex] += (b1 + 0L) << 24;
		  			increaseWrite();
				  } else if (status == CHANNEL_PRESSURE){
		  			increaseWrite();
		              umpMess[writeIndex] = M2Utils::scaleUp(b1,7,32);
		  			increaseWrite();
				  }  else if (status == CC){
					switch(b1){
					 case 0:
						bankMSB[channel] = b2;
						return;
					 case 32:
						bankLSB[channel] = b2;
						return;

					 case 6: //RPN MSB Value
						if(rpnMsb[channel]==255 || rpnLsb[channel]==255){
							return;
						}

						if(rpnMode[channel] && rpnMsb[channel] == 0 && (rpnLsb[channel] == 0 || rpnLsb[channel] == 6)){
							status = rpnMode[channel]? RPN: NRPN;

							umpMess[writeIndex] = ((UMP_M2CVM << 4) + defaultGroup + 0L) << 24;
							umpMess[writeIndex] += (status + channel + 0L)<<16;
							umpMess[writeIndex] += ((int)rpnMsb[channel]<<8) + rpnLsb[channel] + 0L;
							increaseWrite();
							umpMess[writeIndex] = M2Utils::scaleUp(((int)b2<<7),14,32);
							increaseWrite();

						}else{
							rpnMsbValue[channel] = b2;
							return;
						}
						break;
					case 38: //RPN LSB Value
						if(rpnMsb[channel]==255 || rpnLsb[channel]==255){
							return;
						}
						status = rpnMode[channel]? RPN: NRPN;

						umpMess[writeIndex] = ((UMP_M2CVM << 4) + defaultGroup + 0L) << 24;
						umpMess[writeIndex] += (status  + channel + 0L)<<16;
						umpMess[writeIndex] += (rpnMsb[channel]<<8) + rpnLsb[channel] + 0L;
						increaseWrite();
						umpMess[writeIndex] = M2Utils::scaleUp(((int)rpnMsbValue[channel]<<7) + b2,14,32);
						increaseWrite();
						break;
					case 99:
						rpnMode[channel] = false;
						rpnMsb[channel] = b2;
						return;
					case 98:
						rpnMode[channel] = false;
						rpnLsb[channel] = b2;
						return;
					case 101:
						rpnMode[channel] = true;
						rpnMsb[channel] = b2;
						return;

					case 100:
						rpnMode[channel] = true;
						rpnLsb[channel] = b2;
						return;

					default:
						umpMess[writeIndex] += (b1 + 0L) <<8;
						increaseWrite();
						umpMess[writeIndex] = M2Utils::scaleUp(b2,7,32);
						increaseWrite();
						break;
					}
				  }


			  }
   			else {
				  umpMess[writeIndex] = ((UMP_M1CVM << 4) + defaultGroup + 0L) << 24;
				  umpMess[writeIndex] +=  (b0 + 0L) << 16;
				  umpMess[writeIndex] +=  b1  << 8;
				  umpMess[writeIndex] +=  b2;
   				increaseWrite();
		   }
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
		bool outputMIDI2 = false;
		
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

           // printf("\n - new byte: %#02x",midi1Byte);

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
                    //printf(" \nSysex %#02x %#02x %#02x %#02x %#02x %#02x", sysex[0], sysex[1], sysex[2], sysex[3], sysex[4], sysex[5]);

                    umpMess[writeIndex] = ((UMP_SYSEX7 << 4) + defaultGroup + 0L) << 24;
                    umpMess[writeIndex] +=  ((sysex7State == 1?0:3) + 0L) << 20;
                    umpMess[writeIndex] +=  ((sysex7Pos + 0L) << 16) ;
                    umpMess[writeIndex] += (sysex[0] << 8) + sysex[1];
                    increaseWrite();
                    umpMess[writeIndex] = ((sysex[2] + 0L) << 24) + ((sysex[3] + 0L)<< 16) + (sysex[4] << 8) + sysex[5];
                    increaseWrite();

                    sysex7State = 0;
                    M2Utils::clear(sysex, 0, sizeof(sysex));
                    //printf(" \nUMP %#08x %#08x\n", umpMess[writeIndex-2], umpMess[writeIndex-1]);

                }
			} else if(sysex7State >= 1){
				//Check IF new UMP Message Type 3
                //printf(" sysex7Pos %d ",sysex7Pos);

				if(sysex7Pos%6 == 0 && sysex7Pos !=0){
                    //printf(" \nSysex %#02x %#02x %#02x %#02x %#02x %#02x", sysex[0], sysex[1], sysex[2], sysex[3], sysex[4], sysex[5]);
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

                    //printf(" \nUMP %#08x %#08x\n", umpMess[writeIndex-2], umpMess[writeIndex-1]);
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

           // if(debug)printf("\n");
		}
};

#endif
