// Copyright (c) Microsoft Corporation. All rights reserved.

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <WexTestClass.h>

#include "MidiTestCommon.h"

// filler data, not a real midi message but meets ump format and size
UMP128 g_MidiTestData = {0x12, 0xBD, 0x1234, 0xbaadf00d, 0xdeadbeef, 0xd000000d };

_Use_decl_annotations_
void PrintMidiMessage(PVOID Payload, UINT32 PayloadSize, UINT32 ExpectedPayloadSize, LONGLONG PayloadPosition)
{
    if (PayloadSize < ExpectedPayloadSize)
    {
        LOG_OUTPUT(L"INCOMING: PayloadSize %d < ExpectedPayloadSize %d", PayloadSize, ExpectedPayloadSize);
    }
    
    LOG_OUTPUT(L"Payload position is %I64d", PayloadPosition);
    
    if (PayloadSize >= sizeof(UMP32))
    {
        UMP128 *data = (UMP128 *) Payload;
        LOG_OUTPUT(L"INCOMING: mt group 0x%hx status 0x%hx data 0x%lx - size 0x%08x", data->mt_group, data->status, data->data, PayloadSize);
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

