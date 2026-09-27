// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpTransportManager.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpTransportManager
    {
        MidiRtpTransportManager() = default;

        static bool IsTransportAvailable() noexcept;
        static winrt::guid TransportId() noexcept { return MIDI_RTP_TRANSPORT_ID_FOR_SDK; }
        static winrt::hstring DnsSdServiceType() noexcept { return MIDI_RTP_DNSSD_SERVICE_TYPE_FOR_SDK; }
        static uint16_t DefaultHostPort() noexcept { return MIDI_RTP_DEFAULT_HOST_PORT; }

        // Coroutine parameters are taken by value, because a reference does not survive the
        // first suspension
        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> CreateHostAsync(_In_ rtp::MidiRtpHostConfig const config) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> RemoveHostAsync(_In_ winrt::guid const hostId) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> StartHostAsync(_In_ winrt::guid const hostId) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> StopHostAsync(_In_ winrt::guid const hostId) noexcept;

        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> ConnectClientAsync(_In_ rtp::MidiRtpClientConfig const config) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> RemoveClientAsync(_In_ winrt::guid const clientId) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> ReconnectClientAsync(_In_ winrt::guid const clientId) noexcept;

        static foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> DisconnectConnectionAsync(_In_ winrt::guid const entryId, _In_ uint32_t const connectionId) noexcept;

        static collections::IVectorView<rtp::MidiRtpConfiguredHost> GetConfiguredHosts() noexcept;
        static collections::IVectorView<rtp::MidiRtpConfiguredClient> GetConfiguredClients() noexcept;
        static collections::IVectorView<rtp::MidiRtpAdvertisedPeer> GetAdvertisedPeers() noexcept;
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpTransportManager : MidiRtpTransportManagerT<MidiRtpTransportManager, implementation::MidiRtpTransportManager>
    {
    };
}
