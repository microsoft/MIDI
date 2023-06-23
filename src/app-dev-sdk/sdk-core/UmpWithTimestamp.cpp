// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "UmpWithTimestamp.h"
#include "UmpWithTimestamp.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    uint64_t UmpWithTimestamp::Timestamp()
    {
        throw hresult_not_implemented();
    }
    void UmpWithTimestamp::Timestamp(uint64_t value)
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::Ump UmpWithTimestamp::Ump()
    {
        throw hresult_not_implemented();
    }
    void UmpWithTimestamp::Ump(winrt::Microsoft::Devices::Midi2::Ump const& value)
    {
        throw hresult_not_implemented();
    }
}
