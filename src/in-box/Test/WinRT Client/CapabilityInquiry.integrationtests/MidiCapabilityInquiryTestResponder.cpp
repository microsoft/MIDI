// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;

namespace json = winrt::Windows::Data::Json;

namespace
{
    winrt::Windows::Foundation::Collections::IVector<uint8_t> ToByteVector(std::string const& text)
    {
        auto bytes = winrt::single_threaded_vector<uint8_t>();

        for (auto const character : text)
        {
            bytes.Append(static_cast<uint8_t>(character));
        }

        return bytes;
    }

    std::string ToNarrowString(winrt::hstring const& text)
    {
        std::string narrow{};

        for (auto const character : std::wstring_view{ text })
        {
            narrow.push_back(static_cast<char>(character));
        }

        return narrow;
    }
}


void MidiCapabilityInquiryTestResponder::Start(
    MidiEndpointConnection const& connection,
    uint32_t muid)
{
    m_connection = connection;
    m_muid = MidiUniqueId(muid);

    m_token = m_connection.MessageReceived(
        { this, &MidiCapabilityInquiryTestResponder::OnMessageReceived });
}

void MidiCapabilityInquiryTestResponder::Stop()
{
    if (m_connection != nullptr && m_token.value != 0)
    {
        m_connection.MessageReceived(m_token);
        m_token = {};
    }

    m_connection = nullptr;
}

void MidiCapabilityInquiryTestResponder::SetResource(std::string const& resource, std::string const& body)
{
    std::lock_guard<std::mutex> guard(m_lock);
    m_resources[resource] = body;
}

void MidiCapabilityInquiryTestResponder::SetPagedEntries(std::vector<std::string> const& entries)
{
    std::lock_guard<std::mutex> guard(m_lock);
    m_pagedEntries = entries;
}

void MidiCapabilityInquiryTestResponder::SetProfiles(
    std::vector<MidiProfileId> const& enabled,
    std::vector<MidiProfileId> const& disabled)
{
    std::lock_guard<std::mutex> guard(m_lock);
    m_enabledProfiles = enabled;
    m_disabledProfiles = disabled;
}

std::vector<MidiCapabilityInquiryMessageType> MidiCapabilityInquiryTestResponder::MessageLog() const
{
    std::lock_guard<std::mutex> guard(m_lock);
    return m_messageLog;
}

void MidiCapabilityInquiryTestResponder::Send(
    winrt::Windows::Foundation::Collections::IVector<MidiMessage64> const& messages)
{
    if (m_connection == nullptr || messages == nullptr)
    {
        return;
    }

    auto packets = winrt::single_threaded_vector<IMidiUniversalPacket>();

    for (auto const& message : messages)
    {
        packets.Append(message);
    }

    m_connection.SendMultipleMessagesPacketList(packets);
}

void MidiCapabilityInquiryTestResponder::OnMessageReceived(
    winrt::Windows::Foundation::IInspectable const& /*sender*/,
    MidiMessageReceivedEventArgs const& args)
{
    uint32_t word0{};
    uint32_t word1{};
    uint32_t word2{};
    uint32_t word3{};

    if (args.FillWords(word0, word1, word2, word3) != 2)
    {
        return;
    }

    if (((word0 >> 28) & 0x0F) != 0x03)
    {
        return;
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
            m_incoming.clear();
            m_incomingIsOpen = true;
        }
        else if (!m_incomingIsOpen)
        {
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

    if (completed.empty())
    {
        return;
    }

    auto data = winrt::single_threaded_vector<uint8_t>();

    for (auto const value : completed)
    {
        data.Append(value);
    }

    if (!MidiCapabilityInquiryMessage::IsCapabilityInquiryData(data))
    {
        return;
    }

    auto const message = MidiCapabilityInquiryMessage::FromSystemExclusiveData(data);

    if (message != nullptr && message.IsValid())
    {
        HandleMessage(message);
    }
}

void MidiCapabilityInquiryTestResponder::HandleMessage(MidiCapabilityInquiryMessage const& message)
{
    // Our own replies come back around the loopback, so anything we sent is ignored here.
    if (message.SourceMuid() != nullptr && m_muid != nullptr &&
        message.SourceMuid().AsCombined28BitValue() == m_muid.AsCombined28BitValue())
    {
        return;
    }

    auto const group = MidiGroup((uint8_t)0);

    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_messageLog.push_back(message.MessageType());
    }

    switch (message.MessageType())
    {
    case MidiCapabilityInquiryMessageType::Discovery:
    {
        if (!m_answerDiscovery)
        {
            return;
        }

        auto const identity = MidiDeclaredDeviceIdentity(
            0x00, 0x00, 0x41,
            0x0B, 0x00,
            0x01, 0x00,
            0x01, 0x00, 0x00, 0x00);

        Send(MidiCapabilityInquiryMessageBuilder::BuildDiscoveryReply(
            0,
            group,
            m_muid,
            message.SourceMuid(),
            identity,
            MidiCapabilityInquiryCategories::PropertyExchange |
                MidiCapabilityInquiryCategories::ProfileConfiguration,
            m_maximumSystemExclusiveSize,
            message.OutputPathId(),
            0));

        break;
    }

    case MidiCapabilityInquiryMessageType::PropertyExchangeCapabilitiesInquiry:
        Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyExchangeCapabilitiesReply(
            0, group, m_muid, message.SourceMuid(), 4));
        break;

    case MidiCapabilityInquiryMessageType::PropertyGetDataInquiry:
        m_requestCount++;
        HandlePropertyGet(message);
        break;

    case MidiCapabilityInquiryMessageType::PropertySubscriptionInquiry:
        HandleSubscription(message);
        break;

    case MidiCapabilityInquiryMessageType::ProfileInquiry:
    {
        std::lock_guard<std::mutex> guard(m_lock);

        auto enabled = winrt::single_threaded_vector<MidiProfileId>();
        auto disabled = winrt::single_threaded_vector<MidiProfileId>();

        for (auto const& profileId : m_enabledProfiles) { enabled.Append(profileId); }
        for (auto const& profileId : m_disabledProfiles) { disabled.Append(profileId); }

        Send(MidiCapabilityInquiryMessageBuilder::BuildProfileInquiryReply(
            0, group, message.DeviceId(), m_muid, message.SourceMuid(), enabled, disabled));

        break;
    }

    default:
        break;
    }
}

