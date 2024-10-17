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

#include "utils.h"

#include <cstdint>

class mcoded7Decode
{
private:
	uint8_t dumpPos = 255;
	uint8_t fBit = 0;
	uint8_t cnt = 0;
	uint8_t bits = 0;

public:
	uint8_t dump[7];
	uint16_t currentPos() { return dumpPos; }

	void reset()
	{
		M2Utils::clear(dump, 0, 7);
		fBit = 0;
		bits = 0;
		dumpPos = 255;
	}

	void parseS7Byte(uint8_t s7Byte)
	{
		if (dumpPos == 255)
		{
			reset();
			bits = s7Byte;
			dumpPos = 0;
		}
		else
		{
			fBit = ((bits >> (6 - dumpPos)) & 1) << 7;
			dump[dumpPos++] = s7Byte | fBit;
		}
	}
};


class mcoded7Encode
{
private:
	uint16_t dumpPos = 1;
	uint8_t cnt = 6;

public:
	uint8_t dump[8];
	uint16_t currentPos() { return dumpPos - 1; }

	void reset()
	{
		M2Utils::clear(dump, 0, 8);
		dumpPos = 1;
		cnt = 6;
	}

	void parseByte(uint8_t s8Byte)
	{
		uint8_t c = s8Byte & 0x7F;
		uint8_t msb = s8Byte >> 7;
		dump[0] |= msb << cnt;
		dump[dumpPos] = c;
		if (cnt == 0)
		{
			cnt = 6;
		}
		else
		{
			cnt--;
		}
		dumpPos++;
	}
};

#endif

