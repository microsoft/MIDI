// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RouteEngine.h"
#include "StringResources.h"

#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t MaximumWordsPerUmp = 4;

        constexpr wchar_t SessionName[] = L"MIDI Patchbay";

        std::wstring LowerCopy(_In_ std::wstring value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }
    }

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

                        target->SendBufferUsed += messageWordCount;

                        target->MessagesForwarded.fetch_add(1, std::memory_order_relaxed);
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

            if (FAILED(target.Destination->SendMidiMessagesRaw(
                timestamp, static_cast<UINT32>(target.SendBufferUsed), target.SendBuffer.data())))
            {
                target.SendFailures.fetch_add(1, std::memory_order_relaxed);
            }

            target.SendBufferUsed = 0;
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
                std::to_wstring(entry.DestinationGroupIndex));
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

            // Pass one: work out which endpoints are needed and which of them are sources. The
            // COM extensions require the callback to be set before the connection is opened, so
            // that has to be known before anything is created.
            std::set<std::wstring> sourceIds{};
            std::set<std::wstring> allIds{};

            for (auto const& entry : plan)
            {
                if (entry.SourceEndpointDeviceId.empty() || entry.DestinationEndpointDeviceId.empty())
                {
                    continue;
                }

                sourceIds.insert(LowerCopy(entry.SourceEndpointDeviceId));
                allIds.insert(LowerCopy(entry.SourceEndpointDeviceId));
                allIds.insert(LowerCopy(entry.DestinationEndpointDeviceId));
            }

            // Pass two: create every connection, and give each source its hub before opening.
            std::map<std::wstring, winrt::com_ptr<SourceHub>> hubsById{};

            for (auto const& id : allIds)
            {
                auto connection = m_session.CreateEndpointConnection(winrt::hstring{ id });

                if (connection == nullptr)
                {
                    // an endpoint that went away between planning and here is not an error:
                    // its routes simply stay idle until it comes back
                    MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(L"Endpoint connection could not be created.", id.c_str());
                    continue;
                }

                if (sourceIds.count(id) != 0)
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

                    hubsById.emplace(id, hub);
                }

                m_connections.emplace(id, connection);
            }

            // Pass three: attach the targets, now that every destination connection exists.
            size_t activeRoutes{ 0 };

            for (auto const& entry : plan)
            {
                auto const sourceId = LowerCopy(entry.SourceEndpointDeviceId);
                auto const destinationId = LowerCopy(entry.DestinationEndpointDeviceId);

                auto const hubIt = hubsById.find(sourceId);
                auto const destinationIt = m_connections.find(destinationId);

                if (hubIt == hubsById.end() || destinationIt == m_connections.end())
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
                target->ConnectionId = entry.ConnectionId;
                target->SendBuffer.assign(maxWords, 0);

                hubIt->second->Targets.push_back(std::move(target));

                activeRoutes++;
            }

            // Pass four: open. Every callback is already in place, so no message can arrive
            // before the route it belongs to exists.
            for (auto& [id, connection] : m_connections)
            {
                UNREFERENCED_PARAMETER(id);

                if (!connection.Open())
                {
                    MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(L"Endpoint connection could not be opened.", id.c_str());
                }
            }

            for (auto& [id, hub] : hubsById)
            {
                UNREFERENCED_PARAMETER(id);

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
}