void MidiCapabilityInquiryTestResponder::HandleSubscription(MidiCapabilityInquiryMessage const& message)
{
    if (m_answerNothing)
    {
        return;
    }

    auto const group = MidiGroup((uint8_t)0);
    auto const header = message.Header();

    std::string command{};
    std::string resourceName{};
    std::string subscribeId{};

    if (header != nullptr)
    {
        command = ToNarrowString(header.GetNamedString(L"command", L""));
        resourceName = ToNarrowString(header.GetNamedString(L"resource", L""));
        subscribeId = ToNarrowString(header.GetNamedString(L"subscribeId", L""));
    }

    int32_t status{ 400 };
    std::string assigned{};

    if (command == "start")
    {
        if (!m_acceptSubscriptions)
        {
            status = 405;
        }
        else
        {
            std::lock_guard<std::mutex> guard(m_lock);

            assigned = "s" + std::to_string(m_nextSubscribeId++);

            TestSubscription added{};
            added.InitiatorMuid = message.SourceMuid();
            added.Resource = resourceName;

            m_subscriptions[assigned] = added;

            status = 200;
        }
    }
    else if (command == "end")
    {
        std::lock_guard<std::mutex> guard(m_lock);

        m_subscriptions.erase(subscribeId);

        status = 200;
    }

    json::JsonObject replyHeader{};

    replyHeader.SetNamedValue(L"status", json::JsonValue::CreateNumberValue(status));

    if (!assigned.empty())
    {
        replyHeader.SetNamedValue(
            L"subscribeId", json::JsonValue::CreateStringValue(winrt::to_hstring(assigned)));
    }

    Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
        0,
        group,
        MidiCapabilityInquiryMessageType::PropertySubscriptionInquiryReply,
        m_muid,
        message.SourceMuid(),
        message.RequestId(),
        replyHeader,
        nullptr,
        m_maximumSystemExclusiveSize));
}

uint32_t MidiCapabilityInquiryTestResponder::NotifyResourceChanged(std::string const& resource)
{
    auto const group = MidiGroup((uint8_t)0);

    std::vector<std::pair<std::string, TestSubscription>> targets{};
    std::string body{ "[]" };

    {
        std::lock_guard<std::mutex> guard(m_lock);

        for (auto const& entry : m_subscriptions)
        {
            if (entry.second.Resource == resource)
            {
                targets.emplace_back(entry.first, entry.second);
            }
        }

        auto const found = m_resources.find(resource);

        if (found != m_resources.end())
        {
            body = found->second;
        }
    }

    for (auto const& target : targets)
    {
        json::JsonObject header{};

        header.SetNamedValue(
            L"subscribeId", json::JsonValue::CreateStringValue(winrt::to_hstring(target.first)));
        header.SetNamedValue(L"command", json::JsonValue::CreateStringValue(L"full"));

        auto data = winrt::single_threaded_vector<uint8_t>();

        for (auto const character : body)
        {
            data.Append(static_cast<uint8_t>(character));
        }

        Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
            0,
            group,
            MidiCapabilityInquiryMessageType::PropertySubscriptionInquiry,
            m_muid,
            target.second.InitiatorMuid,
            m_nextRequestId++,
            header,
            data,
            m_maximumSystemExclusiveSize));
    }

    return static_cast<uint32_t>(targets.size());
}

