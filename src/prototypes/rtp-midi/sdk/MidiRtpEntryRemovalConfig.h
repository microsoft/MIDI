// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpEntryRemovalConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpEntryRemovalConfig : MidiRtpEntryRemovalConfigT<MidiRtpEntryRemovalConfig>
    {
        MidiRtpEntryRemovalConfig(_In_ winrt::guid const& entryId, _In_ bool const isHost) noexcept :
            m_entryId(entryId),
            m_isHost(isHost)
        {
        }

        winrt::guid TransportId() const noexcept { return MIDI_RTP_TRANSPORT_ID_FOR_SDK; }
        json::JsonObject ConfigJson() const noexcept;

        winrt::guid EntryId() const noexcept { return m_entryId; }
        bool IsHost() const noexcept { return m_isHost; }

    private:
        winrt::guid m_entryId{};
        bool m_isHost{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpEntryRemovalConfig : MidiRtpEntryRemovalConfigT<MidiRtpEntryRemovalConfig, implementation::MidiRtpEntryRemovalConfig>
    {
    };
}
