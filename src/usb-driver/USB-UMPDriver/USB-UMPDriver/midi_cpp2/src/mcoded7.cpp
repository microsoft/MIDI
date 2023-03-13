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

#include "../include/mcoded7.h"

uint16_t mcoded7Decode::currentPos(){ return dumpPos;}

void mcoded7Decode::reset(){
    memset(dump,0,7);
    fBit=0; bits=0;dumpPos=255;
}

void mcoded7Decode::parseS7Byte(uint8_t s7Byte){
    if (dumpPos == 255) {
        reset();
        bits = s7Byte;
        dumpPos=0;
    } else {
        fBit = ((bits >> (6 - dumpPos)) & 1) << 7;
        dump[dumpPos++] = s7Byte | fBit;
    }
}


uint16_t mcoded7Encode::currentPos(){ return dumpPos-1;}

void mcoded7Encode::reset(){
    memset(dump,0,8);
    dumpPos=1; cnt = 6;
}

void mcoded7Encode::parseByte(uint8_t s8Byte){
    uint8_t c = s8Byte & 0x7F;
    uint8_t msb = s8Byte >> 7;
    dump[0] |= msb << cnt;
    dump[dumpPos] = c;
    if (cnt == 0) {
        cnt = 6;
    } else {
        cnt--;
    }
    dumpPos++;
}