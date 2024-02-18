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

        bool IsReadOnly() { return m_isReadOnly; }

        uint8_t Number() const noexcept { return m_number; }
        void Number(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_number = value; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const value) noexcept { if (!m_isReadOnly) m_name = value; }

        bool IsActive() const noexcept { return m_isActive; }
        void IsActive(_In_ bool const value) noexcept { if (!m_isReadOnly) m_isActive = value; }

        midi2::MidiFunctionBlockDirection Direction() const noexcept { return m_direction; }
        void Direction(_In_ midi2::MidiFunctionBlockDirection const value) noexcept { if (!m_isReadOnly) m_direction = value; }

        midi2::MidiFunctionBlockUIHint UIHint() const noexcept { return m_uiHint; }
        void UIHint(_In_ midi2::MidiFunctionBlockUIHint const value) noexcept { if (!m_isReadOnly) m_uiHint = value; }

        midi2::MidiFunctionBlockMidi10 Midi10Connection() const noexcept { return m_midi10Connection; }
        void Midi10Connection(_In_ midi2::MidiFunctionBlockMidi10 const value) noexcept { if (!m_isReadOnly) m_midi10Connection = value; }

        uint8_t MidiCIMessageVersionFormat() const noexcept { return m_midiCIMessageVersionFormat; }
        void MidiCIMessageVersionFormat(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_midiCIMessageVersionFormat = value; }

        uint8_t MaxSystemExclusive8Streams() const noexcept { return m_maxSysEx8Streams; }
        void MaxSystemExclusive8Streams(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_maxSysEx8Streams = value; }

        uint8_t FirstGroupIndex() { return m_firstGroupIndex; }
        void FirstGroupIndex(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_firstGroupIndex = value; }

        uint8_t GroupCount() { return m_numberOfGroupsSpanned; }
        void GroupCount(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_numberOfGroupsSpanned = value; }

        bool IncludesGroup(_In_ midi2::MidiGroup const& group) { return group.Index() >= FirstGroupIndex() && group.Index() < FirstGroupIndex() + GroupCount(); }




        bool UpdateFromJson(_In_ winrt::Windows::Data::Json::JsonObject const json) noexcept;
        bool UpdateFromJsonString(_In_ winrt::hstring const json) noexcept;
        winrt::hstring GetJsonString() noexcept;

        bool UpdateFromMessages(_In_ collections::IIterable<midi2::IMidiUniversalPacket> messages) noexcept;

        bool UpdateFromDevPropertyStruct(_In_ MidiFunctionBlockProperty prop);

        void InternalSetName(_In_ winrt::hstring name) { m_name = name; }
        void InternalSetisReadOnly(_In_ bool isReadOnly) { m_isReadOnly = isReadOnly; }

    private:
        bool m_isReadOnly{ false };
        uint8_t m_number{ 0 };
        winrt::hstring m_name{};
        bool m_isActive{ false };
        midi2::MidiFunctionBlockDirection m_direction{midi2::MidiFunctionBlockDirection::Undefined};
        midi2::MidiFunctionBlockUIHint m_uiHint{midi2::MidiFunctionBlockUIHint::Unknown};
        midi2::MidiFunctionBlockMidi10 m_midi10Connection{midi2::MidiFunctionBlockMidi10::Reserved};
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
