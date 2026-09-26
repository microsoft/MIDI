// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace glass
{
    // What one running layout wants from one entry of its device table. An empty id means the
    // device is not here: the entry keeps its place so the destination indexes the binding engine
    // resolved still line up, and nothing is opened for it.
    struct OutputRequest
    {
        std::wstring EndpointDeviceId{};

        // One bit per group this layout drives on that device. Panic reads it, so a panic is
        // loud where the layout was playing and silent everywhere else.
        uint16_t GroupMask{ 0 };
    };

    // Incoming words from one endpoint. Raised on a service callback thread, so a handler copies
    // what it needs and marshals; the buffer belongs to the caller.
    using FeedbackHandler = std::function<void(
        std::wstring const& endpointDeviceId,
        uint64_t timestamp,
        uint32_t wordCount,
        uint32_t const* words)>;

    // One connection per endpoint per process, shared by every control, every page and every
    // running layout.
    //
    // Group and channel travel inside the message, so hundreds of controls spread over four
    // groups of one device still cost exactly one connection. That is also what makes Panic mean
    // "everything this app is driving" rather than "everything this window is driving", and what
    // stops two layouts pointing at the same synth from fighting over it.
    //
    // Everything here blocks on the service, so it must be called from a background thread. The
    // send table it hands back is used from the UI thread and nothing else, so the hot path takes
    // no lock.
    class OutputRouter
    {
    public:
        static OutputRouter& Current() noexcept;

        // Opens what this owner needs, shares what is already open and closes what nothing wants
        // any more. The table comes back one entry per request, in order, with a null where the
        // device was not there.
        //
        // Blocking. Never call it from the UI thread.
        bool Open(
            _In_ std::wstring const& ownerId,
            _In_ std::vector<OutputRequest> const& requests,
            _In_ FeedbackHandler handler,
            _Out_ std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>>& table) noexcept;

        void Close(_In_ std::wstring const& ownerId) noexcept;

        // All notes off, all sound off, sustain off and pitch bend center, on every group and
        // channel this process is driving. Blocking.
        void Panic() noexcept;

        // The most words one call to this connection may carry. Zero when the connection is not
        // one of ours.
        uint32_t MaximumWordsPerSend(
            _In_ winrt::com_ptr<IMidiEndpointConnectionRaw> const& connection) const noexcept;

        winrt::hstring LastErrorMessage() const noexcept;

        size_t OpenConnectionCount() const noexcept;

        // Drops everything, for app shutdown.
        void Shutdown() noexcept;

    private:
        OutputRouter() noexcept = default;

        struct ReceiveHub;

        struct Entry
        {
            std::wstring EndpointDeviceId{};
            midi2::MidiEndpointConnection Connection{ nullptr };
            winrt::com_ptr<IMidiEndpointConnectionRaw> Raw{ nullptr };
            winrt::com_ptr<ReceiveHub> Hub{ nullptr };

            uint32_t MaximumWords{ 0 };
            uint16_t GroupMask{ 0 };
        };

        // Called by a hub, on a service callback thread.
        void OnMessagesReceived(
            _In_ std::wstring const& endpointDeviceId,
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words) noexcept;

        bool RebuildLocked() noexcept;
        void PublishHandlersLocked() noexcept;
        void CloseEntryLocked(_Inout_ Entry& entry) noexcept;

        mutable std::mutex m_lock{};

        midi2::MidiSession m_session{ nullptr };

        // Keyed by lowercased endpoint device id, so one endpoint is only ever opened once.
        std::map<std::wstring, Entry> m_connections{};

        std::map<std::wstring, std::vector<OutputRequest>> m_owners{};
        std::map<std::wstring, FeedbackHandler> m_handlers{};

        // Swapped whole rather than edited, and behind its own lock, so a callback thread reads a
        // settled list without waiting on the lock a blocking Open is holding.
        mutable std::shared_mutex m_handlerLock{};
        std::shared_ptr<std::vector<FeedbackHandler> const> m_publishedHandlers{};

        winrt::hstring m_lastError{};
    };
}
