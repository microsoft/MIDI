// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>

#include <WexTestClass.h>

#include "MidiTestCommon.h"

// Default assume group 0 for the messages, tests are responsible for changing the
// group number of the test message to align to the ports being used for testing.
UMP32 g_MidiTestData_32 = {0x20AA1234 }; // translates to midi 1 message 0xAA 0x12 0x34
UMP64 g_MidiTestData_64 = {0x40917000, 0x48000000 }; // note on message that translates to g_MidiTestMessage
UMP96 g_MidiTestData_96 = {0xb1CC1234, 0xbaadf00d, 0xdeadbeef }; // does not translate to midi 1
UMP128 g_MidiTestData_128 = {0xF1DD1234, 0xbaadf00d, 0xdeadbeef, 0xd000000d }; // does not translate to midi 1

// note on channel 1, note 0x70, velocity 0x42
MIDI_MESSAGE g_MidiTestMessage = { 0x91, 0x70, 0x42 }; // translates to UMP 0x40917000, 0x48000000

_Use_decl_annotations_
void PrintMidiMessage(PVOID payload, UINT32 payloadSize, UINT32 expectedPayloadSize, LONGLONG payloadPosition)
{
    if (payloadSize < expectedPayloadSize)
    {
        LOG_OUTPUT(L"INCOMING: PayloadSize %d < ExpectedPayloadSize %d", payloadSize, expectedPayloadSize);
    }
    
    LOG_OUTPUT(L"Payload position is %I64d", payloadPosition);

    if (payloadSize == sizeof(MIDI_MESSAGE))
    {
        // if it's bytestream, print it
        MIDI_MESSAGE *data = (MIDI_MESSAGE *) payload;
        LOG_OUTPUT(L"INCOMING: status 0x%hx data1 0x%hx data2 0x%hx - size 0x%08x", data->status, data->data1, data->data2, payloadSize);
    }
    else if (payloadSize >= sizeof(UMP32))
    {
        // if it's UMP
        UMP128 *data = (UMP128 *) payload;
        LOG_OUTPUT(L"INCOMING: data 1 0x%08x - size 0x%08x", data->data1, payloadSize);
        if (payloadSize >= sizeof(UMP64))
        {
            LOG_OUTPUT(L"INCOMING: data 2 0x%08x", data->data2);
            if (payloadSize >= sizeof(UMP96))
            {
                LOG_OUTPUT(L"INCOMING: data 3 0x%08x", data->data3);
                if (payloadSize >= sizeof(UMP128))
                {
                    LOG_OUTPUT(L"INCOMING: data 4 0x%08x", data->data4);
                    if (payloadSize > sizeof(UMP128))
                    {
                        LOG_OUTPUT(L"INCOMING: Buffer contains additional 0x%08x bytes of data", (UINT32) (payloadSize - sizeof(UMP128)));
                    }
                }
            }
        }
    }
}

