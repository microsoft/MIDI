// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.SysExTransfer.MidiSystemExclusiveReceiver.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    struct MidiSystemExclusiveReceiver : MidiSystemExclusiveReceiverT<MidiSystemExclusiveReceiver>
    {
        MidiSystemExclusiveReceiver() = default;

        MidiSystemExclusiveReceiver(
            _In_ midi2::MidiEndpointConnection const& sourceConnection,
            _In_ midi2::MidiGroup const& sourceGroup,
            _In_ uint32_t maximumBytesPerEvent);

        winrt::event_token BytesReceived(
            _In_ foundation::TypedEventHandler<sysex::MidiSystemExclusiveReceiver, sysex::MidiSystemExclusiveReceivedEventArgs> const& handler)
        {
            return m_bytesReceivedEvent.add(handler);
        }

        void BytesReceived(_In_ winrt::event_token const& token) noexcept
        {
            m_bytesReceivedEvent.remove(token);
        }

        bool Start();
        void Stop();

        bool IsReceiving() const noexcept { return m_isReceiving; }

        uint64_t CountBytesReceived() const noexcept { return m_countBytesReceived; }
        uint64_t CountMessagesReceived() const noexcept { return m_countMessagesReceived; }

        void Close() noexcept;

        ~MidiSystemExclusiveReceiver() noexcept { Close(); }

    private:
        void OnMessageReceived(
            _In_ foundation::IInspectable const& sender,
            _In_ midi2::MidiMessageReceivedEventArgs const& args) noexcept;

        void AppendPayloadByte(_In_ uint8_t value) noexcept;
        void RaisePending(_In_ bool flushing) noexcept;

        midi2::MidiEndpointConnection m_connection{ nullptr };
        midi2::MidiGroup m_group{ nullptr };
        uint32_t m_maximumBytesPerEvent{ 256 };

        winrt::event<foundation::TypedEventHandler<sysex::MidiSystemExclusiveReceiver, sysex::MidiSystemExclusiveReceivedEventArgs>> m_bytesReceivedEvent{};
        winrt::event_token m_messageReceivedToken{};

        bool m_isReceiving{ false };

        // bytes waiting to be handed out in the next event
        std::vector<uint8_t> m_pending{};

        // whether the pending block contains a matched F0/F7 pair
        bool m_pendingSawStart{ false };
        bool m_pendingSawEnd{ false };

        // framing state across events, so a message split over several blocks is tracked
        bool m_insideMessage{ false };

        uint64_t m_countBytesReceived{ 0 };
        uint64_t m_countMessagesReceived{ 0 };

        std::mutex m_lock{};
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::factory_implementation
{
    struct MidiSystemExclusiveReceiver : MidiSystemExclusiveReceiverT<MidiSystemExclusiveReceiver, implementation::MidiSystemExclusiveReceiver>
    {
    };
}
