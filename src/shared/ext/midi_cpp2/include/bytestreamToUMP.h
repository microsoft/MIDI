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

class bytestreamToUMP{

	private:
		uint8_t d0;
		uint8_t d1;
		
		uint8_t sysex7State = 0;
		uint8_t sysex7Pos = 0;
		
		uint8_t sysex[6] = {0,0,0,0,0,0};
	    uint8_t messPos=0;
	    uint32_t umpMess[4];
	    
	    //Channel Based Data
		uint8_t bankMSB[16];
		uint8_t bankLSB[16];
		bool rpnMode[16];
		uint8_t rpnMsbValue[16];
		uint8_t rpnMsb[16];
		uint8_t rpnLsb[16];
	    	
		void bsToUMP(uint8_t b0, uint8_t b1, uint8_t b2);


	public:
		uint8_t defaultGroup = 0;
		bool outputMIDI2 = false;
		
   		bytestreamToUMP();
		
		bool availableUMP();
		
		uint32_t readUMP();
		
		void bytestreamParse(uint8_t midi1Byte);

	
};

#endif
