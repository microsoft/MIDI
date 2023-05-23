// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
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
