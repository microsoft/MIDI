// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OutputRouter.h"
#include "PanicMessages.h"
#include "StringResources.h"

namespace glass
{
    namespace
    {
        constexpr wchar_t SessionName[] = L"MIDI Glass";

        constexpr uint32_t MaximumWordsPerUmp = 4;

        constexpr uint8_t GroupCount = 16;

        std::wstring LowerCopy(_In_ std::wstring value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }
    }

    // One per open endpoint. Registered as the raw received callback before the connection is
    // opened, so nothing can arrive before there is somewhere to put it.
    struct OutputRouter::ReceiveHub : winrt::implements<ReceiveHub, IMidiEndpointConnectionMessagesReceivedCallback>
    {
        OutputRouter* Owner{ nullptr };
        std::wstring EndpointDeviceId{};
        std::atomic<bool> Running{ false };

        STDMETHOD(MessagesReceived)(
            GUID sessionId,
            GUID connectionId,
            UINT64 timestamp,
            UINT32 wordCount,
            UINT32 const* messages) override
        {
            UNREFERENCED_PARAMETER(sessionId);
            UNREFERENCED_PARAMETER(connectionId);

            if (Owner != nullptr && messages != nullptr && wordCount > 0 &&
                Running.load(std::memory_order_relaxed))
            {
                Owner->OnMessagesReceived(EndpointDeviceId, timestamp, wordCount, messages);
            }

            return S_OK;
        }
    };

    OutputRouter& OutputRouter::Current() noexcept
    {
        // Deliberately never destroyed. Closing a layout hands its connections back on a
        // background thread, and that thread can still be running while the process is tearing
        // its statics down; a destroyed router touched from there would be a crash on exit.
        static auto* const instance = new OutputRouter{};

        return *instance;
    }

    _Use_decl_annotations_
    void OutputRouter::OnMessagesReceived(
        std::wstring const& endpointDeviceId,
        uint64_t timestamp,
        uint32_t wordCount,
        uint32_t const* words) noexcept
    {
        std::shared_ptr<std::vector<FeedbackHandler> const> handlers{};

        {
            std::shared_lock guard{ m_handlerLock };
            handlers = m_publishedHandlers;
        }

        if (handlers == nullptr)
        {
            return;
        }

        for (auto const& handler : *handlers)
        {
            try
            {
                handler(endpointDeviceId, timestamp, wordCount, words);
            }
            catch (...)
            {
            }
        }
    }

    void OutputRouter::PublishHandlersLocked() noexcept
    {
        auto published = std::make_shared<std::vector<FeedbackHandler>>();
        published->reserve(m_handlers.size());

        for (auto const& [ownerId, handler] : m_handlers)
        {
            UNREFERENCED_PARAMETER(ownerId);

            if (handler)
            {
                published->push_back(handler);
            }
        }

        std::unique_lock guard{ m_handlerLock };
        m_publishedHandlers = std::move(published);
    }

