// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiFunctionBlock.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock>
    {
        MidiFunctionBlock() = default;

        static winrt::hstring ShortLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_FUNCTION_BLOCK_SHORT); }
        static winrt::hstring ShortLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_FUNCTION_BLOCK_SHORT_PLURAL); }
        static winrt::hstring LongLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_FUNCTION_BLOCK_FULL); }
        static winrt::hstring LongLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_FUNCTION_BLOCK_FULL_PLURAL); }



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

        midi2::MidiFunctionBlockRepresentsMidi10Connection RepresentsMidi10Connection() const noexcept { return m_midi10Connection; }
        void RepresentsMidi10Connection(_In_ midi2::MidiFunctionBlockRepresentsMidi10Connection const value) noexcept { if (!m_isReadOnly) m_midi10Connection = value; }

        uint8_t MidiCIMessageVersionFormat() const noexcept { return m_midiCIMessageVersionFormat; }
        void MidiCIMessageVersionFormat(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_midiCIMessageVersionFormat = value; }

        uint8_t MaxSystemExclusive8Streams() const noexcept { return m_maxSysEx8Streams; }
        void MaxSystemExclusive8Streams(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_maxSysEx8Streams = value; }

        midi2::MidiGroup FirstGroup() const noexcept { return m_firstGroup; }
        void FirstGroup(_In_ midi2::MidiGroup const& value) noexcept { if (!m_isReadOnly) m_firstGroup = value; }

        uint8_t GroupCount() const noexcept { return m_numberOfGroupsSpanned; }
        void GroupCount(_In_ uint8_t const value) noexcept { if (!m_isReadOnly) m_numberOfGroupsSpanned = value; }

        bool IncludesGroup(_In_ midi2::MidiGroup const& group) const noexcept { return (group.Index() >= m_firstGroup.Index()) && (group.Index() < m_firstGroup.Index() + GroupCount()); }


        winrt::hstring ToString();


        bool UpdateFromJson(_In_ winrt::Windows::Data::Json::JsonObject const json) noexcept;
        bool UpdateFromJsonString(_In_ winrt::hstring const json) noexcept;
        winrt::hstring GetJsonString() noexcept;

       // bool UpdateFromMessages(_In_ collections::IIterable<midi2::IMidiUniversalPacket> messages) noexcept;

        bool UpdateFromDevPropertyStruct(_In_ MidiFunctionBlockProperty prop);

        void InternalSetName(_In_ winrt::hstring name) noexcept { m_name = name; }
        void InternalSetisReadOnly(_In_ bool isReadOnly) noexcept { m_isReadOnly = isReadOnly; }

    private:
        bool m_isReadOnly{ false };
        uint8_t m_number{ 0 };
        winrt::hstring m_name{};
        bool m_isActive{ false };
        midi2::MidiFunctionBlockDirection m_direction{ midi2::MidiFunctionBlockDirection::Undefined };
        midi2::MidiFunctionBlockUIHint m_uiHint{ midi2::MidiFunctionBlockUIHint::Unknown };
        midi2::MidiFunctionBlockRepresentsMidi10Connection m_midi10Connection{ midi2::MidiFunctionBlockRepresentsMidi10Connection::Reserved };
        uint8_t m_midiCIMessageVersionFormat{ 0 };
        uint8_t m_maxSysEx8Streams{ 0 };

        //uint8_t m_firstGroupIndex{ 0 };
        midi2::MidiGroup m_firstGroup;
        uint8_t m_numberOfGroupsSpanned{ 0 };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock, implementation::MidiFunctionBlock>
    {
    };
}
