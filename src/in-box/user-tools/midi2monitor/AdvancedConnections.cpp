// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AdvancedConnections.h"

#include <unordered_set>

namespace midi2monitor
{
    struct AdvancedConnections::State
    {
        std::mutex Lock{};
        std::condition_variable Wake{};

        // guarded by Lock
        std::vector<Wanted> Desired{};
        bool Dirty{ false };
        bool Stop{ false };

        winrt::com_ptr<MessageCapture> Capture{ nullptr };
        winrt::hstring SessionName{};
        FailureHandler OnFailure{};

        std::atomic<size_t> OpenCount{ 0 };
    };

    namespace
    {
        struct OpenConnection
        {
            uint16_t Slot{ 0xFFFF };
            midi2::MidiEndpointConnection Connection{ nullptr };
            winrt::com_ptr<IMidiEndpointConnectionRaw> Raw{ nullptr };
            GUID ConnectionId{};
        };

        std::wstring Key(_In_ std::wstring const& endpointDeviceId)
        {
            std::wstring key{ endpointDeviceId };

            std::transform(key.begin(), key.end(), key.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return key;
        }

        void Close(
            _In_ midi2::MidiSession const& session,
            _In_ MessageCapture& capture,
            _Inout_ OpenConnection& open) noexcept
        {
            try
            {
                // must happen before the connection goes away: the SDK holds the callback
                // pointer without a reference of its own
                if (open.Raw != nullptr)
                {
                    LOG_IF_FAILED(open.Raw->RemoveMessagesReceivedCallback());
                }

                capture.UnmapConnection(open.ConnectionId);

                if (session != nullptr && open.Connection != nullptr)
                {
                    session.DisconnectEndpointConnection(open.Connection.ConnectionId());
                }
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to close an advanced mode connection.")

            open.Raw = nullptr;
            open.Connection = nullptr;
        }

        bool Open(
            _In_ midi2::MidiSession const& session,
            _In_ MessageCapture& capture,
            _In_ std::wstring const& endpointDeviceId,
            _In_ uint16_t slot,
            _Out_ OpenConnection& open) noexcept
        {
            open = OpenConnection{};
            open.Slot = slot;

            try
            {
                open.Connection = session.CreateEndpointConnection(endpointDeviceId);

                if (open.Connection == nullptr)
                {
                    return false;
                }

                open.Raw = open.Connection.try_as<IMidiEndpointConnectionRaw>();

                if (open.Raw == nullptr)
                {
                    return false;
                }

                auto const id = open.Connection.ConnectionId();
                open.ConnectionId = reinterpret_cast<GUID const&>(id);

                // mapped and registered before opening, so nothing is missed on connect
                capture.MapConnection(open.ConnectionId, slot);

                if (FAILED(open.Raw->SetMessagesReceivedCallback(
                    static_cast<IMidiEndpointConnectionMessagesReceivedCallback*>(&capture))))
                {
                    Close(session, capture, open);
                    return false;
                }

                if (!open.Connection.Open())
                {
                    Close(session, capture, open);
                    return false;
                }

                return true;
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to open an advanced mode connection.")

            Close(session, capture, open);

            return false;
        }
    }

    AdvancedConnections::~AdvancedConnections() noexcept
    {
        Shutdown();
    }

    _Use_decl_annotations_
    void AdvancedConnections::Start(
        winrt::com_ptr<MessageCapture> const& capture,
        winrt::hstring const& sessionName,
        FailureHandler failureHandler) noexcept
    {
        if (m_state != nullptr || capture == nullptr)
        {
            return;
        }

        try
        {
            auto state = std::make_shared<State>();

            state->Capture = capture;
            state->SessionName = sessionName;
            state->OnFailure = std::move(failureHandler);

            std::thread worker{ &AdvancedConnections::ThreadBody, state };
            worker.detach();

            m_state = std::move(state);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to start the advanced mode connection thread.")
    }

    _Use_decl_annotations_
    void AdvancedConnections::Desired(std::vector<Wanted> wanted) noexcept
    {
        if (m_state == nullptr)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> guard{ m_state->Lock };

            if (m_state->Stop)
            {
                return;
            }

            m_state->Desired = std::move(wanted);
            m_state->Dirty = true;
        }

        m_state->Wake.notify_one();
    }

    size_t AdvancedConnections::OpenCount() const noexcept
    {
        return m_state == nullptr ? 0 : m_state->OpenCount.load(std::memory_order_relaxed);
    }

    void AdvancedConnections::Shutdown() noexcept
    {
        auto state = std::move(m_state);

        if (state == nullptr)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> guard{ state->Lock };
            state->Stop = true;
            state->Desired.clear();
            state->Dirty = true;
        }

        state->Wake.notify_one();
    }

    _Use_decl_annotations_
    void AdvancedConnections::ThreadBody(std::shared_ptr<State> state) noexcept
    {
        ::SetThreadDescription(::GetCurrentThread(), L"MIDI Monitor Connections");

        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        }
        catch (...)
        {
            // already initialized: the calls below still work
        }

        midi2::MidiSession session{ nullptr };
        std::unordered_map<std::wstring, OpenConnection> open{};

        // endpoints that wouldn't open: tried and reported once, until they stop being wanted
        std::unordered_set<std::wstring> failed{};

        auto& capture = *state->Capture;

        for (;;)
        {
            std::vector<Wanted> desired{};
            bool stop{ false };

            try
            {
                std::unique_lock<std::mutex> guard{ state->Lock };
                state->Wake.wait(guard, [&state]() { return state->Dirty || state->Stop; });

                desired = state->Desired;
                stop = state->Stop;
                state->Dirty = false;
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                stop = true;
            }

            if (stop)
            {
                desired.clear();
            }

            try
            {
                std::unordered_map<std::wstring, Wanted const*> wantedByKey{};

                for (auto const& wanted : desired)
                {
                    wantedByKey[Key(wanted.EndpointDeviceId)] = &wanted;
                }

                // close what is no longer wanted, or is wanted under another slot
                for (auto it = open.begin(); it != open.end();)
                {
                    auto const found = wantedByKey.find(it->first);

                    if (found == wantedByKey.end() || found->second->Slot != it->second.Slot)
                    {
                        Close(session, capture, it->second);
                        it = open.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                // an endpoint no longer wanted gets a fresh try the next time it is
                std::erase_if(failed, [&wantedByKey](std::wstring const& key) { return !wantedByKey.contains(key); });

                for (auto const& wanted : desired)
                {
                    auto const key = Key(wanted.EndpointDeviceId);

                    if (open.contains(key) || failed.contains(key) || wanted.Slot >= MaximumSourceSlots)
                    {
                        continue;
                    }

                    if (session == nullptr)
                    {
                        if (midi2::MidiApi::EnsureServiceAvailable())
                        {
                            session = midi2::MidiSession::Create(state->SessionName);
                        }
                    }

                    OpenConnection connection{};

                    if (session != nullptr && Open(session, capture, wanted.EndpointDeviceId, wanted.Slot, connection))
                    {
                        open.emplace(key, std::move(connection));
                    }
                    else
                    {
                        failed.insert(key);

                        if (state->OnFailure)
                        {
                            state->OnFailure(wanted.EndpointDeviceId);
                        }
                    }
                }

                // nothing open: give the session back, so an idle window holds nothing
                if (open.empty() && session != nullptr)
                {
                    session.Close();
                    session = nullptr;
                }
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the advanced mode connections.")

            state->OpenCount.store(open.size(), std::memory_order_relaxed);

            if (stop)
            {
                break;
            }
        }

        // anything still open after an exception above
        try
        {
            for (auto& entry : open)
            {
                Close(session, capture, entry.second);
            }

            open.clear();

            if (session != nullptr)
            {
                session.Close();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to close the advanced mode connections.")

        state->OpenCount.store(0, std::memory_order_relaxed);
    }
}
