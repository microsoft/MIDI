// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>

#include <WexTestClass.h>

#include "MidiTestCommon.h"

UMP32 g_MidiTestData_32 = {0x21AA1234 }; // translates to midi 1 message 0xAA 0x12 0x34
UMP64 g_MidiTestData_64 = {0x40917000, 0x48000000 }; // note on message that translates to g_MidiTestMessage
UMP96 g_MidiTestData_96 = {0xb1CC1234, 0xbaadf00d, 0xdeadbeef }; // does not translate to midi 1
UMP128 g_MidiTestData_128 = {0xF1DD1234, 0xbaadf00d, 0xdeadbeef, 0xd000000d }; // does not translate to midi 1

// note on channel 1, note 0x70, velocity 0x42
MIDI_MESSAGE g_MidiTestMessage = { 0x91, 0x70, 0x42 }; // translates to UMP 0x40917000, 0x48000000

_Use_decl_annotations_
void PrintMidiMessage(PVOID Payload, UINT32 PayloadSize, UINT32 ExpectedPayloadSize, LONGLONG PayloadPosition)
{
    if (PayloadSize < ExpectedPayloadSize)
    {
        LOG_OUTPUT(L"INCOMING: PayloadSize %d < ExpectedPayloadSize %d", PayloadSize, ExpectedPayloadSize);
    }
    
    LOG_OUTPUT(L"Payload position is %I64d", PayloadPosition);

    if (PayloadSize == sizeof(MIDI_MESSAGE))
    {
        // if it's bytestream, print it
        MIDI_MESSAGE *data = (MIDI_MESSAGE *) Payload;
        LOG_OUTPUT(L"INCOMING: status 0x%hx data1 0x%hx data2 0x%hx - size 0x%08x", data->status, data->data1, data->data2, PayloadSize);
    }
    else if (PayloadSize >= sizeof(UMP32))
    {
        // if it's UMP
        UMP128 *data = (UMP128 *) Payload;
        LOG_OUTPUT(L"INCOMING: data 1 0x%08x - size 0x%08x", data->data1, PayloadSize);
        if (PayloadSize >= sizeof(UMP64))
        {
            LOG_OUTPUT(L"INCOMING: data 2 0x%08x", data->data2);
            if (PayloadSize >= sizeof(UMP96))
            {
                LOG_OUTPUT(L"INCOMING: data 3 0x%08x", data->data3);
                if (PayloadSize >= sizeof(UMP128))
                {
                    LOG_OUTPUT(L"INCOMING: data 4 0x%08x", data->data4);
                    if (PayloadSize > sizeof(UMP128))
                    {
                        LOG_OUTPUT(L"INCOMING: Buffer contains additional 0x%08x bytes of data", (UINT32) (PayloadSize - sizeof(UMP128)));
                    }
                }
            }
        }
    }
}

