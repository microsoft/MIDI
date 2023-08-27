// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiFunctionBlock.h"
#include "MidiFunctionBlock.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    uint8_t MidiFunctionBlock::Number()
    {
        throw hresult_not_implemented();
    }
    hstring MidiFunctionBlock::Name()
    {
        throw hresult_not_implemented();
    }
    bool MidiFunctionBlock::IsActive()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiFunctionBlockDirection MidiFunctionBlock::Direction()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiFunctionBlockUIHint MidiFunctionBlock::UIHint()
    {
        throw hresult_not_implemented();
    }
    bool MidiFunctionBlock::IsMidi10Connection()
    {
        throw hresult_not_implemented();
    }
    bool MidiFunctionBlock::IsBandwidthRestricted()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Devices::Midi2::MidiGroup> MidiFunctionBlock::IncludedGroups()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiFunctionBlock::MidiCIMessageVersionFormat()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiFunctionBlock::MaxSysEx8Streams()
    {
        throw hresult_not_implemented();
    }
}
