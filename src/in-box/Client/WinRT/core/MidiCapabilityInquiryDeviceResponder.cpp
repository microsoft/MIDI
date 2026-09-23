// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiCapabilityInquiryDeviceResponder.h"

// Consumed here, so the implementation headers are needed as well as the projections.
#include "MidiUniqueId.h"
#include "MidiProfileId.h"
#include "MidiCapabilityInquiryMessage.h"
#include "MidiCapabilityInquiryMessageBuilder.h"
#include "MidiCapabilityInquiryMessageReceivedEventArgs.h"
#include "MidiResourceList.h"
#include "MidiResourceListEntry.h"
#include "MidiProgramList.h"
#include "MidiChannelList.h"
#include "MidiDeviceInfo.h"
#include "MidiGroup.h"

#include "MidiCiMessage.h"

#include "CapabilityInquiry.MidiCapabilityInquiryDeviceResponder.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        namespace native = ::WindowsMidiServicesCapabilityInquiry;

        constexpr std::wstring_view ResourceNameKey{ L"resource" };
        constexpr std::wstring_view ResourceIdKey{ L"resId" };
        constexpr std::wstring_view OffsetKey{ L"offset" };
        constexpr std::wstring_view LimitKey{ L"limit" };
        constexpr std::wstring_view StatusKey{ L"status" };
        constexpr std::wstring_view TotalCountKey{ L"totalCount" };

        constexpr std::wstring_view CommandKey{ L"command" };
        constexpr std::wstring_view SubscribeIdKey{ L"subscribeId" };

        constexpr std::wstring_view CommandStart{ L"start" };
        constexpr std::wstring_view CommandEnd{ L"end" };
        constexpr std::wstring_view CommandFull{ L"full" };

        constexpr std::wstring_view ResourceDeviceInfo{ L"DeviceInfo" };
        constexpr std::wstring_view ResourceResourceList{ L"ResourceList" };
        constexpr std::wstring_view ResourceChannelList{ L"ChannelList" };
        constexpr std::wstring_view ResourceProgramList{ L"ProgramList" };

        std::string ToNarrowString(_In_ winrt::hstring const& text) noexcept
        {
            std::string narrow{};

            try
            {
                for (auto const character : std::wstring_view{ text })
                {
                    // Everything a capability inquiry resource carries is seven bit, and anything
                    // else could not have traveled here in the first place.
                    narrow.push_back(static_cast<char>(character & 0x7F));
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            return narrow;
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

        int32_t SafeGetNamedInteger(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view const key,
            _In_ int32_t const defaultValue) noexcept
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

                if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                {
                    return defaultValue;
                }

                return static_cast<int32_t>(value.GetNumber());
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return defaultValue;
            }
        }

        foundation::Collections::IVector<uint8_t> ToByteVector(_In_ std::string const& text) noexcept
        {
            auto bytes = winrt::single_threaded_vector<uint8_t>();

            try
            {
                for (auto const character : text)
                {
                    bytes.Append(static_cast<uint8_t>(character));
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            return bytes;
        }
    }


    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::InternalAttach(
        midi2::MidiEndpointConnection const& connection,
        midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);

        m_connection = connection;
        m_identity = identity;
    }

    void MidiCapabilityInquiryDeviceResponder::InternalDetach() noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);

        m_connection = nullptr;
        m_incoming.clear();
        m_incomingIsOpen = false;
    }

    _Use_decl_annotations_
    ci::MidiUniqueId MidiCapabilityInquiryDeviceResponder::GetMuid(uint8_t const functionBlockNumber) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            auto const entry = m_muids.find(functionBlockNumber);

            if (entry != m_muids.end())
            {
                return entry->second;
            }

            // Drawn on first use, never derived from anything stable about this machine, because
            // the specification requires an identifier not to survive a restart.
            auto const muid = ci::MidiUniqueId::CreateRandom();

            m_muids.insert_or_assign(functionBlockNumber, muid);

            return muid;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::RegenerateMuid(uint8_t const functionBlockNumber) noexcept
    {
        try
        {
            ci::MidiUniqueId previous{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const entry = m_muids.find(functionBlockNumber);

                if (entry != m_muids.end())
                {
                    previous = entry->second;
                }

                m_muids.insert_or_assign(functionBlockNumber, ci::MidiUniqueId::CreateRandom());
            }

            // Anything talking to the old identifier has to be told to stop, or it will keep
            // addressing a function block which no longer answers to it.
            if (previous != nullptr)
            {
                Send(MidiCapabilityInquiryMessageBuilder::BuildInvalidateMuid(
                    0, midi2::MidiGroup((uint8_t)0), previous, previous));
            }

            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    ci::MidiUniqueId MidiCapabilityInquiryDeviceResponder::MuidForReply() noexcept
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (!m_muids.empty())
                {
                    return m_muids.begin()->second;
                }
            }

            return GetMuid(0);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    ci::MidiCapabilityInquiryCategories MidiCapabilityInquiryDeviceResponder::SupportedCategories() noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            auto categories = static_cast<uint32_t>(ci::MidiCapabilityInquiryCategories::None);

            // A device declares a category when it has something to answer with. Declaring one and
            // then answering nothing is worse than not declaring it.
            if (m_deviceInfo != nullptr ||
                m_channelList != nullptr ||
                m_resourceList != nullptr ||
                !m_programLists.empty() ||
                !m_resources.empty())
            {
                categories |= static_cast<uint32_t>(ci::MidiCapabilityInquiryCategories::PropertyExchange);
            }

            if (!m_profiles.empty())
            {
                categories |= static_cast<uint32_t>(ci::MidiCapabilityInquiryCategories::ProfileConfiguration);
            }

            return static_cast<ci::MidiCapabilityInquiryCategories>(categories);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return ci::MidiCapabilityInquiryCategories::None;
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::ReceivableMaximumSystemExclusiveSize(uint32_t const value) noexcept
    {
        // Every device that implements profiles or property exchange has to accept at least the
        // minimum, so a smaller declaration would be a device saying it cannot do what it does.
        auto const minimum = MidiCapabilityInquiryMessageBuilder::MinimumReceivableSystemExclusiveSize();

        m_maximumSystemExclusiveSize = value < minimum ? minimum : value;
    }

    ci::MidiDeviceInfo MidiCapabilityInquiryDeviceResponder::DeviceInfo() noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        return m_deviceInfo;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::DeviceInfo(ci::MidiDeviceInfo const& value) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_deviceInfo = value;
    }

    ci::MidiChannelList MidiCapabilityInquiryDeviceResponder::ChannelList() noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        return m_channelList;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::ChannelList(ci::MidiChannelList const& value) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_channelList = value;
    }

    ci::MidiResourceList MidiCapabilityInquiryDeviceResponder::ResourceList() noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        return m_resourceList;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::ResourceList(ci::MidiResourceList const& value) noexcept
    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_resourceList = value;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::SetProgramList(
        winrt::hstring const& resourceId,
        ci::MidiProgramList const& programList) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            if (programList == nullptr)
            {
                m_programLists.erase(std::wstring{ resourceId });
            }
            else
            {
                m_programLists.insert_or_assign(std::wstring{ resourceId }, programList);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    ci::MidiProgramList MidiCapabilityInquiryDeviceResponder::GetProgramList(
        winrt::hstring const& resourceId) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            auto const entry = m_programLists.find(std::wstring{ resourceId });

            return entry == m_programLists.end() ? nullptr : entry->second;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::string MidiCapabilityInquiryDeviceResponder::ResourceKey(
        winrt::hstring const& resource,
        winrt::hstring const& resourceId) noexcept
    {
        return ToNarrowString(resource) + "\x01" + ToNarrowString(resourceId);
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::SetResource(
        winrt::hstring const& resource,
        winrt::hstring const& resourceId,
        winrt::hstring const& jsonData) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            m_resources.insert_or_assign(ResourceKey(resource, resourceId), ToNarrowString(jsonData));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::RemoveResource(
        winrt::hstring const& resource,
        winrt::hstring const& resourceId) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            return m_resources.erase(ResourceKey(resource, resourceId)) > 0;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::SetProfiles(
        uint8_t const deviceId,
        foundation::Collections::IIterable<ci::MidiProfileId> const& enabledProfiles,
        foundation::Collections::IIterable<ci::MidiProfileId> const& disabledProfiles) noexcept
    {
        try
        {
            ProfileSet set{};

            if (enabledProfiles != nullptr)
            {
                for (auto const& profileId : enabledProfiles)
                {
                    if (profileId != nullptr)
                    {
                        set.Enabled.push_back(profileId);
                    }
                }
            }

            if (disabledProfiles != nullptr)
            {
                for (auto const& profileId : disabledProfiles)
                {
                    if (profileId != nullptr)
                    {
                        set.Disabled.push_back(profileId);
                    }
                }
            }

            std::lock_guard<std::mutex> guard(m_lock);

            m_profiles.insert_or_assign(deviceId, set);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::Send(
        foundation::Collections::IVector<midi2::MidiMessage64> const& messages) noexcept
    {
        try
        {
            midi2::MidiEndpointConnection connection{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_lock);
                connection = m_connection;
            }

            if (connection == nullptr || messages == nullptr || messages.Size() == 0)
            {
                return false;
            }

            auto packets = winrt::single_threaded_vector<midi2::IMidiUniversalPacket>();

            for (auto const& message : messages)
            {
                packets.Append(message);
            }

            return midi2::MidiEndpointConnection::SendMessageSucceeded(
                connection.SendMultipleMessagesPacketList(packets));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::InternalProcessSystemExclusivePacket(
        uint32_t const word0,
        uint32_t const word1,
        internal::MidiTimestamp const timestamp) noexcept
    {
        try
        {
            if (!m_isEnabled)
            {
                return false;
            }

            auto const group = midi2::MidiGroup(static_cast<uint8_t>((word0 >> 24) & 0x0F));

            auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);
            auto const byteCount = static_cast<uint8_t>((word0 >> 16) & 0x0F);

            if (byteCount > 6)
            {
                return false;
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
                    // A start while another transfer is open means the previous one was abandoned.
                    m_incoming.clear();
                    m_incomingIsOpen = true;
                }
                else if (!m_incomingIsOpen)
                {
                    return false;
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
                // Part of a transfer that is still arriving. Whether it is ours is not yet known,
                // so it is not claimed.
                return false;
            }

            auto data = winrt::single_threaded_vector<uint8_t>();

            for (auto const value : completed)
            {
                data.Append(value);
            }

            if (!ci::MidiCapabilityInquiryMessage::IsCapabilityInquiryData(data))
            {
                return false;
            }

            auto const message = ci::MidiCapabilityInquiryMessage::FromSystemExclusiveData(data);

            if (message == nullptr || !message.IsValid())
            {
                return false;
            }

            HandleMessage(message, group, timestamp);

            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::HandleMessage(
        ci::MidiCapabilityInquiryMessage const& message,
        midi2::MidiGroup const& group,
        internal::MidiTimestamp const timestamp) noexcept
    {
        try
        {
            // Our own replies come back around some transports. Answering them would be a loop.
            auto const source = message.SourceMuid();

            if (source != nullptr)
            {
                std::lock_guard<std::mutex> guard(m_lock);

                for (auto const& entry : m_muids)
                {
                    if (entry.second != nullptr &&
                        entry.second.AsCombined28BitValue() == source.AsCombined28BitValue())
                    {
                        return;
                    }
                }
            }

            bool handled{ false };

            switch (message.MessageType())
            {
            case ci::MidiCapabilityInquiryMessageType::Discovery:
                HandleDiscovery(message, group);
                handled = true;
                break;

            case ci::MidiCapabilityInquiryMessageType::PropertyExchangeCapabilitiesInquiry:
                Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyExchangeCapabilitiesReply(
                    0, group, MuidForReply(), message.SourceMuid(), 1));
                handled = true;
                break;

            case ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry:
                HandlePropertyGet(message, group);
                handled = true;
                break;

            case ci::MidiCapabilityInquiryMessageType::PropertySubscriptionInquiry:
                HandleSubscription(message, group);
                handled = true;
                break;

            case ci::MidiCapabilityInquiryMessageType::InvalidateMuid:
                // An initiator withdrawing its identifier takes its subscriptions with it, or this
                // responder would keep sending updates to a device that is no longer there.
                if (message.SourceMuid() != nullptr)
                {
                    auto const gone = message.SourceMuid().AsCombined28BitValue();

                    std::lock_guard<std::mutex> guard(m_lock);

                    for (auto entry = m_subscribers.begin(); entry != m_subscribers.end(); )
                    {
                        entry = (entry->second.InitiatorMuid == gone)
                            ? m_subscribers.erase(entry)
                            : std::next(entry);
                    }
                }
                break;

            case ci::MidiCapabilityInquiryMessageType::ProfileInquiry:
                HandleProfileInquiry(message, group);
                handled = true;
                break;

            default:
                break;
            }

            if (!handled)
            {
                auto const args = winrt::make<MidiCapabilityInquiryMessageReceivedEventArgs>(
                    message, group, timestamp);

                m_messageReceivedEvent(*this, args);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::HandleDiscovery(
        ci::MidiCapabilityInquiryMessage const& message,
        midi2::MidiGroup const& group) noexcept
    {
        try
        {
            // What the initiator said it can receive. Replies to it are sized against that rather
            // than against what this device is able to send.
            auto const data = message.Data();

            if (data != nullptr && data.Size() >= 29 && message.SourceMuid() != nullptr)
            {
                uint8_t sizeBytes[4]
                {
                    data.GetAt(25), data.GetAt(26), data.GetAt(27), data.GetAt(28)
                };

                auto const declared = native::ReadMuid(sizeBytes);

                std::lock_guard<std::mutex> guard(m_lock);

                m_initiatorMaximumSystemExclusiveSizes.insert_or_assign(
                    message.SourceMuid().AsCombined28BitValue(), declared);
            }

            midi2enum::MidiDeclaredDeviceIdentity identity{ nullptr };
            std::vector<std::pair<uint8_t, ci::MidiUniqueId>> blocks{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                identity = m_identity;

                for (auto const& entry : m_muids)
                {
                    blocks.push_back(entry);
                }
            }

            // A device which has not drawn an identifier yet still has to answer, so the first
            // Discovery is what brings function block zero into being.
            if (blocks.empty())
            {
                blocks.push_back({ (uint8_t)0, GetMuid(0) });
            }

            auto const categories = SupportedCategories();

            // A reply per function block, each with its own identifier, which is what the
            // specification means by an identifier belonging to a function block.
            for (auto const& block : blocks)
            {
                Send(MidiCapabilityInquiryMessageBuilder::BuildDiscoveryReply(
                    0,
                    group,
                    block.second,
                    message.SourceMuid(),
                    identity,
                    categories,
                    m_maximumSystemExclusiveSize,
                    message.OutputPathId(),
                    block.first));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::HandleProfileInquiry(
        ci::MidiCapabilityInquiryMessage const& message,
        midi2::MidiGroup const& group) noexcept
    {
        try
        {
            auto enabled = winrt::single_threaded_vector<ci::MidiProfileId>();
            auto disabled = winrt::single_threaded_vector<ci::MidiProfileId>();

            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const entry = m_profiles.find(message.DeviceId());

                if (entry != m_profiles.end())
                {
                    for (auto const& profileId : entry->second.Enabled) { enabled.Append(profileId); }
                    for (auto const& profileId : entry->second.Disabled) { disabled.Append(profileId); }
                }
            }

            // A device with no profiles at the address it was asked about still replies, with both
            // lists empty. That is a real answer and not the same as saying nothing.
            Send(MidiCapabilityInquiryMessageBuilder::BuildProfileInquiryReply(
                0, group, message.DeviceId(), MuidForReply(), message.SourceMuid(), enabled, disabled));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    std::string MidiCapabilityInquiryDeviceResponder::BuildResourceListJson() noexcept
    {
        try
        {
            auto list = winrt::make<MidiResourceList>();

            auto addEntry = [&list](std::wstring_view const resource, bool const canPaginate)
            {
                auto entry = winrt::make<MidiResourceListEntry>(winrt::hstring{ resource });

                entry.CanPaginate(canPaginate);

                list.Entries().Append(entry);
            };

            // ResourceList always describes itself, which is what lets an initiator ask for it
            // without having been told it exists.
            addEntry(ResourceResourceList, false);

            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (m_deviceInfo != nullptr) { addEntry(ResourceDeviceInfo, false); }
                if (m_channelList != nullptr) { addEntry(ResourceChannelList, false); }
                if (!m_programLists.empty()) { addEntry(ResourceProgramList, true); }

                for (auto const& entry : list.Entries())
                {
                    if (m_subscribableResources.count(std::wstring{ entry.Resource() }) > 0)
                    {
                        entry.CanSubscribe(true);
                    }
                }
            }

            return ToNarrowString(list.GetJson().Stringify());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return "[]";
        }
    }

    _Use_decl_annotations_
    std::string MidiCapabilityInquiryDeviceResponder::ResourceDataFor(
        winrt::hstring const& resource,
        winrt::hstring const& resourceId,
        int32_t const offset,
        int32_t const limit,
        int32_t& totalCount) noexcept
    {
        totalCount = -1;

        try
        {
            if (resource == winrt::hstring{ ResourceResourceList })
            {
                ci::MidiResourceList supplied{ nullptr };

                {
                    std::lock_guard<std::mutex> guard(m_lock);
                    supplied = m_resourceList;
                }

                if (supplied != nullptr)
                {
                    return ToNarrowString(supplied.GetJson().Stringify());
                }

                return BuildResourceListJson();
            }

            if (resource == winrt::hstring{ ResourceDeviceInfo })
            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (m_deviceInfo == nullptr)
                {
                    return {};
                }

                return ToNarrowString(m_deviceInfo.GetJson().Stringify());
            }

            if (resource == winrt::hstring{ ResourceChannelList })
            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (m_channelList == nullptr)
                {
                    return {};
                }

                return ToNarrowString(m_channelList.GetJson().Stringify());
            }

            if (resource == winrt::hstring{ ResourceProgramList })
            {
                ci::MidiProgramList programList{ nullptr };

                {
                    std::lock_guard<std::mutex> guard(m_lock);

                    auto const entry = m_programLists.find(std::wstring{ resourceId });

                    if (entry != m_programLists.end())
                    {
                        programList = entry->second;
                    }
                    else if (resourceId.empty() && !m_programLists.empty())
                    {
                        // A request with no resource id against a device with one list means that
                        // list, which is the common case and worth not refusing.
                        programList = m_programLists.begin()->second;
                    }
                }

                if (programList == nullptr)
                {
                    return {};
                }

                totalCount = static_cast<int32_t>(programList.Entries().Size());

                if (offset < 0 || limit <= 0)
                {
                    return ToNarrowString(programList.GetJson().Stringify());
                }

                // Paging is the initiator's choice, so the slice is cut here rather than the whole
                // list being sent and trimmed at the other end.
                auto page = winrt::make<MidiProgramList>();

                for (int32_t i = offset; i < offset + limit && i < totalCount; i++)
                {
                    page.Entries().Append(programList.Entries().GetAt((uint32_t)i));
                }

                return ToNarrowString(page.GetJson().Stringify());
            }

            std::lock_guard<std::mutex> guard(m_lock);

            auto const entry = m_resources.find(ResourceKey(resource, resourceId));

            return entry == m_resources.end() ? std::string{} : entry->second;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return {};
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::HandlePropertyGet(
        ci::MidiCapabilityInquiryMessage const& message,
        midi2::MidiGroup const& group) noexcept
    {
        try
        {
            auto const header = message.Header();

            auto const resource = SafeGetNamedString(header, ResourceNameKey);
            auto const resourceId = SafeGetNamedString(header, ResourceIdKey);

            auto const offset = SafeGetNamedInteger(header, OffsetKey, -1);
            auto const limit = SafeGetNamedInteger(header, LimitKey, -1);

            int32_t totalCount{ -1 };

            auto const data = resource.empty()
                ? std::string{}
                : ResourceDataFor(resource, resourceId, offset, limit, totalCount);

            json::JsonObject replyHeader{};

            if (data.empty())
            {
                // 404 is how the specification has a device say it does not have that resource,
                // which is different from it refusing to answer at all.
                replyHeader.SetNamedValue(
                    winrt::hstring{ StatusKey }, json::JsonValue::CreateNumberValue(404));

                Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
                    0,
                    group,
                    ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiryReply,
                    MuidForReply(),
                    message.SourceMuid(),
                    message.RequestId(),
                    replyHeader,
                    nullptr,
                    m_maximumSystemExclusiveSize));

                return;
            }

            replyHeader.SetNamedValue(
                winrt::hstring{ StatusKey }, json::JsonValue::CreateNumberValue(200));

            if (totalCount >= 0)
            {
                replyHeader.SetNamedValue(
                    winrt::hstring{ TotalCountKey },
                    json::JsonValue::CreateNumberValue(totalCount));
            }

            // Sized against what the initiator declared it can receive rather than against what we
            // can send. It said so in its Discovery message; a device we have not heard from gets
            // the minimum every implementation is required to accept.
            auto initiatorMaximum = MidiCapabilityInquiryMessageBuilder::MinimumReceivableSystemExclusiveSize();

            if (message.SourceMuid() != nullptr)
            {
                std::lock_guard<std::mutex> guard(m_lock);

                auto const entry = m_initiatorMaximumSystemExclusiveSizes.find(
                    message.SourceMuid().AsCombined28BitValue());

                if (entry != m_initiatorMaximumSystemExclusiveSizes.end() &&
                    entry->second > initiatorMaximum)
                {
                    initiatorMaximum = entry->second;
                }
            }

            Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
                0,
                group,
                ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiryReply,
                MuidForReply(),
                message.SourceMuid(),
                message.RequestId(),
                replyHeader,
                ToByteVector(data),
                initiatorMaximum));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    uint8_t MidiCapabilityInquiryDeviceResponder::NextRequestId() noexcept
    {
        // Zero is legal but is what an implementation that never set one sends, so it is skipped.
        auto const value = m_nextRequestId.fetch_add(1);

        return (value == 0 || value > 0x7F) ? (m_nextRequestId = 1, (uint8_t)1) : value;
    }

    _Use_decl_annotations_
    uint32_t MidiCapabilityInquiryDeviceResponder::MaximumSystemExclusiveSizeFor(
        uint32_t const initiatorMuid) noexcept
    {
        auto maximum = MidiCapabilityInquiryMessageBuilder::MinimumReceivableSystemExclusiveSize();

        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            auto const entry = m_initiatorMaximumSystemExclusiveSizes.find(initiatorMuid);

            if (entry != m_initiatorMaximumSystemExclusiveSizes.end() && entry->second > maximum)
            {
                maximum = entry->second;
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return maximum;
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::SetResourceSubscribable(
        winrt::hstring const& resource,
        bool const canSubscribe) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            if (canSubscribe)
            {
                m_subscribableResources.insert(std::wstring{ resource });
                return;
            }

            m_subscribableResources.erase(std::wstring{ resource });

            // Anyone already subscribed is dropped with it. They are told the next time an update
            // would have gone out, which is the only moment the responder speaks unprompted.
            for (auto entry = m_subscribers.begin(); entry != m_subscribers.end(); )
            {
                entry = (entry->second.Resource == std::wstring{ resource })
                    ? m_subscribers.erase(entry)
                    : std::next(entry);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::IsResourceSubscribable(
        winrt::hstring const& resource) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            return m_subscribableResources.count(std::wstring{ resource }) > 0;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    void MidiCapabilityInquiryDeviceResponder::HandleSubscription(
        ci::MidiCapabilityInquiryMessage const& message,
        midi2::MidiGroup const& group) noexcept
    {
        try
        {
            auto const header = message.Header();

            auto const command = SafeGetNamedString(header, CommandKey);
            auto const resource = SafeGetNamedString(header, ResourceNameKey);
            auto const resourceId = SafeGetNamedString(header, ResourceIdKey);
            auto const subscribeId = SafeGetNamedString(header, SubscribeIdKey);

            int32_t status{ 400 };
            winrt::hstring assigned{};

            if (command == winrt::hstring{ CommandStart })
            {
                int32_t totalCount{ -1 };

                const bool haveData = !resource.empty() &&
                    !ResourceDataFor(resource, resourceId, -1, -1, totalCount).empty();

                std::lock_guard<std::mutex> guard(m_lock);

                if (!haveData || m_subscribableResources.count(std::wstring{ resource }) == 0)
                {
                    // 405 is the specification's "this device does not do that with this
                    // resource", which is what an initiator needs to know to fall back to polling.
                    status = 405;
                }
                else
                {
                    assigned = winrt::hstring{ L"s" + std::to_wstring(m_nextSubscribeId++) };

                    DeviceSubscription added{};

                    added.InitiatorMuid = message.SourceMuid() == nullptr
                        ? 0 : message.SourceMuid().AsCombined28BitValue();
                    added.InitiatorId = message.SourceMuid();
                    added.Resource = resource;
                    added.ResourceId = resourceId;

                    m_subscribers.insert_or_assign(std::wstring{ assigned }, added);

                    status = 200;
                }
            }
            else if (command == winrt::hstring{ CommandEnd })
            {
                std::lock_guard<std::mutex> guard(m_lock);

                m_subscribers.erase(std::wstring{ subscribeId });

                status = 200;
            }

            json::JsonObject replyHeader{};

            replyHeader.SetNamedValue(
                winrt::hstring{ StatusKey }, json::JsonValue::CreateNumberValue(status));

            if (!assigned.empty())
            {
                replyHeader.SetNamedValue(
                    winrt::hstring{ SubscribeIdKey }, json::JsonValue::CreateStringValue(assigned));
            }

            Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
                0,
                group,
                ci::MidiCapabilityInquiryMessageType::PropertySubscriptionInquiryReply,
                MuidForReply(),
                message.SourceMuid(),
                message.RequestId(),
                replyHeader,
                nullptr,
                message.SourceMuid() == nullptr
                    ? MidiCapabilityInquiryMessageBuilder::MinimumReceivableSystemExclusiveSize()
                    : MaximumSystemExclusiveSizeFor(message.SourceMuid().AsCombined28BitValue())));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    uint32_t MidiCapabilityInquiryDeviceResponder::NotifyResourceChanged(
        winrt::hstring const& resource,
        winrt::hstring const& resourceId) noexcept
    {
        uint32_t told{ 0 };

        try
        {
            if (!m_isEnabled || resource.empty())
            {
                return 0;
            }

            std::vector<std::pair<std::wstring, DeviceSubscription>> targets{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                for (auto const& entry : m_subscribers)
                {
                    if (entry.second.Resource != std::wstring{ resource })
                    {
                        continue;
                    }

                    // A subscriber that named no resource id wants whichever one it subscribed
                    // to, so an update for a different one is not theirs.
                    if (entry.second.ResourceId != std::wstring{ resourceId })
                    {
                        continue;
                    }

                    targets.emplace_back(entry.first, entry.second);
                }
            }

            if (targets.empty())
            {
                return 0;
            }

            int32_t totalCount{ -1 };

            auto const data = ResourceDataFor(resource, resourceId, -1, -1, totalCount);

            if (data.empty())
            {
                return 0;
            }

            for (auto const& target : targets)
            {
                json::JsonObject header{};

                header.SetNamedValue(
                    winrt::hstring{ SubscribeIdKey },
                    json::JsonValue::CreateStringValue(winrt::hstring{ target.first }));

                header.SetNamedValue(
                    winrt::hstring{ CommandKey },
                    json::JsonValue::CreateStringValue(winrt::hstring{ CommandFull }));

                if (Send(MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
                    0,
                    midi2::MidiGroup((uint8_t)0),
                    ci::MidiCapabilityInquiryMessageType::PropertySubscriptionInquiry,
                    MuidForReply(),
                    target.second.InitiatorId,
                    NextRequestId(),
                    header,
                    ToByteVector(data),
                    MaximumSystemExclusiveSizeFor(target.second.InitiatorMuid))))
                {
                    told++;
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return told;
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::SendProfileEnabledReport(
        uint8_t const functionBlockNumber,
        uint8_t const deviceId,
        ci::MidiProfileId const& profileId,
        uint16_t const channelCount) noexcept
    {
        return Send(MidiCapabilityInquiryMessageBuilder::BuildProfileEnabledReport(
            0, midi2::MidiGroup((uint8_t)0), deviceId, GetMuid(functionBlockNumber), profileId, channelCount));
    }
    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::SendProfileDisabledReport(
        uint8_t const functionBlockNumber,
        uint8_t const deviceId,
        ci::MidiProfileId const& profileId,
        uint16_t const channelCount) noexcept
    {
        return Send(MidiCapabilityInquiryMessageBuilder::BuildProfileDisabledReport(
            0, midi2::MidiGroup((uint8_t)0), deviceId, GetMuid(functionBlockNumber), profileId, channelCount));
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::SendProfileAddedReport(
        uint8_t const functionBlockNumber,
        uint8_t const deviceId,
        ci::MidiProfileId const& profileId) noexcept
    {
        return Send(MidiCapabilityInquiryMessageBuilder::BuildProfileAddedReport(
            0, midi2::MidiGroup((uint8_t)0), deviceId, GetMuid(functionBlockNumber), profileId));
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryDeviceResponder::SendProfileRemovedReport(
        uint8_t const functionBlockNumber,
        uint8_t const deviceId,
        ci::MidiProfileId const& profileId) noexcept
    {
        return Send(MidiCapabilityInquiryMessageBuilder::BuildProfileRemovedReport(
            0, midi2::MidiGroup((uint8_t)0), deviceId, GetMuid(functionBlockNumber), profileId));
    }
}
