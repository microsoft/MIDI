// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiGroup.h"
#include "MidiGroup.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    uint8_t MidiGroup::GroupIndex()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiGroup::GroupNumberForDisplay()
    {
        throw hresult_not_implemented();
    }
    bool MidiGroup::IsActive()
    {
        throw hresult_not_implemented();
    }
}
