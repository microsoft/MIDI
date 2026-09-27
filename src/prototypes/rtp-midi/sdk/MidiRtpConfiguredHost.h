// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpConfiguredHost.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpConfiguredHost : MidiRtpConfiguredHostT<MidiRtpConfiguredHost>
    {
        MidiRtpConfiguredHost() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ActualServiceInstanceName() const noexcept { return m_actualServiceInstanceName; }
        bool ServiceInstanceNameWasChanged() const noexcept { return m_serviceInstanceNameWasChanged; }
        bool IsEnabled() const noexcept { return m_isEnabled; }
        bool HasStarted() const noexcept { return m_hasStarted; }
        bool Advertise() const noexcept { return m_advertise; }
        winrt::hstring ConfiguredPort() const noexcept { return m_configuredPort; }
        winrt::hstring ActualPort() const noexcept { return m_actualPort; }
        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }
        bool UsedPortFallback() const noexcept { return m_usedPortFallback; }
        bool SendRecoveryJournal() const noexcept { return m_sendRecoveryJournal; }
        int32_t LastErrorCode() const noexcept { return m_lastErrorCode; }
        collections::IVectorView<rtp::MidiRtpConnection> Connections() const noexcept { return m_connections.GetView(); }

        // false when the entry has no usable identifier
        bool InternalInitialize(_In_ json::JsonObject const& source) noexcept;

    private:
        winrt::guid m_hostId{};
        winrt::hstring m_name{};
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_actualServiceInstanceName{};
        bool m_serviceInstanceNameWasChanged{ false };
        bool m_isEnabled{ false };
        bool m_hasStarted{ false };
        bool m_advertise{ false };
        winrt::hstring m_configuredPort{};
        winrt::hstring m_actualPort{};
        bool m_allowPortFallback{ false };
        bool m_usedPortFallback{ false };
        bool m_sendRecoveryJournal{ false };
        int32_t m_lastErrorCode{ 0 };
        collections::IVector<rtp::MidiRtpConnection> m_connections{ winrt::single_threaded_vector<rtp::MidiRtpConnection>() };
    };
}
