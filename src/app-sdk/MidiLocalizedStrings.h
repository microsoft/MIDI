// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiLocalizedStrings.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiLocalizedStrings : MidiLocalizedStringsT<MidiLocalizedStrings>
    {
        MidiLocalizedStrings() = default;

        static hstring LanguageCode();
        static void LanguageCode(hstring const& value);
        static hstring GetMidiMessageTypeLabel(bool abbreviatedVersion);
        static hstring FormatMessageTypeValue(winrt::Microsoft::Devices::Midi2::MidiMessageType const& messageType, bool abbreviatedVersion);
        static hstring GetGroupLabel(bool abbreviatedVersion);
        static hstring FormatGroupValue(uint8_t groupIndex);
        static hstring GetChannelLabel(bool abbreviatedVersion);
        static hstring FormatChannelValue(uint8_t channelIndex);
        static hstring GetUmpEndpointLabel(bool abbreviatedVersion);
        static hstring GetAllGroupsValue(bool abbreviatedVersion);
        static hstring GetAllChannelsValue(bool abbreviatedVersion);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiLocalizedStrings : MidiLocalizedStringsT<MidiLocalizedStrings, implementation::MidiLocalizedStrings>
    {
    };
}