    _Use_decl_annotations_
    void OutputRouter::CloseEntryLocked(Entry& entry) noexcept
    {
        if (entry.Hub != nullptr)
        {
            entry.Hub->Running.store(false);
            entry.Hub->Owner = nullptr;
        }

        if (entry.Raw != nullptr)
        {
            LOG_IF_FAILED(entry.Raw->RemoveMessagesReceivedCallback());
        }

        try
        {
            if (entry.Connection != nullptr && m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(entry.Connection.ConnectionId());
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to disconnect an endpoint connection.")

        entry.Hub = nullptr;
        entry.Raw = nullptr;
        entry.Connection = nullptr;
    }

    bool OutputRouter::RebuildLocked() noexcept
    {
        // What every owner between them wants, and the groups they drive there.
        std::map<std::wstring, uint16_t> wanted{};

        for (auto const& [ownerId, requests] : m_owners)
        {
            UNREFERENCED_PARAMETER(ownerId);

            for (auto const& request : requests)
            {
                if (request.EndpointDeviceId.empty())
                {
                    continue;
                }

                wanted[LowerCopy(request.EndpointDeviceId)] |= request.GroupMask;
            }
        }

        // Close what nothing wants any more, before opening anything, so a machine with one
        // single-client device can hand it from one layout to the next.
        for (auto it = m_connections.begin(); it != m_connections.end();)
        {
            if (wanted.find(it->first) == wanted.end())
            {
                CloseEntryLocked(it->second);
                it = m_connections.erase(it);
            }
            else
            {
                it->second.GroupMask = wanted[it->first];
                ++it;
            }
        }

        if (wanted.empty())
        {
            // Nothing left to drive, so the session goes too rather than holding the service open
            // for an app with no layout running.
            try
            {
                if (m_session != nullptr)
                {
                    m_session.Close();
                    m_session = nullptr;
                }
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to close the MIDI session.")

            return true;
        }

        if (m_session == nullptr)
        {
            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                m_lastError = ::midiglass::resources::GetString(L"ErrorServiceUnavailable");
                return false;
            }

            m_session = midi2::MidiSession::Create(SessionName);

            if (m_session == nullptr)
            {
                m_lastError = ::midiglass::resources::GetString(L"ErrorSessionFailed");
                return false;
            }
        }

        for (auto const& [id, groupMask] : wanted)
        {
            if (m_connections.find(id) != m_connections.end())
            {
                continue;
            }

            Entry entry{};
            entry.EndpointDeviceId = id;
            entry.GroupMask = groupMask;

            try
            {
                entry.Connection = m_session.CreateEndpointConnection(winrt::hstring{ id });
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to create an endpoint connection.")

            if (entry.Connection == nullptr)
            {
                // An endpoint that went away between resolving and here is not an error. Its
                // controls stay quiet until it comes back.
                continue;
            }

            entry.Raw = entry.Connection.try_as<IMidiEndpointConnectionRaw>();

            if (entry.Raw == nullptr)
            {
                m_lastError = ::midiglass::resources::GetString(L"ErrorNoComExtensions");
                continue;
            }

            entry.MaximumWords = entry.Raw->GetSupportedMaxMidiWordsPerTransmission();

            if (entry.MaximumWords < MaximumWordsPerUmp)
            {
                continue;
            }

            auto hub = winrt::make_self<ReceiveHub>();
            hub->Owner = this;
            hub->EndpointDeviceId = id;

            // The callback has to be in place before Open, or feedback that arrives during the
            // open is lost and the connection has to be torn down to add one.
            if (SUCCEEDED(entry.Raw->SetMessagesReceivedCallback(hub.get())))
            {
                entry.Hub = hub;
            }

            bool opened{ false };

            try
            {
                opened = entry.Connection.Open();
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to open an endpoint connection.")

            if (!opened)
            {
                CloseEntryLocked(entry);
                continue;
            }

            if (entry.Hub != nullptr)
            {
                entry.Hub->Running.store(true);
            }

            m_connections.emplace(id, std::move(entry));
        }

        return true;
    }

    _Use_decl_annotations_
    bool OutputRouter::Open(
        std::wstring const& ownerId,
        std::vector<OutputRequest> const& requests,
        FeedbackHandler handler,
        std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>>& table) noexcept
    {
        std::scoped_lock guard{ m_lock };

        table.clear();
        table.resize(requests.size());

        m_owners[ownerId] = requests;
        m_handlers[ownerId] = std::move(handler);

        PublishHandlersLocked();

        auto const rebuilt = RebuildLocked();

        for (size_t i = 0; i < requests.size(); ++i)
        {
            if (requests[i].EndpointDeviceId.empty())
            {
                continue;
            }

            auto const it = m_connections.find(LowerCopy(requests[i].EndpointDeviceId));

            if (it != m_connections.end())
            {
                table[i] = it->second.Raw;
            }
        }

        return rebuilt;
    }

    _Use_decl_annotations_
    void OutputRouter::Close(std::wstring const& ownerId) noexcept
    {
        std::scoped_lock guard{ m_lock };

        m_owners.erase(ownerId);
        m_handlers.erase(ownerId);

        PublishHandlersLocked();
        RebuildLocked();
    }

    void OutputRouter::Panic() noexcept
    {
        std::scoped_lock guard{ m_lock };

        std::array<uint32_t, PanicWordsPerChannel * PanicChannelCount> words{};

        for (auto& [id, entry] : m_connections)
        {
            UNREFERENCED_PARAMETER(id);

            if (entry.Raw == nullptr || entry.GroupMask == 0)
            {
                continue;
            }

            for (uint8_t group = 0; group < GroupCount; group++)
            {
                if ((entry.GroupMask & (1u << group)) == 0)
                {
                    continue;
                }

                auto const written = BuildPanicWordsForGroup(group, words);

                // Timestamp zero means send it now. A panic that waited its turn behind
                // scheduled messages would not be a panic.
                for (uint32_t position = 0; position < written;)
                {
                    auto const chunk = std::min(entry.MaximumWords, written - position);

                    LOG_IF_FAILED(entry.Raw->SendMidiMessagesRaw(0, chunk, words.data() + position));

                    position += chunk;
                }
            }
        }
    }

    _Use_decl_annotations_
    uint32_t OutputRouter::MaximumWordsPerSend(
        winrt::com_ptr<IMidiEndpointConnectionRaw> const& connection) const noexcept
    {
        if (connection == nullptr)
        {
            return 0;
        }

        std::scoped_lock guard{ m_lock };

        for (auto const& [id, entry] : m_connections)
        {
            UNREFERENCED_PARAMETER(id);

            if (entry.Raw == connection)
            {
                return entry.MaximumWords;
            }
        }

        return 0;
    }

    winrt::hstring OutputRouter::LastErrorMessage() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_lastError;
    }

    size_t OutputRouter::OpenConnectionCount() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_connections.size();
    }

    void OutputRouter::Shutdown() noexcept
    {
        std::scoped_lock guard{ m_lock };

        m_owners.clear();
        m_handlers.clear();

        PublishHandlersLocked();
        RebuildLocked();
    }
}
