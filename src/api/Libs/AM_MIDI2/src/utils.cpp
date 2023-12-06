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

// power of 2, pow(exp)
uint32_t power_of_2(uint8_t exp)
{
    return 1 << exp; // implement integer power of 2 using bit shift
}


uint32_t M2Utils::scaleUp(uint32_t srcVal, uint8_t srcBits, uint8_t dstBits){
   //Handle value of 0 - skip processing
    if(srcVal == 0){
        return 0L;
    }

    //handle 1-bit (bool) scaling
    if(srcBits == 1){
        return power_of_2(dstBits) - 1L;
    }

    // simple bit shift
	uint8_t scaleBits = (dstBits - srcBits);
	uint32_t bitShiftedValue = (srcVal + 0L) << scaleBits;
	auto srcCenter = power_of_2((srcBits-1));
	if (srcVal <= srcCenter ) {
		return bitShiftedValue;
	}

	// expanded bit repeat scheme
	uint8_t repeatBits = srcBits - 1;
	auto repeatMask = power_of_2(repeatBits) - 1;
	uint32_t repeatValue = srcVal & repeatMask;
	if (scaleBits > repeatBits) {
		repeatValue <<= scaleBits - repeatBits;
	} else {
		repeatValue >>= repeatBits - scaleBits;
	}

	while (repeatValue != 0) {
		bitShiftedValue |= repeatValue;
		repeatValue >>= repeatBits;
	}
	return bitShiftedValue;
}

uint32_t M2Utils::scaleDown(uint32_t srcVal, uint8_t srcBits, uint8_t dstBits) {
	// simple bit shift
	uint8_t scaleBits = (srcBits - dstBits);
	return srcVal >> scaleBits;
}



