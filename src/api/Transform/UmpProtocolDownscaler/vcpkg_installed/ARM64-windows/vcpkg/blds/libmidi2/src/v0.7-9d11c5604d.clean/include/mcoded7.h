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

#ifndef MC7_H
#define MC7_H

#include <cstdint>

class mcoded7Decode{
	private:
		uint8_t dumpPos=255;
		uint8_t fBit=0;
		uint8_t cnt=0;
		uint8_t bits=0;
	public:
        uint8_t dump[7];
        uint16_t currentPos();
		void reset();
		void parseS7Byte(uint8_t s7Byte);
};


class mcoded7Encode{

	private:
		uint16_t dumpPos = 1;
		uint8_t cnt = 6;
	public:
        uint8_t dump[8];
        uint16_t currentPos();
        void reset();
		void parseByte(uint8_t s8Byte);
};

#endif

