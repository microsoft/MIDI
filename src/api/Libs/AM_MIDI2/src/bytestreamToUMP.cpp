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


#include "../include/utils.h"
#include "../include/bytestreamToUMP.h"

#ifndef clear
#define clear(dest, c,n ) for(uint16_t i = 0 ; i < n ; i ++) dest[i] = c;
#endif

bytestreamToUMP::bytestreamToUMP(){
    clear(bankMSB, 255, sizeof(bankMSB));
    clear(bankLSB, 255, sizeof(bankLSB));
    clear(rpnMsbValue, 255, sizeof(rpnMsbValue));
    clear(rpnMsb, 255, sizeof(rpnMsb));
    clear(rpnLsb, 255, sizeof(rpnLsb));
}
	 
void bytestreamToUMP::bsToUMP(uint8_t b0, uint8_t b1, uint8_t b2){
  uint8_t status = b0 & 0xF0;
 
   if(d0 >= TIMING_CODE){
	  umpMess[messPos] = ((UMP_SYSTEM << 4) + defaultGroup + 0L) << 24;
	  umpMess[messPos] +=  (b0 + 0L) << 16;
	  umpMess[messPos] +=  b1  << 8;
	  umpMess[messPos] +=  b2;
	  messPos++;
  }else if(status>=NOTE_OFF && status<=PITCH_BEND){
	  if(outputMIDI2){
		  uint8_t channel = b0 & 0xF;
		  
		  if(status==NOTE_ON && b2==0){
			 status=NOTE_OFF;
             b2 = 0x40;
		  }
		  
		  umpMess[messPos] = ((UMP_M2CVM << 4) + defaultGroup + 0L) << 24;
		  umpMess[messPos] += (status + channel + 0L)<<16;
		  
		  if(status==NOTE_ON || status==NOTE_OFF){
			umpMess[messPos] += (b1 + 0L) <<8;
                        umpMess[messPos+1] = (M2Utils::scaleUp(b2,7,16) << 16);
		  } else if (status == KEY_PRESSURE){
			umpMess[messPos] += (b1 + 0L) <<8;
                        umpMess[messPos+1] = M2Utils::scaleUp(b2,7,32);
		  } else if (status == PITCH_BEND){
                        umpMess[messPos+1] = M2Utils::scaleUp((b1<<7) + b2,14,32);
		  } else if (status == PROGRAM_CHANGE){
			if(bankMSB[channel]!=255 && bankLSB[channel]!=255){
				umpMess[messPos] += 1;
				umpMess[messPos+1] += (bankMSB[channel] <<8) + bankLSB[channel ];
			}
			umpMess[messPos+1] += (b2 + 0L) << 24;
		  } else if (status == CHANNEL_PRESSURE){
                        umpMess[messPos+1] = M2Utils::scaleUp(b1,7,32);
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
					
					umpMess[messPos] = ((UMP_M2CVM << 4) + defaultGroup + 0L) << 24;
					umpMess[messPos] += (status + channel + 0L)<<16;
					umpMess[messPos] += ((int)rpnMsb[channel]<<7) + rpnLsb[channel] + 0L;
                                    umpMess[messPos+1] = M2Utils::scaleUp(((int)b2<<7),14,32);

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
				
				umpMess[messPos] = ((UMP_M2CVM << 4) + defaultGroup + 0L) << 24;
				umpMess[messPos] += (status  + channel + 0L)<<16;
				umpMess[messPos] += ((int)rpnMsb[channel]<<7) + rpnLsb[channel] + 0L;
                                umpMess[messPos+1] = M2Utils::scaleUp(((int)rpnMsbValue[channel]<<7) + b2,14,32);
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
				umpMess[messPos] += (b1 + 0L) <<8;
                                umpMess[messPos+1] = M2Utils::scaleUp(b2,7,32);
				break;
			}					
		  }
		  messPos+=2;
		  
	  }	else {
		  umpMess[messPos] = ((UMP_M1CVM << 4) + defaultGroup + 0L) << 24;        
		  umpMess[messPos] +=  (b0 + 0L) << 16;
		  umpMess[messPos] +=  b1  << 8;
		  umpMess[messPos] +=  b2;
		  
		  messPos++;
	  }
  }
  
}


bool bytestreamToUMP::availableUMP(){
	return messPos;
}

uint32_t bytestreamToUMP::readUMP(){
	uint32_t mess = umpMess[0];			
	for(uint8_t i=0;i<messPos;i++){
		umpMess[i]=umpMess[i+1];
	}
	messPos--;			
	
	return mess;
}


void bytestreamToUMP::bytestreamParse(uint8_t midi1Byte){
		  
  if (midi1Byte == TUNEREQUEST || midi1Byte >=  TIMINGCLOCK) { 
	  bsToUMP(midi1Byte,0,0);
	  return;
  }
  
  if (midi1Byte & NOTE_OFF) { // Status byte received
	d0 = midi1Byte;
	d1 = 255;
	
	if (midi1Byte == SYSEX_START){
		sysex7State = 1;
		sysex7Pos = 0;
	}else
	if (midi1Byte == SYSEX_STOP){
	  
		umpMess[messPos] = ((UMP_SYSEX7 << 4) + defaultGroup + 0L) << 24;
		umpMess[messPos] +=  ((sysex7State == 1?0:3) + 0L) << 20;
		umpMess[messPos] +=  ((sysex7Pos + 0L) << 16) ;
		umpMess[messPos++] += (sysex[0] << 8) + sysex[1];
		umpMess[messPos++] = ((sysex[2] + 0L) << 24) + ((sysex[3] + 0L)<< 16) + (sysex[4] << 8) + sysex[5];

		sysex7State = 0;
        clear(sysex, 0, sizeof(sysex));
	}
  } else 
  if(sysex7State >= 1){
	  //Check IF new UMP Message Type 3
	  if(sysex7Pos%6 == 0 && sysex7Pos !=0){
		 umpMess[messPos] = ((UMP_SYSEX7 << 4) + defaultGroup + 0L) << 24;
		 umpMess[messPos] +=  (sysex7State + 0L) << 20;
		 umpMess[messPos] +=  6L << 16;
		 umpMess[messPos++] += (sysex[0] << 8) + sysex[1];
		 umpMess[messPos++] = ((sysex[2] + 0L) << 24) + ((sysex[3] + 0L)<< 16) + (sysex[4] << 8) + sysex[5] + 0L;
         clear(sysex, 0, sizeof(sysex));
		 sysex7State=2;
		 sysex7Pos=0;
	  }
	  
	  sysex[sysex7Pos] = midi1Byte;
	  sysex7Pos++;
  } else
  if (d1 != 255) { // Second byte
      bsToUMP(d0, d1, midi1Byte);
		d1 = 255;
  } else if (d0){ // status byte set
	  if (
		d0 == PROGRAM_CHANGE
		|| d0 == TIMING_CODE
		|| d0 == CHANNEL_PRESSURE
		|| d0 == SONG_SELECT
	  ) { 
          bsToUMP(d0, midi1Byte, 0);
		  return;
	  } else 
	  if (d0 < SYSEX_START || d0 == SPP) { // First data byte
		d1=midi1Byte;
		return;
	  }
  }  
}

