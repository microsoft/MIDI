// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.SysExTransfer.MidiSystemExclusiveReceivedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    struct MidiSystemExclusiveReceivedEventArgs : MidiSystemExclusiveReceivedEventArgsT<MidiSystemExclusiveReceivedEventArgs>
    {
        MidiSystemExclusiveReceivedEventArgs() = default;

        midi2::MidiGroup Group() const noexcept { return m_group; }
        collections::IVectorView<uint8_t> Bytes() const noexcept { return m_bytes; }
        bool IsPartial() const noexcept { return m_isPartial; }

        void InternalInitialize(
            _In_ midi2::MidiGroup const& group,
            _In_ collections::IVectorView<uint8_t> const& bytes,
            _In_ bool isPartial) noexcept
        {
            m_group = group;
            m_bytes = bytes;
            m_isPartial = isPartial;
        }

    private:
        midi2::MidiGroup m_group{ nullptr };
        collections::IVectorView<uint8_t> m_bytes{ nullptr };
        bool m_isPartial{ false };
    };
}