uint32_t MidiCapabilityInquiryTestResponder::EndAllSubscriptions()
{
    auto const group = MidiGroup((uint8_t)0);

    std::map<std::string, TestSubscription> held{};

    {
        std::lock_guard<std::mutex> guard(m_lock);
        held.swap(m_subscriptions);
    }

    for (auto const& entry : held)
    {
        json::JsonObject header{};

        header.SetNamedValue(
            L"subscribeId", json::JsonValue::CreateStringValue(winrt::to_hstring(entry.first)));
        header.SetNamedValue(L"command", json::JsonValue::CreateStringValue(L"end"));

        Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
            0,
            group,
            MidiCapabilityInquiryMessageType::PropertySubscriptionInquiry,
            m_muid,
            entry.second.InitiatorMuid,
            m_nextRequestId++,
            header,
            nullptr,
            m_maximumSystemExclusiveSize));
    }

    return static_cast<uint32_t>(held.size());
}

uint32_t MidiCapabilityInquiryTestResponder::SubscriptionCount() const
{
    std::lock_guard<std::mutex> guard(m_lock);

    return static_cast<uint32_t>(m_subscriptions.size());
}

void MidiCapabilityInquiryTestResponder::HandlePropertyGet(MidiCapabilityInquiryMessage const& message)
{
    if (m_answerNothing)
    {
        return;
    }

    auto const group = MidiGroup((uint8_t)0);

    if (m_answerWithNak)
    {
        auto details = winrt::single_threaded_vector<uint8_t>();

        details.Append(message.RequestId());
        details.Append(1);

        Send(MidiCapabilityInquiryMessageBuilder::BuildNak(
            0,
            group,
            0x7F,
            m_muid,
            message.SourceMuid(),
            MidiCapabilityInquiryMessageType::PropertyGetDataInquiry,
            0x01,
            0x00,
            details,
            L"Not available"));

        return;
    }

    auto const header = message.Header();

    std::string resourceName{};
    int32_t offset{ -1 };
    int32_t limit{ -1 };

    if (header != nullptr)
    {
        if (header.HasKey(L"resource"))
        {
            resourceName = ToNarrowString(header.GetNamedString(L"resource", L""));
        }

        if (header.HasKey(L"offset"))
        {
            offset = static_cast<int32_t>(header.GetNamedNumber(L"offset", 0));
        }

        if (header.HasKey(L"limit"))
        {
            limit = static_cast<int32_t>(header.GetNamedNumber(L"limit", 0));
        }
    }

    std::string body{};
    int32_t totalCount{ -1 };

    {
        std::lock_guard<std::mutex> guard(m_lock);

        if (!m_pagedEntries.empty() && offset >= 0 && limit > 0)
        {
            // Slice the list the way a device with a long list would.
            totalCount = static_cast<int32_t>(m_pagedEntries.size());

            body = "[";

            for (int32_t i = offset; i < offset + limit && i < totalCount; i++)
            {
                if (i > offset)
                {
                    body += ",";
                }

                body += m_pagedEntries[i];
            }

            body += "]";
        }
        else
        {
            auto const entry = m_resources.find(resourceName);

            body = entry == m_resources.end() ? std::string{ "[]" } : entry->second;
        }
    }

    json::JsonObject replyHeader{};

    replyHeader.SetNamedValue(
        L"status", json::JsonValue::CreateNumberValue(m_resourceStatus.load()));

    if (totalCount >= 0)
    {
        replyHeader.SetNamedValue(
            L"totalCount", json::JsonValue::CreateNumberValue(totalCount));
    }

    Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
        0,
        group,
        MidiCapabilityInquiryMessageType::PropertyGetDataInquiryReply,
        m_muid,
        message.SourceMuid(),
        message.RequestId(),
        replyHeader,
        ToByteVector(body),
        m_maximumSystemExclusiveSize));
}

void MidiCapabilityInquiryTestResponder::SendProfileEnabledReport(
    MidiProfileId const& profileId,
    uint16_t channelCount)
{
    Send(MidiCapabilityInquiryMessageBuilder::BuildProfileEnabledReport(
        0, MidiGroup((uint8_t)0), 0x7F, m_muid, profileId, channelCount));
}
