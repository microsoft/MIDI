// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiCapabilityInquiryMessageReceivedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiCapabilityInquiryMessageReceivedEventArgs : MidiCapabilityInquiryMessageReceivedEventArgsT<MidiCapabilityInquiryMessageReceivedEventArgs>
    {
        MidiCapabilityInquiryMessageReceivedEventArgs() = default;

        MidiCapabilityInquiryMessageReceivedEventArgs(
            _In_ ci::MidiCapabilityInquiryMessage const& message,
            _In_ midi2::MidiGroup const& group,
            _In_ internal::MidiTimestamp const timestamp) noexcept
            : m_message(message), m_group(group), m_timestamp(timestamp) {}

        ci::MidiCapabilityInquiryMessage Message() const noexcept { return m_message; }
        midi2::MidiGroup Group() const noexcept { return m_group; }
        internal::MidiTimestamp Timestamp() const noexcept { return m_timestamp; }

    private:
        ci::MidiCapabilityInquiryMessage m_message{ nullptr };
        midi2::MidiGroup m_group{ nullptr };
        internal::MidiTimestamp m_timestamp{ 0 };
    };
}
