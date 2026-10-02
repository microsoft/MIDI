// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpHostCreationConfig.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpHostCreationConfig : MidiRtpHostCreationConfigT<MidiRtpHostCreationConfig>
    {
        MidiRtpHostCreationConfig() = default;

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_RTP_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        void ServiceInstanceName(_In_ winrt::hstring const& value) noexcept { m_serviceInstanceName = internal::TrimmedHStringCopy(value); }

        bool UseAutomaticPortAllocation() const noexcept { return m_useAutomaticPortAllocation; }
        void UseAutomaticPortAllocation(_In_ bool const value) noexcept { m_useAutomaticPortAllocation = value; }

        uint16_t ManuallyAssignedPort() const noexcept { return m_manuallyAssignedPort; }
        void ManuallyAssignedPort(_In_ uint16_t const value) noexcept { m_manuallyAssignedPort = value; }

        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }
        void AllowPortFallback(_In_ bool const value) noexcept { m_allowPortFallback = value; }

        bool Advertise() const noexcept { return m_advertise; }
        void Advertise(_In_ bool const value) noexcept { m_advertise = value; }

        rtp::MidiRtpRemoteClientPolicy RemoteClientPolicy() const noexcept { return m_remoteClientPolicy; }
        void RemoteClientPolicy(_In_ rtp::MidiRtpRemoteClientPolicy const value) noexcept { m_remoteClientPolicy = value; }

        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        void SendRecoveryJournal(_In_ bool const value) noexcept { m_sendRecoveryJournal = value; }

        winrt::guid NetworkAdapterId() const noexcept { return m_networkAdapterId; }
        void NetworkAdapterId(_In_ winrt::guid const& value) noexcept;

        winrt::hstring NetworkAdapterName() const noexcept { return m_networkAdapterName; }
        void NetworkAdapterName(_In_ winrt::hstring const& value) noexcept { m_networkAdapterName = internal::TrimmedHStringCopy(value); }

        bool AllowNetworkAdapterFallback() const noexcept { return m_allowNetworkAdapterFallback; }
        void AllowNetworkAdapterFallback(_In_ bool const value) noexcept { m_allowNetworkAdapterFallback = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_hostId{ foundation::GuidHelper::CreateNewGuid() };
        winrt::hstring m_name{};
        winrt::hstring m_serviceInstanceName{};
        bool m_useAutomaticPortAllocation{ true };
        uint16_t m_manuallyAssignedPort{ MIDI_RTP_SDK_DEFAULT_HOST_PORT };
        bool m_allowPortFallback{ true };
        bool m_advertise{ true };
        rtp::MidiRtpRemoteClientPolicy m_remoteClientPolicy{ rtp::MidiRtpRemoteClientPolicy::AllowAny };
        bool m_sendRecoveryJournal{ true };

        winrt::guid m_networkAdapterId{};
        winrt::hstring m_networkAdapterName{};

        // Not exposed. It is how the service finds the adapter again under a new GUID.
        winrt::hstring m_networkAdapterPhysicalAddress{};

        bool m_allowNetworkAdapterFallback{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpHostCreationConfig : MidiRtpHostCreationConfigT<MidiRtpHostCreationConfig, implementation::MidiRtpHostCreationConfig>
    {
    };
}
