// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpTransportManager.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    // Parameters are taken by value because each one is used after the coroutine resumes on
    // another thread, where a reference to the caller's value would dangle
    struct MidiRtpTransportManager
    {
        static bool IsTransportAvailable() noexcept;
        static winrt::guid TransportId() noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        static foundation::IAsyncOperation<rtp::MidiRtpHostCreationResponse> CreateRtpHostAsync(_In_ rtp::MidiRtpHostCreationConfig const creationConfig) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpHostRemovalResponse> RemoveRtpHostAsync(_In_ rtp::MidiRtpHostRemovalConfig const removalConfig) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpHostUpdateResponse> StopRtpHostAsync(_In_ winrt::guid const hostId) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpHostUpdateResponse> StartRtpHostAsync(_In_ winrt::guid const hostId) noexcept;

        static foundation::IAsyncOperation<rtp::MidiRtpClientConnectResponse> ConnectRtpClientAsync(_In_ rtp::MidiRtpClientConnectConfig const connectConfig) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpClientConnectResponse> ReconnectRtpClientAsync(_In_ winrt::guid const clientId) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpClientDisconnectResponse> DisconnectRtpClientAsync(_In_ rtp::MidiRtpClientDisconnectConfig const disconnectConfig) noexcept;

        static foundation::IAsyncOperation<rtp::MidiRtpRemoteClientApprovalResponse> ApproveOrDenyRemoteClientConnectRequestAsync(_In_ rtp::MidiRtpRemoteClientApprovalConfig const approvalConfig) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpRemoteClientDisconnectResponse> DisconnectRemoteClientAsync(_In_ rtp::MidiRtpRemoteClientDisconnectConfig const disconnectConfig) noexcept;
        static foundation::IAsyncOperation<rtp::MidiRtpRemoteClientForgetResponse> ForgetRemoteClientAsync(_In_ rtp::MidiRtpRemoteClientForgetConfig const forgetConfig) noexcept;

        static collections::IVectorView<rtp::MidiRtpConfiguredHost> GetConfiguredHosts() noexcept;
        static collections::IVectorView<rtp::MidiRtpConfiguredClient> GetConfiguredClients() noexcept;
        static collections::IVectorView<rtp::MidiRtpPendingRemoteClient> GetPendingRemoteClients() noexcept;
        static collections::IVectorView<rtp::MidiRtpAdvertisedHost> GetAdvertisedHosts() noexcept;

        static winrt::hstring MidiRtpDnsServiceType() noexcept { return MIDI_RTP_SDK_DNSSD_SERVICE_TYPE; }
        static winrt::hstring MidiRtpDnsDomain() noexcept { return MIDI_RTP_SDK_DNSSD_DOMAIN; }
        static winrt::hstring MidiRtpDnsSdQueryName() noexcept { return MIDI_RTP_SDK_DNSSD_SERVICE_TYPE L"." MIDI_RTP_SDK_DNSSD_DOMAIN; }
        static uint16_t DefaultHostPort() noexcept { return MIDI_RTP_SDK_DEFAULT_HOST_PORT; }
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpTransportManager : MidiRtpTransportManagerT<MidiRtpTransportManager, implementation::MidiRtpTransportManager, winrt::static_lifetime>
    {
    };
}
