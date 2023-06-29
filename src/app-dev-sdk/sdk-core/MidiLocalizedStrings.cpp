// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiLocalizedStrings.h"
#include "MidiLocalizedStrings.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiLocalizedStrings::LanguageCode()
    {
        throw hresult_not_implemented();
    }
    void MidiLocalizedStrings::LanguageCode(hstring const& value)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::GetMidiMessageTypeLabel(bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::FormatMessageTypeValue(winrt::Microsoft::Devices::Midi2::MidiUmpMessageType const& messageType, bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::GetGroupLabel(bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::FormatGroupValue(uint8_t groupIndex)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::GetChannelLabel(bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::FormatChannelValue(uint8_t channelIndex)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::GetUmpEndpointLabel(bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::GetAllGroupsValue(bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLocalizedStrings::GetAllChannelsValue(bool abbreviatedVersion)
    {
        throw hresult_not_implemented();
    }
}
