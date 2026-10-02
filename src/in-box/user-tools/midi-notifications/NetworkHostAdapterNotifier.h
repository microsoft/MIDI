// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Tells the customer when a host on this PC cannot start, because the network adapter it is
// limited to is missing and it is set not to use any other. A host which falls back to every
// adapter is still running, so it gets no banner.
//
// One per transport, so each banner opens the right page. Runs on the app's message thread, like
// the other notifiers. The service's signal only says "ask again": which hosts are waiting comes
// from the service through the SDK.
class NetworkHostAdapterNotifier
{
public:
    enum class Transport
    {
        NetworkMidi2,
        RtpMidi,
    };

    explicit NetworkHostAdapterNotifier(_In_ Transport const transport) noexcept :
        m_transport{ transport }
    {
    }

    void Evaluate() noexcept;

private:
    struct WaitingHost
    {
        std::wstring Identity;      // the host's entry id
        std::wstring DisplayName;   // already sanitized for a toast
        std::wstring AdapterName;   // already sanitized for a toast
    };

    std::vector<WaitingHost> ReadWaitingHosts() const noexcept;

    void Notify(_In_ std::vector<WaitingHost> const& waiting) noexcept;

    Transport m_transport;

    ToastSender m_toast{ };

    // Hosts the customer has already been told about, while they are still waiting
    std::set<std::wstring> m_reportedIdentities{ };

    std::chrono::steady_clock::time_point m_lastToast{ };

    // An adapter which comes and goes, like Wi-Fi at the edge of its range, would otherwise bring
    // up a banner each time it went away
    static constexpr std::chrono::seconds MinimumIntervalBetweenToasts{ 60 };
};
