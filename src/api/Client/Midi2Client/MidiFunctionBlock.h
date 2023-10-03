// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiFunctionBlock.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock>
    {
        MidiFunctionBlock() = default;

        uint8_t Number() const noexcept { return m_number; }
        winrt::hstring Name() const noexcept { return m_name; }
        bool IsActive() const noexcept { return m_isActive; }
        midi2::MidiFunctionBlockDirection Direction() const noexcept { return m_direction; }
        midi2::MidiFunctionBlockUIHint UIHint() const noexcept { return m_uiHint; }
        MidiFunctionBlockMidi10 Midi10Connection() const noexcept { return m_midi10Connection; }
        uint8_t MidiCIMessageVersionFormat() const noexcept { return m_midiCIMessageVersionFormat; }
        uint8_t MaxSystemExclusive8Streams() const noexcept { return m_maxSysEx8Streams; }

        uint8_t FirstGroupIndex() { return m_firstGroupIndex; }
        uint8_t NumberOfGroupsSpanned() { return m_numberOfGroupsSpanned; }

        bool UpdateFromJson(_In_ winrt::Windows::Data::Json::JsonObject const json) noexcept;
        bool UpdateFromJsonString(_In_ winrt::hstring const json) noexcept;
        bool UpdateFromMessages(_In_ collections::IIterable<midi2::MidiMessage128> messages) noexcept;
        winrt::hstring GetJsonString() noexcept;

    private:
        uint8_t m_number{ 0 };
        winrt::hstring m_name;
        bool m_isActive{ false };
        MidiFunctionBlockDirection m_direction;
        MidiFunctionBlockUIHint m_uiHint;
        MidiFunctionBlockMidi10 m_midi10Connection;
        uint8_t m_midiCIMessageVersionFormat{ 0 };
        uint8_t m_maxSysEx8Streams{ 0 };

        uint8_t m_firstGroupIndex{ 0 };
        uint8_t m_numberOfGroupsSpanned{ 0 };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock, implementation::MidiFunctionBlock>
    {
    };
}
