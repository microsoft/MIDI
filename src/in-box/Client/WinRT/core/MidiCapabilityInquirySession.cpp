// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiCapabilityInquirySession.h"

// Consumed here, so the implementation headers are needed as well as the projections.
#include "MidiUniqueId.h"
#include "MidiProfileId.h"
#include "MidiCapabilityInquiryMessage.h"
#include "MidiCapabilityInquiryMessageBuilder.h"
#include "MidiCapabilityInquiryResponder.h"
#include "MidiCapabilityInquiryMessageReceivedEventArgs.h"
#include "MidiPropertyExchangeResponse.h"
#include "MidiProfileInquiryResponse.h"
#include "MidiDeclaredDeviceIdentity.h"
#include "MidiGroup.h"

#include "MidiCiMessage.h"

#include "CapabilityInquiry.MidiCapabilityInquirySession.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        namespace native = ::WindowsMidiServicesCapabilityInquiry;

        // The resource names every device that implements property exchange is expected to know.
        constexpr std::wstring_view ResourceKey{ L"resource" };
        constexpr std::wstring_view ResourceIdKey{ L"resId" };
        constexpr std::wstring_view OffsetKey{ L"offset" };
        constexpr std::wstring_view LimitKey{ L"limit" };
        constexpr std::wstring_view StatusKey{ L"status" };
        constexpr std::wstring_view TotalCountKey{ L"totalCount" };
        constexpr std::wstring_view MutualEncodingKey{ L"mutualEncoding" };
        constexpr std::wstring_view MessageKey{ L"message" };

        constexpr std::wstring_view ResourceDeviceInfo{ L"DeviceInfo" };
        constexpr std::wstring_view ResourceResourceList{ L"ResourceList" };
        constexpr std::wstring_view ResourceChannelList{ L"ChannelList" };
        constexpr std::wstring_view ResourceProgramList{ L"ProgramList" };

        // Large enough that a modest list arrives in one request, small enough that the first page
        // of a workstation sized list comes back quickly.
        constexpr uint32_t DefaultPageSize = 100;

        // A device which keeps answering with entries but never reports a total would otherwise
        // page forever.
        constexpr uint32_t MaximumPages = 2000;

        // The status a device puts in a reply header when it answered the request.
        constexpr int32_t ResourceStatusOk = 200;

        json::JsonObject MakeResourceHeader(
            _In_ std::wstring_view const resource,
            _In_ winrt::hstring const& resourceId) noexcept
        {
            try
            {
                json::JsonObject header{};

                header.SetNamedValue(
                    winrt::hstring{ ResourceKey },
                    json::JsonValue::CreateStringValue(winrt::hstring{ resource }));

                if (!resourceId.empty())
                {
                    header.SetNamedValue(
                        winrt::hstring{ ResourceIdKey },
                        json::JsonValue::CreateStringValue(resourceId));
                }

                return header;
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return nullptr;
            }
        }

        double SafeGetNamedNumber(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view const key,
            _In_ double const defaultValue) noexcept
        {
            try
            {
                if (object == nullptr)
                {
                    return defaultValue;
                }

                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return defaultValue;
                }

                auto const value = object.Lookup(name);

                // A device is free to send the wrong type. Treating that as absent keeps one bad
                // key from discarding an otherwise usable reply.
                if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                {
                    return defaultValue;
                }

                return value.GetNumber();
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return defaultValue;
            }
        }

        winrt::hstring SafeGetNamedString(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view const key) noexcept
        {
            try
            {
                if (object == nullptr)
                {
                    return L"";
                }

                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return L"";
                }

                auto const value = object.Lookup(name);

                if (value == nullptr || value.ValueType() != json::JsonValueType::String)
                {
                    return L"";
                }

                return value.GetString();
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return L"";
            }
        }
    }


    _Use_decl_annotations_
    ci::MidiCapabilityInquirySession MidiCapabilityInquirySession::Create(
        midi2::MidiEndpointConnection const& connection) noexcept
    {
        return Create(connection, nullptr);
    }

    _Use_decl_annotations_
    ci::MidiCapabilityInquirySession MidiCapabilityInquirySession::Create(
        midi2::MidiEndpointConnection const& connection,
        midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept
    {
        try
        {
            if (connection == nullptr)
            {
                return nullptr;
            }

            auto session = winrt::make_self<MidiCapabilityInquirySession>();

            if (!session->InternalInitialize(connection, identity))
            {
                return nullptr;
            }

            return *session;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquirySession::InternalInitialize(
        midi2::MidiEndpointConnection const& connection,
        midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept
    {
        try
        {
            m_connection = connection;
            m_identity = identity;

            // A new identifier every time. The specification requires that one must not survive a
            // restart, so it must not be derived from anything stable about this machine.
            m_sourceMuid = ci::MidiUniqueId::CreateRandom();
            m_sourceMuidValue = m_sourceMuid == nullptr ? 0 : m_sourceMuid.AsCombined28BitValue();

            m_group = midi2::MidiGroup((uint8_t)0);

            m_messageReceivedToken = m_connection.MessageReceived(
                { this, &MidiCapabilityInquirySession::OnMessageReceived });

            m_isOpen = true;

            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    void MidiCapabilityInquirySession::Close() noexcept
    {
        if (!m_isOpen.exchange(false))
        {
            return;
        }

        try
        {
            // Release the identifier before letting go of the connection, so anything that was
            // talking to this session knows to stop.
            SendInvalidateMuid();

            if (m_connection != nullptr && m_messageReceivedToken.value != 0)
            {
                m_connection.MessageReceived(m_messageReceivedToken);
                m_messageReceivedToken = {};
            }

            m_connection = nullptr;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        // Anything still waiting has to come back rather than sit out its timeout.
        {
            std::lock_guard<std::mutex> guard(m_lock);
            m_pendingRequests.clear();
        }

        m_requestCompleted.notify_all();
    }

    midi2::MidiGroup MidiCapabilityInquirySession::Group() const noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        return m_group;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::Group(midi2::MidiGroup const& value) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);

        if (value != nullptr)
        {
            m_group = value;
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::ResponseTimeoutMilliseconds(uint32_t const value) noexcept
    {
        // A timeout of zero would give a device no chance at all to answer.
        m_responseTimeoutMilliseconds = value == 0 ? 1 : value;
    }

    midi2enum::MidiDeclaredDeviceIdentity MidiCapabilityInquirySession::Identity() const noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        return m_identity;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::Identity(midi2enum::MidiDeclaredDeviceIdentity const& value) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_identity = value;
    }

    foundation::Collections::IVectorView<ci::MidiCapabilityInquiryResponder>
    MidiCapabilityInquirySession::GetResponders()
    {
        auto responders = winrt::single_threaded_vector<ci::MidiCapabilityInquiryResponder>();

        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            for (auto const& entry : m_responders)
            {
                responders.Append(entry.second);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return responders.GetView();
    }

    _Use_decl_annotations_
    ci::MidiCapabilityInquiryResponder MidiCapabilityInquirySession::GetResponder(
        ci::MidiUniqueId const& muid) noexcept
    {
        try
        {
            if (muid == nullptr)
            {
                return nullptr;
            }

            std::lock_guard<std::mutex> guard(m_lock);

            auto const entry = m_responders.find(muid.AsCombined28BitValue());

            return entry == m_responders.end() ? nullptr : entry->second;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    uint8_t MidiCapabilityInquirySession::NextRequestId() noexcept
    {
        // Request ids are seven bit and zero is a legal value, but keeping away from it makes a
        // stray zero in a reply obvious rather than plausible.
        auto value = m_nextRequestId.fetch_add(1) & 0x7F;

        if (value == 0)
        {
            value = m_nextRequestId.fetch_add(1) & 0x7F;
        }

        return static_cast<uint8_t>(value);
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquirySession::Send(
        foundation::Collections::IVector<midi2::MidiMessage64> const& messages) noexcept
    {
        try
        {
            if (!m_isOpen || m_connection == nullptr)
            {
                return false;
            }

            if (messages == nullptr || messages.Size() == 0)
            {
                return false;
            }

            auto packets = winrt::single_threaded_vector<midi2::IMidiUniversalPacket>();

            for (auto const& message : messages)
            {
                packets.Append(message);
            }

            auto const result = m_connection.SendMultipleMessagesPacketList(packets);

            return midi2::MidiEndpointConnection::SendMessageSucceeded(result);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    uint32_t MidiCapabilityInquirySession::MaximumSystemExclusiveSizeFor(uint32_t const muid) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);

        auto const entry = m_responders.find(muid);

        if (entry == m_responders.end())
        {
            return 0;
        }

        return entry->second.ReceivableMaximumSystemExclusiveSize();
    }


    // ---------------------------------------------------------------- receiving

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::OnMessageReceived(
        foundation::IInspectable const& /*sender*/,
        midi2::MidiMessageReceivedEventArgs const& args) noexcept
    {
        try
        {
            if (!m_isOpen || args == nullptr)
            {
                return;
            }

            uint32_t word0{};
            uint32_t word1{};
            uint32_t word2{};
            uint32_t word3{};

            auto const wordCount = args.FillWords(word0, word1, word2, word3);

            // Capability inquiry travels as seven bit system exclusive, which is always two words.
            if (wordCount != 2)
            {
                return;
            }

            if (((word0 >> 28) & 0x0F) != 0x03)
            {
                return;
            }

            auto const group = midi2::MidiGroup(static_cast<uint8_t>((word0 >> 24) & 0x0F));

            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (m_group != nullptr && group.Index() != m_group.Index())
                {
                    return;
                }
            }

            auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);
            auto const byteCount = static_cast<uint8_t>((word0 >> 16) & 0x0F);

            if (byteCount > 6)
            {
                return;
            }

            const uint8_t bytes[6]
            {
                static_cast<uint8_t>((word0 >> 8) & 0x7F),
                static_cast<uint8_t>(word0 & 0x7F),
                static_cast<uint8_t>((word1 >> 24) & 0x7F),
                static_cast<uint8_t>((word1 >> 16) & 0x7F),
                static_cast<uint8_t>((word1 >> 8) & 0x7F),
                static_cast<uint8_t>(word1 & 0x7F),
            };

            std::vector<uint8_t> completed{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (status == 0x0 || status == 0x1)
                {
                    // A start that arrives while another transfer is open means the previous one
                    // was abandoned. Keeping its bytes would corrupt this one.
                    m_incoming.clear();
                    m_incomingIsOpen = true;
                }
                else if (!m_incomingIsOpen)
                {
                    // A continue or an end with no start. Nothing can be made of it.
                    return;
                }

                for (uint8_t i = 0; i < byteCount; i++)
                {
                    m_incoming.push_back(bytes[i]);
                }

                if (status == 0x0 || status == 0x3)
                {
                    completed.swap(m_incoming);
                    m_incomingIsOpen = false;
                }
            }

            if (!completed.empty())
            {
                HandleCompleteTransfer(completed, group, args.Timestamp());
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::HandleCompleteTransfer(
        std::vector<uint8_t> const& payload,
        midi2::MidiGroup const& group,
        internal::MidiTimestamp const timestamp) noexcept
    {
        try
        {
            auto data = winrt::single_threaded_vector<uint8_t>();

            for (auto const value : payload)
            {
                data.Append(value);
            }

            if (!ci::MidiCapabilityInquiryMessage::IsCapabilityInquiryData(data))
            {
                return;
            }

            auto const message = ci::MidiCapabilityInquiryMessage::FromSystemExclusiveData(data);

            if (message == nullptr || !message.IsValid())
            {
                return;
            }

            // Traffic between two other devices on the same cable is not ours to interpret.
            auto const destination = message.DestinationMuid();

            if (destination != nullptr &&
                !destination.IsBroadcast() &&
                destination.AsCombined28BitValue() != m_sourceMuidValue)
            {
                return;
            }

            if (message.MessageType() == ci::MidiCapabilityInquiryMessageType::DiscoveryReply ||
                message.MessageType() == ci::MidiCapabilityInquiryMessageType::Discovery)
            {
                RecordResponder(message);
            }

            if (TryCompleteRequest(message))
            {
                return;
            }

            auto const args = winrt::make<MidiCapabilityInquiryMessageReceivedEventArgs>(
                message, group, timestamp);

            switch (message.MessageType())
            {
            case ci::MidiCapabilityInquiryMessageType::ProfileAddedReport:
            case ci::MidiCapabilityInquiryMessageType::ProfileRemovedReport:
            case ci::MidiCapabilityInquiryMessageType::ProfileEnabledReport:
            case ci::MidiCapabilityInquiryMessageType::ProfileDisabledReport:
                m_profileStateChangedEvent(*this, args);
                break;

            default:
                break;
            }

            m_messageReceivedEvent(*this, args);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::RecordResponder(
        ci::MidiCapabilityInquiryMessage const& message) noexcept
    {
        try
        {
            auto const sourceMuid = message.SourceMuid();

            if (sourceMuid == nullptr)
            {
                return;
            }

            auto const muidValue = sourceMuid.AsCombined28BitValue();

            // A device answering its own Discovery would be us hearing our own message come back.
            if (muidValue == m_sourceMuidValue)
            {
                return;
            }

            auto const data = message.Data();

            // Manufacturer, family, model, revision, categories, size, path and block, in the order
            // the message table puts them after the thirteen byte common header.
            if (data == nullptr || data.Size() < 30)
            {
                return;
            }

            std::array<uint8_t, 32> raw{};

            for (uint32_t i = 0; i < 30 && i < data.Size(); i++)
            {
                raw[i] = data.GetAt(i);
            }

            auto const identity = winrt::make<midi2enum::implementation::MidiDeclaredDeviceIdentity>(
                raw[13], raw[14], raw[15],
                raw[16], raw[17],
                raw[18], raw[19],
                raw[20], raw[21], raw[22], raw[23]);

            auto responder = winrt::make_self<MidiCapabilityInquiryResponder>();

            responder->InternalSetMuid(sourceMuid);
            responder->InternalSetIdentity(identity);
            responder->InternalSetMessageVersion(message.SourceVersion());
            responder->InternalSetCategories(
                static_cast<ci::MidiCapabilityInquiryCategories>(raw[24]));

            responder->InternalSetMaximumSystemExclusiveSize(native::ReadMuid(raw.data() + 25));
            responder->InternalSetOutputPathId(raw[29]);

            if (data.Size() >= 31)
            {
                responder->InternalSetFunctionBlockNumber(data.GetAt(30));
            }

            bool isNew{ false };

            {
                std::lock_guard<std::mutex> guard(m_lock);

                isNew = m_responders.find(muidValue) == m_responders.end();
                m_responders.insert_or_assign(muidValue, *responder);
            }

            if (isNew)
            {
                m_responderFoundEvent(*this, *responder);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquirySession::TryCompleteRequest(
        ci::MidiCapabilityInquiryMessage const& message) noexcept
    {
        try
        {
            auto const sourceMuid = message.SourceMuid();

            if (sourceMuid == nullptr)
            {
                return false;
            }

            auto const sourceValue = sourceMuid.AsCombined28BitValue();
            auto const messageType = message.MessageType();

            bool completed{ false };

            {
                std::lock_guard<std::mutex> guard(m_lock);

                for (auto& entry : m_pendingRequests)
                {
                    auto& request = entry.second;

                    if (request.IsComplete || request.DestinationMuid != sourceValue)
                    {
                        continue;
                    }

                    if (messageType == ci::MidiCapabilityInquiryMessageType::Nak)
                    {
                        // A negative acknowledgment names the message it answers, so it can be
                        // matched even when several requests are outstanding to the same device.
                        if (message.HasAcknowledgmentFields() &&
                            message.StatusDetails().Size() > 0 &&
                            message.HasPropertyExchangeFields() == false &&
                            request.RequestId != 0 &&
                            message.StatusDetails().GetAt(0) != request.RequestId &&
                            static_cast<uint8_t>(message.OriginalMessageType()) >= 0x30 &&
                            static_cast<uint8_t>(message.OriginalMessageType()) <= 0x3F)
                        {
                            continue;
                        }

                        request.FirstMessage = message;
                        request.IsComplete = true;
                        completed = true;
                        break;
                    }

                    if (messageType != request.ExpectedReply)
                    {
                        continue;
                    }

                    if (message.HasPropertyExchangeFields())
                    {
                        if (message.RequestId() != request.RequestId)
                        {
                            continue;
                        }

                        if (request.ChunksReceived == 0)
                        {
                            request.FirstMessage = message;
                        }

                        request.ChunksReceived++;

                        if (message.ChunkCount() != 0)
                        {
                            request.ChunkCount = message.ChunkCount();
                        }

                        auto const body = message.Body();

                        for (auto const value : body)
                        {
                            request.Body.push_back(value);
                        }

                        // A chunk numbered zero is how a device says the data it was sending is no
                        // longer good. The transfer ends, and what arrived cannot be trusted.
                        if (message.ChunkNumber() == 0)
                        {
                            request.Body.clear();
                            request.IsComplete = true;
                            completed = true;
                            break;
                        }

                        if (message.ChunkCount() != 0 && message.ChunkNumber() >= message.ChunkCount())
                        {
                            request.IsComplete = true;
                            completed = true;
                        }
                        else
                        {
                            // More to come. Waking the waiter anyway is what gives each chunk its
                            // own share of the timeout rather than the whole transfer.
                            completed = true;
                        }

                        break;
                    }

                    if (messageType == ci::MidiCapabilityInquiryMessageType::ProfileInquiryReply &&
                        message.DeviceId() != request.DeviceId)
                    {
                        continue;
                    }

                    request.FirstMessage = message;
                    request.IsComplete = true;
                    completed = true;
                    break;
                }
            }

            if (completed)
            {
                m_requestCompleted.notify_all();
            }

            return completed;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquirySession::WaitForRequest(uint64_t const key) noexcept
    {
        try
        {
            std::unique_lock<std::mutex> guard(m_lock);

            uint16_t lastProgress{ 0 };

            for (;;)
            {
                auto const timeout = std::chrono::milliseconds(m_responseTimeoutMilliseconds.load());

                auto const signaled = m_requestCompleted.wait_for(guard, timeout, [this, key, lastProgress]()
                {
                    if (!m_isOpen)
                    {
                        return true;
                    }

                    auto const entry = m_pendingRequests.find(key);

                    if (entry == m_pendingRequests.end())
                    {
                        return true;
                    }

                    return entry->second.IsComplete || entry->second.ChunksReceived != lastProgress;
                });

                auto const entry = m_pendingRequests.find(key);

                if (entry == m_pendingRequests.end())
                {
                    return false;
                }

                if (entry->second.IsComplete)
                {
                    return true;
                }

                if (!signaled)
                {
                    // Nothing arrived within the timeout. A device that is going to answer has had
                    // its chance, and a chunked transfer gets the same allowance for every chunk.
                    return false;
                }

                lastProgress = entry->second.ChunksReceived;
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::RemoveRequest(uint64_t const key) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);
            m_pendingRequests.erase(key);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }


    // ---------------------------------------------------------------- discovery

    bool MidiCapabilityInquirySession::SendInvalidateMuid() noexcept
    {
        try
        {
            return Send(MidiCapabilityInquiryMessageBuilder::BuildInvalidateMuid(
                0, Group(), m_sourceMuid, m_sourceMuid));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    foundation::IAsyncOperation<foundation::Collections::IVectorView<ci::MidiCapabilityInquiryResponder>>
    MidiCapabilityInquirySession::DiscoverAsync()
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            Send(MidiCapabilityInquiryMessageBuilder::BuildDiscovery(
                0,
                Group(),
                m_sourceMuid,
                Identity(),
                ci::MidiCapabilityInquiryCategories::PropertyExchange |
                    ci::MidiCapabilityInquiryCategories::ProfileConfiguration,
                MidiCapabilityInquiryMessageBuilder::MinimumReceivableSystemExclusiveSize(),
                0));

            // There is no way to know how many devices are out there, so this waits the whole
            // timeout rather than returning as soon as one answers.
            std::unique_lock<std::mutex> guard(m_lock);

            m_requestCompleted.wait_for(
                guard,
                std::chrono::milliseconds(m_responseTimeoutMilliseconds.load()),
                [this]() { return !m_isOpen; });
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        co_return GetResponders();
    }


    // -------------------------------------------------------- property exchange

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiCapabilityInquiryStatus>
    MidiCapabilityInquirySession::RequestPropertyExchangeCapabilitiesAsync(
        ci::MidiUniqueId destinationMuid)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            if (destinationMuid == nullptr || !m_isOpen)
            {
                co_return ci::MidiCapabilityInquiryStatus::Failed;
            }

            auto const destinationValue = destinationMuid.AsCombined28BitValue();

            PendingRequest request{};

            request.ExpectedReply = ci::MidiCapabilityInquiryMessageType::PropertyExchangeCapabilitiesInquiryReply;
            request.DestinationMuid = destinationValue;

            uint64_t key{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                key = reinterpret_cast<uint64_t>(this) ^ (static_cast<uint64_t>(destinationValue) << 8) ^ m_nextRequestId.load();

                while (m_pendingRequests.find(key) != m_pendingRequests.end())
                {
                    key++;
                }

                m_pendingRequests[key] = request;
            }

            auto const sent = Send(
                MidiCapabilityInquiryMessageBuilder::BuildPropertyExchangeCapabilitiesInquiry(
                    0, Group(), m_sourceMuid, destinationMuid, 1));

            if (!sent)
            {
                RemoveRequest(key);
                co_return ci::MidiCapabilityInquiryStatus::Failed;
            }

            auto const answered = WaitForRequest(key);

            ci::MidiCapabilityInquiryMessage reply{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const entry = m_pendingRequests.find(key);

                if (entry != m_pendingRequests.end())
                {
                    reply = entry->second.FirstMessage;
                }
            }

            RemoveRequest(key);

            if (!answered || reply == nullptr)
            {
                co_return ci::MidiCapabilityInquiryStatus::NoResponse;
            }

            if (reply.MessageType() == ci::MidiCapabilityInquiryMessageType::Nak)
            {
                co_return ci::MidiCapabilityInquiryStatus::NegativeAcknowledgment;
            }

            // The count is the byte immediately after the common header.
            auto const data = reply.Data();

            if (data != nullptr && data.Size() > 13)
            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const responder = m_responders.find(destinationValue);

                if (responder != m_responders.end())
                {
                    winrt::get_self<MidiCapabilityInquiryResponder>(responder->second)
                        ->InternalSetMaximumSimultaneousPropertyRequests(data.GetAt(13));
                }
            }

            co_return ci::MidiCapabilityInquiryStatus::Success;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            co_return ci::MidiCapabilityInquiryStatus::Failed;
        }
    }

    _Use_decl_annotations_
    ci::MidiPropertyExchangeResponse MidiCapabilityInquirySession::BuildPropertyResponse(
        PendingRequest const& request,
        bool const timedOut) noexcept
    {
        auto response = winrt::make_self<MidiPropertyExchangeResponse>();

        try
        {
            response->InternalSetRequestId(request.RequestId);

            if (timedOut || request.FirstMessage == nullptr)
            {
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::NoResponse);
                return *response;
            }

            auto const& message = request.FirstMessage;

            response->InternalSetResponderMuid(message.SourceMuid());

            if (message.MessageType() == ci::MidiCapabilityInquiryMessageType::Nak)
            {
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::NegativeAcknowledgment);
                response->InternalSetNakStatusCode(message.StatusCode());
                response->InternalSetNakStatusMessage(message.StatusMessage());

                return *response;
            }

            auto const header = message.Header();

            response->InternalSetHeader(header);
            response->InternalSetHeaderText(message.HeaderText());
            response->InternalSetChunkCount(request.ChunkCount == 0 ? request.ChunksReceived : request.ChunkCount);

            auto const resourceStatus = static_cast<int32_t>(
                SafeGetNamedNumber(header, StatusKey, ResourceStatusOk));

            response->InternalSetResourceStatus(resourceStatus);

            auto body = winrt::single_threaded_vector<uint8_t>();

            for (auto const value : request.Body)
            {
                body.Append(value);
            }

            response->InternalSetBody(body);

            // A device may say it encoded the data. Nothing here decodes those, so the text and
            // the parsed forms are left empty rather than filled with something wrong.
            auto const encoding = SafeGetNamedString(header, MutualEncodingKey);
            auto const isPlainText = encoding.empty() || encoding == L"ASCII";

            if (isPlainText && !request.Body.empty())
            {
                std::wstring text{};
                text.reserve(request.Body.size());

                for (auto const value : request.Body)
                {
                    text.push_back(static_cast<wchar_t>(value));
                }

                winrt::hstring const bodyText{ text };

                response->InternalSetBodyAsText(bodyText);

                json::JsonValue parsed{ nullptr };

                if (json::JsonValue::TryParse(bodyText, parsed))
                {
                    response->InternalSetBodyAsJson(parsed);
                }
            }

            if (resourceStatus == ResourceStatusOk)
            {
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Success);
            }
            else
            {
                // The device answered and declined. Its own message, when it sent one, says why.
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::NegativeAcknowledgment);
                response->InternalSetNakStatusMessage(SafeGetNamedString(header, MessageKey));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::InvalidResponse);
        }

        return *response;
    }

    _Use_decl_annotations_
    ci::MidiPropertyExchangeResponse MidiCapabilityInquirySession::RequestProperty(
        ci::MidiUniqueId const& destinationMuid,
        json::JsonObject const& header,
        foundation::Collections::IIterable<uint8_t> const& body,
        bool const isSet) noexcept
    {
        try
        {
            if (destinationMuid == nullptr || header == nullptr || !m_isOpen)
            {
                auto failed = winrt::make_self<MidiPropertyExchangeResponse>();
                failed->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Failed);
                return *failed;
            }

            auto const destinationValue = destinationMuid.AsCombined28BitValue();
            auto const requestId = NextRequestId();

            PendingRequest request{};

            request.ExpectedReply = isSet
                ? ci::MidiCapabilityInquiryMessageType::PropertySetDataInquiryReply
                : ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiryReply;

            request.DestinationMuid = destinationValue;
            request.RequestId = requestId;

            uint64_t key{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                key = (static_cast<uint64_t>(destinationValue) << 8) | requestId;

                while (m_pendingRequests.find(key) != m_pendingRequests.end())
                {
                    key += 0x100000000ull;
                }

                m_pendingRequests[key] = request;
            }

            auto const messages = MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
                0,
                Group(),
                isSet
                    ? ci::MidiCapabilityInquiryMessageType::PropertySetDataInquiry
                    : ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry,
                m_sourceMuid,
                destinationMuid,
                requestId,
                header,
                body,
                MaximumSystemExclusiveSizeFor(destinationValue));

            if (!Send(messages))
            {
                RemoveRequest(key);

                auto failed = winrt::make_self<MidiPropertyExchangeResponse>();
                failed->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Failed);
                failed->InternalSetRequestId(requestId);

                return *failed;
            }

            auto const answered = WaitForRequest(key);

            PendingRequest completed{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const entry = m_pendingRequests.find(key);

                if (entry != m_pendingRequests.end())
                {
                    completed = entry->second;
                }
            }

            RemoveRequest(key);

            return BuildPropertyResponse(completed, !answered);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            auto failed = winrt::make_self<MidiPropertyExchangeResponse>();
            failed->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Failed);

            return *failed;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiPropertyExchangeResponse>
    MidiCapabilityInquirySession::GetPropertyDataAsync(
        ci::MidiUniqueId destinationMuid,
        json::JsonObject header)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        co_return RequestProperty(destinationMuid, header, nullptr, false);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiPropertyExchangeResponse>
    MidiCapabilityInquirySession::SetPropertyDataAsync(
        ci::MidiUniqueId destinationMuid,
        json::JsonObject header,
        foundation::Collections::IIterable<uint8_t> body)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        co_return RequestProperty(destinationMuid, header, body, true);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiResourceList> MidiCapabilityInquirySession::GetResourceListAsync(
        ci::MidiUniqueId destinationMuid)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            auto const response = RequestProperty(
                destinationMuid, MakeResourceHeader(ResourceResourceList, L""), nullptr, false);

            if (response.Status() != ci::MidiCapabilityInquiryStatus::Success)
            {
                co_return nullptr;
            }

            auto const parsed = response.BodyAsJson();

            if (parsed == nullptr || parsed.ValueType() != json::JsonValueType::Array)
            {
                co_return nullptr;
            }

            co_return ci::MidiResourceList::FromJson(parsed.GetArray());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            co_return nullptr;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiDeviceInfo> MidiCapabilityInquirySession::GetDeviceInfoAsync(
        ci::MidiUniqueId destinationMuid)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            auto const response = RequestProperty(
                destinationMuid, MakeResourceHeader(ResourceDeviceInfo, L""), nullptr, false);

            if (response.Status() != ci::MidiCapabilityInquiryStatus::Success)
            {
                co_return nullptr;
            }

            auto const parsed = response.BodyAsJson();

            if (parsed == nullptr || parsed.ValueType() != json::JsonValueType::Object)
            {
                co_return nullptr;
            }

            co_return ci::MidiDeviceInfo::FromJson(parsed.GetObject());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            co_return nullptr;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiChannelList> MidiCapabilityInquirySession::GetChannelListAsync(
        ci::MidiUniqueId destinationMuid)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            auto const response = RequestProperty(
                destinationMuid, MakeResourceHeader(ResourceChannelList, L""), nullptr, false);

            if (response.Status() != ci::MidiCapabilityInquiryStatus::Success)
            {
                co_return nullptr;
            }

            auto const parsed = response.BodyAsJson();

            if (parsed == nullptr || parsed.ValueType() != json::JsonValueType::Array)
            {
                co_return nullptr;
            }

            co_return ci::MidiChannelList::FromJson(parsed.GetArray());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            co_return nullptr;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiProgramList> MidiCapabilityInquirySession::GetProgramListPageAsync(
        ci::MidiUniqueId destinationMuid,
        winrt::hstring resourceId,
        uint32_t offset,
        uint32_t limit)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            auto header = MakeResourceHeader(ResourceProgramList, resourceId);

            if (header == nullptr)
            {
                co_return nullptr;
            }

            // Paging needs both keys or neither, so they are written together.
            header.SetNamedValue(
                winrt::hstring{ OffsetKey }, json::JsonValue::CreateNumberValue(offset));

            header.SetNamedValue(
                winrt::hstring{ LimitKey }, json::JsonValue::CreateNumberValue(limit));

            auto const response = RequestProperty(destinationMuid, header, nullptr, false);

            if (response.Status() != ci::MidiCapabilityInquiryStatus::Success)
            {
                co_return nullptr;
            }

            auto const parsed = response.BodyAsJson();

            if (parsed == nullptr || parsed.ValueType() != json::JsonValueType::Array)
            {
                co_return nullptr;
            }

            auto const page = ci::MidiProgramList::FromJson(parsed.GetArray());

            if (page == nullptr)
            {
                co_return nullptr;
            }

            page.Offset(offset);

            // The total is in the reply header rather than in the resource, and a device which
            // sends its whole list at once does not report one.
            auto const total = static_cast<uint32_t>(
                SafeGetNamedNumber(response.Header(), TotalCountKey, 0));

            page.TotalCount(total);

            co_return page;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            co_return nullptr;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiProgramList> MidiCapabilityInquirySession::GetProgramListAsync(
        ci::MidiUniqueId destinationMuid,
        winrt::hstring resourceId)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        try
        {
            ci::MidiProgramList whole{ nullptr };

            uint32_t offset{ 0 };

            for (uint32_t page = 0; page < MaximumPages; page++)
            {
                auto const thisPage = co_await GetProgramListPageAsync(
                    destinationMuid, resourceId, offset, DefaultPageSize);

                if (thisPage == nullptr)
                {
                    break;
                }

                if (whole == nullptr)
                {
                    whole = thisPage;
                }
                else
                {
                    for (auto const& entry : thisPage.Entries())
                    {
                        whole.Entries().Append(entry);
                    }

                    if (thisPage.TotalCount() > whole.TotalCount())
                    {
                        whole.TotalCount(thisPage.TotalCount());
                    }
                }

                auto const received = thisPage.Entries().Size();

                // A device which answers with nothing has nothing more to give, whatever it said
                // the total was. Without this a wrong total would page forever.
                if (received == 0)
                {
                    break;
                }

                offset += received;

                if (whole.TotalCount() == 0 || offset >= whole.TotalCount())
                {
                    break;
                }
            }

            if (whole != nullptr)
            {
                // What comes back is the whole list, so it starts at the beginning and there is
                // nothing after it.
                whole.Offset(0);

                if (whole.TotalCount() < whole.Entries().Size())
                {
                    whole.TotalCount(whole.Entries().Size());
                }
            }

            co_return whole;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            co_return nullptr;
        }
    }


    // ---------------------------------------------------------------- profiles

    _Use_decl_annotations_
    foundation::IAsyncOperation<ci::MidiProfileInquiryResponse> MidiCapabilityInquirySession::GetProfilesAsync(
        ci::MidiUniqueId destinationMuid,
        uint8_t deviceId)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        auto response = winrt::make_self<MidiProfileInquiryResponse>();

        response->InternalSetDeviceId(deviceId);
        response->InternalSetResponderMuid(destinationMuid);

        try
        {
            if (destinationMuid == nullptr || !m_isOpen)
            {
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Failed);
                co_return *response;
            }

            auto const destinationValue = destinationMuid.AsCombined28BitValue();

            PendingRequest request{};

            request.ExpectedReply = ci::MidiCapabilityInquiryMessageType::ProfileInquiryReply;
            request.DestinationMuid = destinationValue;
            request.DeviceId = deviceId;

            uint64_t key{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                key = (static_cast<uint64_t>(destinationValue) << 8) | deviceId | 0x8000000000000000ull;

                while (m_pendingRequests.find(key) != m_pendingRequests.end())
                {
                    key++;
                }

                m_pendingRequests[key] = request;
            }

            auto const sent = Send(MidiCapabilityInquiryMessageBuilder::BuildProfileInquiry(
                0, Group(), deviceId, m_sourceMuid, destinationMuid));

            if (!sent)
            {
                RemoveRequest(key);

                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Failed);
                co_return *response;
            }

            auto const answered = WaitForRequest(key);

            ci::MidiCapabilityInquiryMessage reply{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const entry = m_pendingRequests.find(key);

                if (entry != m_pendingRequests.end())
                {
                    reply = entry->second.FirstMessage;
                }
            }

            RemoveRequest(key);

            if (!answered || reply == nullptr)
            {
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::NoResponse);
                co_return *response;
            }

            if (reply.MessageType() == ci::MidiCapabilityInquiryMessageType::Nak)
            {
                response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::NegativeAcknowledgment);
                co_return *response;
            }

            response->InternalSetProfiles(reply.EnabledProfiles(), reply.DisabledProfiles());
            response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Success);

            co_return *response;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            response->InternalSetStatus(ci::MidiCapabilityInquiryStatus::Failed);
            co_return *response;
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquirySession::SendSetProfileOn(
        ci::MidiUniqueId const& destinationMuid,
        uint8_t const deviceId,
        ci::MidiProfileId const& profileId,
        uint16_t const channelCount) noexcept
    {
        try
        {
            return Send(MidiCapabilityInquiryMessageBuilder::BuildSetProfileOn(
                0, Group(), deviceId, m_sourceMuid, destinationMuid, profileId, channelCount));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquirySession::SendSetProfileOff(
        ci::MidiUniqueId const& destinationMuid,
        uint8_t const deviceId,
        ci::MidiProfileId const& profileId) noexcept
    {
        try
        {
            return Send(MidiCapabilityInquiryMessageBuilder::BuildSetProfileOff(
                0, Group(), deviceId, m_sourceMuid, destinationMuid, profileId));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }


    // ------------------------------------------------------------------ events

    _Use_decl_annotations_
    winrt::event_token MidiCapabilityInquirySession::ProfileStateChanged(
        foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryMessageReceivedEventArgs> const& handler)
    {
        return m_profileStateChangedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::ProfileStateChanged(winrt::event_token const& token) noexcept
    {
        m_profileStateChangedEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiCapabilityInquirySession::ResponderFound(
        foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryResponder> const& handler)
    {
        return m_responderFoundEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::ResponderFound(winrt::event_token const& token) noexcept
    {
        m_responderFoundEvent.remove(token);
    }

    _Use_decl_annotations_
    winrt::event_token MidiCapabilityInquirySession::MessageReceived(
        foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryMessageReceivedEventArgs> const& handler)
    {
        return m_messageReceivedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiCapabilityInquirySession::MessageReceived(winrt::event_token const& token) noexcept
    {
        m_messageReceivedEvent.remove(token);
    }
}
