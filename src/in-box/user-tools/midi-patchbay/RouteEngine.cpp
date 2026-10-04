// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RouteEngine.h"
#include "StringResources.h"

#include <midi_timestamp.h>
#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t MaximumWordsPerUmp = 4;

        constexpr wchar_t SessionName[] = L"MIDI Patchbay";

        // Long enough to hear, short enough that the button does not feel stuck.
        constexpr uint32_t TestNoteMilliseconds = 350;

        std::wstring LowerCopy(_In_ std::wstring value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }
    }

    // Sends what the queued connections into one destination are holding, as fast as each
    // connection's speed allows. One thread for each destination, so a send that waits to
    // complete holds up only the device it is waiting for.
    struct RouteEngine::DestinationSender
    {
        winrt::com_ptr<IMidiEndpointConnectionRaw> Destination{ nullptr };

        // Owned by the hubs, which are destroyed only after this thread has stopped
        std::vector<Target*> Targets{};

        DestinationSender() noexcept = default;
        DestinationSender(DestinationSender const&) = delete;
        DestinationSender& operator=(DestinationSender const&) = delete;

        ~DestinationSender() noexcept
        {
            Stop();
        }

        bool Start() noexcept
        {
            try
            {
                auto const maxWords = Destination == nullptr ? 0 : Destination->GetSupportedMaxMidiWordsPerTransmission();

                if (maxWords < MaximumWordsPerUmp || Targets.empty())
                {
                    return false;
                }

                m_buffer.assign(maxWords, 0);

                if (!m_wake.try_create(wil::EventOptions::None, nullptr))
                {
                    return false;
                }

                // At MIDI 1.0 wire speed, a message can be due every millisecond or so
                m_paceTimer.reset(::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));

                if (!m_paceTimer)
                {
                    m_paceTimer.reset(::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS));
                }

                if (!m_paceTimer)
                {
                    return false;
                }

                m_ticksPerSecond = internal::GetMidiTimestampFrequency();
                m_thread = std::thread([this]() { Run(); });

                return true;
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start a send thread.")

            return false;
        }

        bool IsRunning() const noexcept
        {
            return m_thread.joinable();
        }

        void RequestStop() noexcept
        {
            m_stopRequested.store(true);

            if (m_wake)
            {
                m_wake.SetEvent();
            }
        }

        // The thread finishes the send it is in first
        void Stop() noexcept
        {
            RequestStop();

            try
            {
                if (m_thread.joinable())
                {
                    m_thread.join();
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop a send thread.")
        }

        void Wake() noexcept
        {
            m_wake.SetEvent();
        }

    private:
        void Run() noexcept
        {
            try
            {
                HANDLE const handles[]{ m_wake.get(), m_paceTimer.get() };

                while (!m_stopRequested.load())
                {
                    if (::WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE) == WAIT_FAILED)
                    {
                        // never spin on a broken handle
                        LOG_LAST_ERROR();
                        break;
                    }

                    auto const waitTicks = SendWhatIsAllowed();

                    if (waitTicks > 0)
                    {
                        ArmPaceTimer(waitTicks);
                    }
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"A send thread failed.")
        }

        // Takes from each connection in turn, so a busy one cannot hold up the others. Returns
        // how long until a sending speed lets the next message go, or zero.
        uint64_t SendWhatIsAllowed() noexcept
        {
            uint64_t earliestWait{ 0 };
            bool sentAny{ true };

            while (sentAny && !m_stopRequested.load())
            {
                sentAny = false;
                earliestWait = 0;

                for (auto* target : Targets)
                {
                    if (m_stopRequested.load())
                    {
                        break;
                    }

                    uint64_t timestamp{ 0 };
                    uint32_t messageCount{ 0 };
                    uint64_t waitTicks{ 0 };

                    auto const wordCount = target->Queue->Take(
                        internal::GetCurrentMidiTimestamp(),
                        m_buffer.data(),
                        static_cast<uint32_t>(m_buffer.size()),
                        timestamp,
                        messageCount,
                        waitTicks);

                    if (wordCount > 0)
                    {
                        sentAny = true;

                        target->MessagesForwarded.fetch_add(messageCount, std::memory_order_relaxed);

                        if (FAILED(Destination->SendMidiMessagesRaw(timestamp, wordCount, m_buffer.data())))
                        {
                            target->SendFailures.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    else if (waitTicks > 0 && (earliestWait == 0 || waitTicks < earliestWait))
                    {
                        earliestWait = waitTicks;
                    }
                }
            }

            return earliestWait;
        }

        void ArmPaceTimer(_In_ uint64_t const ticks) noexcept
        {
            if (m_ticksPerSecond == 0)
            {
                return;
            }

            // relative, in 100 nanosecond units, and never zero
            LARGE_INTEGER dueTime{};
            dueTime.QuadPart = -static_cast<LONGLONG>((std::max)((ticks * 10'000'000ull) / m_ticksPerSecond, 1ull));

            // If this fails, the next message to arrive still wakes the thread, only later
            LOG_IF_WIN32_BOOL_FALSE(::SetWaitableTimer(m_paceTimer.get(), &dueTime, 0, nullptr, nullptr, FALSE));
        }

        std::vector<uint32_t> m_buffer{};

        wil::unique_event_nothrow m_wake{};
        wil::unique_handle m_paceTimer{};

        std::atomic<bool> m_stopRequested{ false };
        uint64_t m_ticksPerSecond{ 0 };

        std::thread m_thread{};
    };

    // One per source endpoint. Registered as the raw messages-received callback on that
    // endpoint's connection, and fans every arriving batch out to its targets.
    struct RouteEngine::SourceHub : winrt::implements<SourceHub, IMidiEndpointConnectionMessagesReceivedCallback>
    {
        std::wstring SourceEndpointDeviceId{};
        winrt::com_ptr<IMidiEndpointConnectionRaw> SourceRaw{ nullptr };

        // Stable addresses: the callback thread holds pointers into this across a batch.
        std::vector<std::unique_ptr<Target>> Targets{};

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

            if (messages != nullptr && wordCount > 0)
            {
                OnMessagesReceived(timestamp, wordCount, messages);
            }

            return S_OK;
        }

        void OnMessagesReceived(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* messages) noexcept
        {
            if (!Running.load(std::memory_order_relaxed))
            {
                return;
            }

            for (auto& target : Targets)
            {
                target->SendBufferUsed = 0;
                target->SendBufferMessages = 0;
            }

            uint32_t position{ 0 };

            while (position < wordCount)
            {
                auto const word0 = messages[position];
                auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(word0);

                if (messageWordCount == 0 || messageWordCount > MaximumWordsPerUmp ||
                    position + messageWordCount > wordCount)
                {
                    // The SDK only hands over complete UMPs, so a partial tail means something
                    // upstream is malformed. Stop rather than read past the end of the buffer.
                    break;
                }

                // A groupless message says nothing about which group it belongs to, so there is
                // no way to tell which route it was meant for. Passing it on would duplicate it
                // onto every destination the customer has wired up.
                if (internal::MessageHasGroupField(word0))
                {
                    auto const group = static_cast<int32_t>(internal::GetGroupIndexFromFirstWord(word0));

                    for (auto& target : Targets)
                    {
                        if (target->SourceGroupIndex != AllGroups && target->SourceGroupIndex != group)
                        {
                            continue;
                        }

                        if (!target->Filter.Allows(messages + position, messageWordCount))
                        {
                            continue;
                        }

                        auto const destinationGroup = target->DestinationGroupIndex == AllGroups
                            ? group
                            : target->DestinationGroupIndex;

                        if (target->SendBufferUsed + messageWordCount > target->SendBuffer.size())
                        {
                            Flush(*target, timestamp);
                        }

                        target->SendBuffer[target->SendBufferUsed] =
                            internal::GetFirstWordWithNewGroup(word0, static_cast<uint8_t>(destinationGroup));

                        for (uint8_t i = 1; i < messageWordCount; i++)
                        {
                            target->SendBuffer[target->SendBufferUsed + i] = messages[position + i];
                        }

                        // On this target's own copy, so one destination's transform cannot
                        // disturb what another destination is sent.
                        target->Transform.Apply(
                            target->SendBuffer.data() + target->SendBufferUsed, messageWordCount);

                        target->SendBufferUsed += messageWordCount;
                        target->SendBufferMessages++;
                    }
                }

                position += messageWordCount;
            }

            for (auto& target : Targets)
            {
                Flush(*target, timestamp);
            }
        }

        static void Flush(_Inout_ Target& target, _In_ uint64_t timestamp) noexcept
        {
            if (target.SendBufferUsed == 0 || target.Destination == nullptr)
            {
                return;
            }

            if (target.Queue != nullptr)
            {
                // Never waited for here. This thread also carries everything the source sends
                // to its other destinations.
                if (target.Queue->Push(timestamp, target.SendBuffer.data(), static_cast<uint32_t>(target.SendBufferUsed)))
                {
                    target.Sender->Wake();
                }
                else
                {
                    target.MessagesDropped.fetch_add(target.SendBufferMessages, std::memory_order_relaxed);
                }
            }
            else
            {
                target.MessagesForwarded.fetch_add(target.SendBufferMessages, std::memory_order_relaxed);

                if (FAILED(target.Destination->SendMidiMessagesRaw(
                    timestamp, static_cast<UINT32>(target.SendBufferUsed), target.SendBuffer.data())))
                {
                    target.SendFailures.fetch_add(1, std::memory_order_relaxed);
                }
            }

            target.SendBufferUsed = 0;
            target.SendBufferMessages = 0;
        }
    };

    RouteEngine& RouteEngine::Current() noexcept
    {
        static RouteEngine instance{};
        return instance;
    }

    _Use_decl_annotations_
    std::wstring RouteEngine::BuildSignature(std::vector<RoutePlanEntry> const& plan) noexcept
    {
        std::vector<std::wstring> parts{};
        parts.reserve(plan.size());

        for (auto const& entry : plan)
        {
            parts.push_back(
                LowerCopy(entry.SourceEndpointDeviceId) + L'|' +
                std::to_wstring(entry.SourceGroupIndex) + L'|' +
                LowerCopy(entry.DestinationEndpointDeviceId) + L'|' +
                std::to_wstring(entry.DestinationGroupIndex) + L'|' +
                FilterSignature(entry.Filter) + L'|' +
                TransformSignature(entry.Transform) + L'|' +
                std::to_wstring(entry.SendSpeedLimit) + L'|' +
                (entry.WaitForSendComplete ? L'w' : L'-'));
        }

        std::sort(parts.begin(), parts.end());

        std::wstring signature{};

        for (auto const& part : parts)
        {
            signature += part;
            signature += L'\n';
        }

        return signature;
    }

    void RouteEngine::TearDownLocked() noexcept
    {
        for (auto& hub : m_hubs)
        {
            if (hub == nullptr)
            {
                continue;
            }

            hub->Running.store(false);

            if (hub->SourceRaw != nullptr)
            {
                LOG_IF_FAILED(hub->SourceRaw->RemoveMessagesReceivedCallback());
            }
        }

        // No callback can add to a queue now. Each send thread finishes the send it is in, and
        // whatever is still queued goes with the plan. All are told first, so they stop together.
        for (auto& sender : m_senders)
        {
            if (sender != nullptr)
            {
                sender->RequestStop();
            }
        }

        m_senders.clear();

        m_hubs.clear();

        for (auto& [id, connection] : m_connections)
        {
            UNREFERENCED_PARAMETER(id);

            try
            {
                if (connection != nullptr && m_session != nullptr)
                {
                    m_session.DisconnectEndpointConnection(connection.ConnectionId());
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to disconnect an endpoint connection.")
        }

        m_connections.clear();
        m_activeRoutes = 0;
    }

    _Use_decl_annotations_
    void RouteEngine::Apply(std::vector<RoutePlanEntry> plan) noexcept
    {
        std::scoped_lock guard{ m_lock };

        try
        {
            auto const signature = BuildSignature(plan);

            if (signature == m_signature)
            {
                return;
            }

            TearDownLocked();

            m_signature = signature;
            m_lastError = {};

            if (plan.empty())
            {
                // nothing to route, so the session is dropped too rather than left holding
                // the service open for an idle app
                if (m_session != nullptr)
                {
                    m_session.Close();
                    m_session = nullptr;
                }

                return;
            }

            if (m_session == nullptr)
            {
                if (!midi2::MidiApi::EnsureServiceAvailable())
                {
                    m_lastError = resources::GetString(L"ErrorServiceUnavailable");
                    m_signature.clear();
                    return;
                }

                m_session = midi2::MidiSession::Create(SessionName);

                if (m_session == nullptr)
                {
                    m_lastError = resources::GetString(L"ErrorSessionFailed");
                    m_signature.clear();
                    return;
                }
            }

            // Pass one: work out which connections are needed and which of them are sources. The
            // COM extensions require the callback to be set before the connection is opened, so
            // that has to be known before anything is created.
            std::set<ConnectionKey> sourceKeys{};
            std::set<ConnectionKey> allKeys{};

            for (auto const& entry : plan)
            {
                if (entry.SourceEndpointDeviceId.empty() || entry.DestinationEndpointDeviceId.empty())
                {
                    continue;
                }

                ConnectionKey const sourceKey{ LowerCopy(entry.SourceEndpointDeviceId), entry.WaitForSendComplete };

                sourceKeys.insert(sourceKey);
                allKeys.insert(sourceKey);
                allKeys.insert(ConnectionKey{ LowerCopy(entry.DestinationEndpointDeviceId), entry.WaitForSendComplete });
            }

            // Pass two: create every connection, and give each source its hub before opening.
            std::map<ConnectionKey, winrt::com_ptr<SourceHub>> hubsByKey{};

            for (auto const& key : allKeys)
            {
                auto const& [id, waitForSendComplete] = key;

                // Settings only where a patch waits, so every other connection opens exactly as
                // it always has
                auto connection = waitForSendComplete
                    ? m_session.CreateEndpointConnection(winrt::hstring{ id }, midi2::MidiEndpointConnectionSettings{ true })
                    : m_session.CreateEndpointConnection(winrt::hstring{ id });

                if (connection == nullptr)
                {
                    // an endpoint that went away between planning and here is not an error:
                    // its routes simply stay idle until it comes back
                    MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(L"Endpoint connection could not be created.", id.c_str());
                    continue;
                }

                if (sourceKeys.count(key) != 0)
                {
                    auto raw = connection.try_as<IMidiEndpointConnectionRaw>();

                    if (raw == nullptr)
                    {
                        m_lastError = resources::GetString(L"ErrorNoComExtensions");
                        continue;
                    }

                    auto hub = winrt::make_self<SourceHub>();

                    hub->SourceEndpointDeviceId = id;
                    hub->SourceRaw = raw;

                    if (FAILED(raw->SetMessagesReceivedCallback(hub.get())))
                    {
                        m_lastError = resources::GetString(L"ErrorReceiveCallback");
                        continue;
                    }

                    hubsByKey.emplace(key, hub);
                }

                m_connections.emplace(key, connection);
            }

            // Pass three: build the targets, now that every destination connection exists.
            struct PendingTarget
            {
                SourceHub* Hub{ nullptr };
                ConnectionKey DestinationKey{};
                std::unique_ptr<Target> Route{};
            };

            std::vector<PendingTarget> pending{};
            auto const ticksPerSecond = internal::GetMidiTimestampFrequency();

            for (auto const& entry : plan)
            {
                ConnectionKey const sourceKey{ LowerCopy(entry.SourceEndpointDeviceId), entry.WaitForSendComplete };
                ConnectionKey const destinationKey{ LowerCopy(entry.DestinationEndpointDeviceId), entry.WaitForSendComplete };

                auto const hubIt = hubsByKey.find(sourceKey);
                auto const destinationIt = m_connections.find(destinationKey);

                if (hubIt == hubsByKey.end() || destinationIt == m_connections.end())
                {
                    continue;
                }

                auto destinationRaw = destinationIt->second.try_as<IMidiEndpointConnectionRaw>();

                if (destinationRaw == nullptr)
                {
                    m_lastError = resources::GetString(L"ErrorNoComExtensions");
                    continue;
                }

                // One batch never exceeds what this destination takes in a single call, so the
                // send path can split on that boundary without ever reallocating. A buffer
                // shorter than the longest UMP could not hold even one message.
                auto const maxWords = destinationRaw->GetSupportedMaxMidiWordsPerTransmission();

                if (maxWords < MaximumWordsPerUmp)
                {
                    continue;
                }

                auto target = std::make_unique<Target>();

                target->Destination = destinationRaw;
                target->SourceGroupIndex = entry.SourceGroupIndex;
                target->DestinationGroupIndex = entry.DestinationGroupIndex;
                target->Filter = entry.Filter;
                target->Transform = entry.Transform;
                target->ConnectionId = entry.ConnectionId;
                target->SendBuffer.assign(maxWords, 0);

                // After the filter and the transform, so the speed is spent only on what is sent
                if (entry.SendSpeedLimit != 0 || entry.WaitForSendComplete)
                {
                    target->Queue = std::make_unique<SendQueue>();
                    target->Queue->Configure(entry.SendSpeedLimit, ticksPerSecond);
                }

                pending.push_back(PendingTarget{ hubIt->second.get(), destinationKey, std::move(target) });
            }

            // Pass four: a send thread for each destination with connections that hold messages
            // back. A connection whose thread cannot start is left out, rather than left to fill
            // a queue that nothing empties.
            std::map<ConnectionKey, DestinationSender*> sendersByKey{};

            for (auto& item : pending)
            {
                if (item.Route->Queue == nullptr)
                {
                    continue;
                }

                auto& sender = sendersByKey[item.DestinationKey];

                if (sender == nullptr)
                {
                    m_senders.push_back(std::make_unique<DestinationSender>());

                    sender = m_senders.back().get();
                    sender->Destination = item.Route->Destination;
                }

                sender->Targets.push_back(item.Route.get());
                item.Route->Sender = sender;
            }

            for (auto& sender : m_senders)
            {
                if (!sender->Start())
                {
                    m_lastError = resources::GetString(L"ErrorRoutingFailed");
                }
            }

            size_t activeRoutes{ 0 };

            for (auto& item : pending)
            {
                if (item.Route->Sender != nullptr && !item.Route->Sender->IsRunning())
                {
                    continue;
                }

                item.Hub->Targets.push_back(std::move(item.Route));

                activeRoutes++;
            }

            std::erase_if(m_senders, [](std::unique_ptr<DestinationSender> const& sender)
                {
                    return !sender->IsRunning();
                });

            // Pass five: open. Every callback is already in place, so no message can arrive
            // before the route it belongs to exists.
            for (auto& [key, connection] : m_connections)
            {
                if (!connection.Open())
                {
                    MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(L"Endpoint connection could not be opened.", key.first.c_str());
                }
            }

            for (auto& [key, hub] : hubsByKey)
            {
                UNREFERENCED_PARAMETER(key);

                hub->Running.store(true);
                m_hubs.push_back(hub);
            }

            m_activeRoutes = activeRoutes;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_PATCHBAY_LOG_HRESULT_EXCEPTION(ex, L"Unable to apply the routing plan.");
            m_lastError = resources::GetString(L"ErrorRoutingFailed");
            m_signature.clear();
        }
        catch (...)
        {
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to apply the routing plan.");
            m_lastError = resources::GetString(L"ErrorRoutingFailed");
            m_signature.clear();
        }
    }

    void RouteEngine::Shutdown() noexcept
    {
        std::scoped_lock guard{ m_lock };

        try
        {
            TearDownLocked();

            if (m_session != nullptr)
            {
                m_session.Close();
                m_session = nullptr;
            }

            m_signature.clear();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to shut down the routing engine.")
    }

    RouteEngine::~RouteEngine() noexcept
    {
        // The window shuts the engine down on a thread of its own, which can still be running
        // when the process exits. This waits for it, then stops anything it left.
        Shutdown();
    }

    std::unordered_map<std::wstring, RouteStats> RouteEngine::Stats() const noexcept
    {
        std::unordered_map<std::wstring, RouteStats> result{};

        std::scoped_lock guard{ m_lock };

        for (auto const& hub : m_hubs)
        {
            if (hub == nullptr)
            {
                continue;
            }

            for (auto const& target : hub->Targets)
            {
                if (target == nullptr)
                {
                    continue;
                }

                RouteStats stats{};

                stats.MessagesForwarded = target->MessagesForwarded.load(std::memory_order_relaxed);
                stats.SendFailures = target->SendFailures.load(std::memory_order_relaxed);
                stats.MessagesWaiting = target->Queue == nullptr ? 0 : target->Queue->WaitingMessageCount();
                stats.MessagesDropped = target->MessagesDropped.load(std::memory_order_relaxed);
                stats.IsActive = true;

                result[target->ConnectionId] = stats;
            }
        }

        return result;
    }

    winrt::hstring RouteEngine::LastErrorMessage() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_lastError;
    }

    size_t RouteEngine::ActiveRouteCount() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_activeRoutes;
    }

    _Use_decl_annotations_
    bool RouteEngine::SendTestNote(
        std::wstring const& endpointDeviceId,
        int32_t groupIndex,
        uint8_t noteIndex) noexcept
    {
        try
        {
            if (endpointDeviceId.empty())
            {
                return false;
            }

            auto const group = static_cast<uint32_t>(groupIndex == AllGroups ? 0 : std::clamp(groupIndex, 0, 15));
            auto const note = static_cast<uint32_t>(noteIndex & 0x7F);

            // MIDI 1.0 channel voice on channel 1: the one shape every endpoint understands,
            // whether or not it is natively UMP.
            auto const noteOn = (2u << 28) | (group << 24) | (0x9u << 20) | (note << 8) | 100u;
            auto const noteOff = (2u << 28) | (group << 24) | (0x8u << 20) | (note << 8) | 0u;

            midi2::MidiSession temporarySession{ nullptr };
            midi2::MidiEndpointConnection temporaryConnection{ nullptr };
            winrt::com_ptr<IMidiEndpointConnectionRaw> raw{ nullptr };

            {
                std::scoped_lock guard{ m_lock };

                auto const id = LowerCopy(endpointDeviceId);

                for (auto const waitsForSendComplete : { false, true })
                {
                    auto const it = m_connections.find(ConnectionKey{ id, waitsForSendComplete });

                    if (it != m_connections.end() && it->second != nullptr)
                    {
                        raw = it->second.try_as<IMidiEndpointConnectionRaw>();
                        break;
                    }
                }
            }

            if (raw == nullptr)
            {
                if (!midi2::MidiApi::EnsureServiceAvailable())
                {
                    return false;
                }

                temporarySession = midi2::MidiSession::Create(SessionName);

                if (temporarySession == nullptr)
                {
                    return false;
                }

                temporaryConnection = temporarySession.CreateEndpointConnection(winrt::hstring{ endpointDeviceId });

                if (temporaryConnection == nullptr || !temporaryConnection.Open())
                {
                    temporarySession.Close();
                    return false;
                }

                raw = temporaryConnection.try_as<IMidiEndpointConnectionRaw>();
            }

            auto const closeTemporary = wil::scope_exit([&temporarySession]()
                {
                    if (temporarySession != nullptr)
                    {
                        temporarySession.Close();
                    }
                });

            if (raw == nullptr)
            {
                return false;
            }

            auto sent = SUCCEEDED(raw->SendMidiMessagesRaw(0, 1, &noteOn));

            std::this_thread::sleep_for(std::chrono::milliseconds{ TestNoteMilliseconds });

            sent = SUCCEEDED(raw->SendMidiMessagesRaw(0, 1, &noteOff)) && sent;

            return sent;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to play the test note.")

        return false;
    }
}
