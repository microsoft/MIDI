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
        bool IsMidi10Connection() const noexcept { return m_isMidi10Connection; }
        bool IsBandwidthRestricted() const noexcept { return m_isBandwidthRestricted; }
        uint8_t MidiCIMessageVersionFormat() const noexcept { return m_midiCIMessageVersionFormat; }
        uint8_t MaxSysEx8Streams() const noexcept { return m_maxSysEx8Streams; }

        collections::IVectorView<midi2::MidiGroup> IncludedGroups() { return m_includedGroups.GetView(); }

        // TODO Function to init values

    private:
        uint8_t m_number{ 0 };
        winrt::hstring m_name;
        bool m_isActive{ false };
        MidiFunctionBlockDirection m_direction{ 0 };
        MidiFunctionBlockUIHint m_uiHint{ 0 };
        bool m_isMidi10Connection{ false };
        bool m_isBandwidthRestricted{ false };
        uint8_t m_midiCIMessageVersionFormat{ 0 };
        uint8_t m_maxSysEx8Streams{ 0 };

        collections::IVector<midi2::MidiGroup>
            m_includedGroups{ winrt::single_threaded_vector<midi2::MidiGroup>() };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiFunctionBlock : MidiFunctionBlockT<MidiFunctionBlock, implementation::MidiFunctionBlock>
    {
    };
}
