// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiProperty.h"
#include "MidiProperty.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiProperty::RawJson()
    {
        throw hresult_not_implemented();
    }
}
