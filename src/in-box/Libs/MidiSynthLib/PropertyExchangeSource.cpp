// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/PropertyExchangeSource.h"

#include "MidiSynth/ProgramList.h"

#include "MidiCiProgramList.h"

#include <cstdio>
#include <cstring>
#include <iterator>

#include <windows.h>

namespace ci = ::WindowsMidiServicesCapabilityInquiry;

namespace
{
    // Reply header for a successful request.
    constexpr uint8_t ReplyHeaderOk[]{ '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '2', '0', '0', '}' };

    // The sound set cannot change while the device is running, so what is built from it is worth
    // caching. Without this a client refetches nine kilobytes on every reconnect.
    constexpr uint8_t ReplyHeaderOkCacheable[]
    {
        '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '2', '0', '0', ',',
        '"', 'c', 'a', 'c', 'h', 'e', 'T', 'i', 'm', 'e', '"', ':', '3', '6', '0', '0', '}'
    };

    constexpr uint8_t ReplyHeaderNotFound[]{ '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '4', '0', '4', '}' };

    // What an initiator declared it can receive. 512 is the smallest seen in practice.
    constexpr size_t AssumedInitiatorMaximumSysExSize = 512;

    // M2-103-UM section 14 is explicit that a reply to a ResourceList inquiry does not list
    // ResourceList itself. The resource is still answered by name; it just does not advertise.
    constexpr ci::ResourceListEntry ResourceEntryList[]
    {
        { "DeviceInfo", false, false, false },
        { "ChannelList", false, true, false },
        { "ProgramList", true, false, true },
    };

    // Shown to a customer when a client lists the collections a channel can select from. Not
    // localized: this library has no resource loading, and the program names it sits beside come
    // out of the sound set file untranslated.
    constexpr char const* MelodicProgramListTitle = "Melodic Programs";
    constexpr char const* DrumKitProgramListTitle = "Drum Kits";
}

namespace MidiSynth
{
    _Use_decl_annotations_
    const ci::ResourceListEntry* PropertyExchangeSource::ResourceEntries(size_t& count) noexcept
    {
        count = std::size(ResourceEntryList);
        return ResourceEntryList;
    }

    _Use_decl_annotations_
    void PropertyExchangeSource::Build(const DlsCollection& collection, const SynthIdentity& identity)
    {
        m_melodicProgramListJson = BuildProgramListJson(collection, ProgramListKind::Melodic);
        m_drumKitProgramListJson = BuildProgramListJson(collection, ProgramListKind::DrumKits);

        ci::DeviceInfoFields info{};

        info.ManufacturerId[0] = identity.ManufacturerSysExId[0];
        info.ManufacturerId[1] = identity.ManufacturerSysExId[1];
        info.ManufacturerId[2] = identity.ManufacturerSysExId[2];
        info.Manufacturer = "Microsoft";

        // Two seven bit bytes, least significant first, exactly as the identity reply carries them.
        info.FamilyId[0] = static_cast<uint8_t>(identity.FamilyCode & 0x7F);
        info.FamilyId[1] = static_cast<uint8_t>((identity.FamilyCode >> 7) & 0x7F);
        info.Family = "Windows";

        info.ModelId[0] = static_cast<uint8_t>(identity.FamilyMemberCode & 0x7F);
        info.ModelId[1] = static_cast<uint8_t>((identity.FamilyMemberCode >> 7) & 0x7F);
        info.Model = "General MIDI Synth";

        // The same four bytes the identity reply and the discovery reply carry, because M2-105-UM
        // requires them to match. The string is built from them so the two cannot drift apart.
        info.VersionId[0] = identity.SoftwareRevision[0];
        info.VersionId[1] = identity.SoftwareRevision[1];
        info.VersionId[2] = identity.SoftwareRevision[2];
        info.VersionId[3] = identity.SoftwareRevision[3];

        char versionText[20]{};

        (void)snprintf(versionText, sizeof(versionText), "%u.%u.%u.%u",
            identity.SoftwareRevision[0], identity.SoftwareRevision[1],
            identity.SoftwareRevision[2], identity.SoftwareRevision[3]);

        info.Version = versionText;

        m_deviceInfoJson.resize(ci::BuildDeviceInfoJson(info, nullptr, 0));
        (void)ci::BuildDeviceInfoJson(info, m_deviceInfoJson.data(), m_deviceInfoJson.size());

        size_t nameCount = 0;
        auto const* const resources = ResourceEntries(nameCount);

        m_resourceListJson.resize(ci::BuildResourceListJson(resources, nameCount, nullptr, 0));
        (void)ci::BuildResourceListJson(resources, nameCount, m_resourceListJson.data(), m_resourceListJson.size());
    }

    _Use_decl_annotations_
    const std::vector<char>& PropertyExchangeSource::RebuildChannelListJson(
        const SynthEngine& engine,
        const DlsCollection& collection)
    {
        ci::ChannelListEntry entries[MidiChannelCount]{};
        char titles[MidiChannelCount][16]{};
        std::string programTitles[MidiChannelCount];

        // One link object per kind, shared by every channel that uses it. They outlive the build.
        constexpr ci::ResourceLink MelodicLink
        {
            "ProgramList", MelodicProgramListResourceId, MelodicProgramListTitle
        };

        constexpr ci::ResourceLink DrumKitLink
        {
            "ProgramList", DrumKitProgramListResourceId, DrumKitProgramListTitle
        };

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            const auto state = engine.ChannelState(channel);

            snprintf(titles[channel], sizeof(titles[channel]), "Channel %u", channel + 1u);

            entries[channel].Title = titles[channel];
            entries[channel].Channel = static_cast<uint16_t>(channel + 1);
            entries[channel].BankMsb = state.BankMsb;
            entries[channel].BankLsb = state.BankLsb;
            entries[channel].Program = state.Program;

            // Channel 10 is the drum channel by convention, but content can move it with the GS
            // rhythm part message, so the engine's own flag decides rather than the channel number.
            const auto* instrument = collection.FindInstrument(
                state.BankMsb, state.BankLsb, state.Program, state.IsDrumChannel);

            if (instrument != nullptr)
            {
                programTitles[channel] = ToNarrow(instrument->Name);
                entries[channel].ProgramTitle = programTitles[channel].c_str();
            }

            entries[channel].Links = state.IsDrumChannel ? &DrumKitLink : &MelodicLink;
            entries[channel].LinkCount = 1;
        }

        const auto required = ci::BuildChannelListJson(entries, MidiChannelCount, nullptr, 0);

        m_channelListJson.resize(required);

        (void)ci::BuildChannelListJson(
            entries, MidiChannelCount, m_channelListJson.data(), m_channelListJson.size());

        return m_channelListJson;
    }

    _Use_decl_annotations_
    void PropertyExchangeSource::BeginReply(
        const UmpDispatcher::PendingPropertyRequest& request,
        const std::vector<char>& resource,
        bool cacheable) noexcept
    {
        m_replyInitiatorMuid = request.InitiatorMuid;
        m_replyRequestId = request.RequestId;

        m_chunker = {};
        m_chunker.Resource = reinterpret_cast<const uint8_t*>(resource.data());
        m_chunker.ResourceByteCount = resource.size();
        m_chunker.Header = cacheable ? ReplyHeaderOkCacheable : ReplyHeaderOk;
        m_chunker.HeaderByteCount = static_cast<uint16_t>(
            cacheable ? sizeof(ReplyHeaderOkCacheable) : sizeof(ReplyHeaderOk));

        m_nextChunk = m_chunker.Plan(AssumedInitiatorMaximumSysExSize) ? 1 : 0;
    }

    _Use_decl_annotations_
    void PropertyExchangeSource::BeginProgramListReply(
        const DlsCollection& collection,
        const UmpDispatcher::PendingPropertyRequest& request,
        const std::string& resourceId,
        size_t offset,
        size_t limit) noexcept
    {
        auto const kind = (resourceId == DrumKitProgramListResourceId)
            ? ProgramListKind::DrumKits
            : ProgramListKind::Melodic;

        // The whole point of totalCount is that it does not depend on the page, so it is the count
        // of the list rather than the number of entries about to go out.
        auto const total = CountPrograms(collection, kind);

        // An initiator that did not paginate gets the list built at startup. Only a real page
        // costs a serialization, and either buffer outlives the reply that points into it.
        auto const wholeList = (offset == 0 && limit >= total);

        if (!wholeList)
        {
            m_programPageJson = BuildProgramListPageJson(collection, kind, offset, limit);
        }

        auto const& resource = wholeList ? ProgramListJson(resourceId) : m_programPageJson;

        auto const headerLength = snprintf(
            m_replyHeader, sizeof(m_replyHeader),
            "{\"status\":200,\"cacheTime\":3600,\"totalCount\":%zu}", total);

        if (headerLength <= 0 || static_cast<size_t>(headerLength) >= sizeof(m_replyHeader))
        {
            m_nextChunk = 0;
            return;
        }

        m_replyInitiatorMuid = request.InitiatorMuid;
        m_replyRequestId = request.RequestId;

        m_chunker = {};
        m_chunker.Resource = reinterpret_cast<const uint8_t*>(resource.data());
        m_chunker.ResourceByteCount = resource.size();
        m_chunker.Header = reinterpret_cast<const uint8_t*>(m_replyHeader);
        m_chunker.HeaderByteCount = static_cast<uint16_t>(headerLength);

        m_nextChunk = m_chunker.Plan(AssumedInitiatorMaximumSysExSize) ? 1 : 0;
    }

    _Use_decl_annotations_
    bool PropertyExchangeSource::SendNextChunk(
        IUmpOutput& output,
        uint8_t group,
        uint32_t sourceMuid) noexcept
    {
        if (m_nextChunk == 0)
        {
            return false;
        }

        uint8_t buffer[640]{};

        const auto written = m_chunker.BuildChunk(
            m_nextChunk, sourceMuid, m_replyInitiatorMuid, m_replyRequestId, buffer, sizeof(buffer));

        if (written == 0)
        {
            m_nextChunk = 0;
            return false;
        }

        UmpDispatcher::PacketizeSysEx7(output, group, buffer, written);

        m_nextChunk = (m_nextChunk >= m_chunker.ChunkCount) ? 0 : static_cast<uint16_t>(m_nextChunk + 1);

        return m_nextChunk != 0;
    }

    _Use_decl_annotations_
    void PropertyExchangeSource::SendNotFound(
        IUmpOutput& output,
        uint8_t group,
        uint32_t sourceMuid,
        const UmpDispatcher::PendingPropertyRequest& request) noexcept
    {
        ci::PropertyExchangeMessageFields fields{};

        fields.Type = ci::MessageType::PropertyGetDataReply;
        fields.SourceMuid = sourceMuid;
        fields.DestinationMuid = request.InitiatorMuid;
        fields.RequestId = request.RequestId;
        fields.Header = ReplyHeaderNotFound;
        fields.HeaderByteCount = static_cast<uint16_t>(sizeof(ReplyHeaderNotFound));
        fields.ChunkCount = 1;
        fields.ChunkNumber = 1;

        uint8_t buffer[64]{};

        const auto written = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        if (written > 0)
        {
            UmpDispatcher::PacketizeSysEx7(output, group, buffer, written);
        }
    }

    _Use_decl_annotations_
    const char* PropertyExchangeSource::AddChannelListSubscription(uint32_t initiatorMuid) noexcept
    {
        for (size_t i = 0; i < m_subscriptionCount; i++)
        {
            // Re-subscribing keeps the identifier it already has, so an initiator that asks twice
            // does not end up holding two subscriptions to the same thing.
            if (m_subscriptions[i].InitiatorMuid == initiatorMuid)
            {
                m_subscriptions[i].NeedsUpdate = false;
                return m_subscriptions[i].SubscribeId;
            }
        }

        if (m_subscriptionCount >= MaxSubscriptions)
        {
            return "";
        }

        auto& added = m_subscriptions[m_subscriptionCount];

        added = {};
        added.InitiatorMuid = initiatorMuid;

        // M2-103-UM caps a subscribeId at eight characters, so the counter wraps before the text
        // would need a sixth digit. Truncating instead would let two subscribers share an id.
        if (m_nextSubscribeId > 99999)
        {
            m_nextSubscribeId = 1;
        }

        snprintf(added.SubscribeId, sizeof(added.SubscribeId), "ch%u", m_nextSubscribeId++);

        m_subscriptionCount++;

        return added.SubscribeId;
    }

    _Use_decl_annotations_
    bool PropertyExchangeSource::RemoveSubscription(
        uint32_t initiatorMuid,
        const std::string& subscribeId) noexcept
    {
        bool removed{ false };

        for (size_t i = 0; i < m_subscriptionCount; )
        {
            const bool matches =
                m_subscriptions[i].InitiatorMuid == initiatorMuid &&
                (subscribeId.empty() || subscribeId == m_subscriptions[i].SubscribeId);

            if (!matches)
            {
                i++;
                continue;
            }

            m_subscriptions[i] = m_subscriptions[m_subscriptionCount - 1];
            m_subscriptions[m_subscriptionCount - 1] = {};
            m_subscriptionCount--;

            removed = true;
        }

        return removed;
    }

    _Use_decl_annotations_
    bool PropertyExchangeSource::ChannelListChanged(const SynthEngine& engine) noexcept
    {
        ChannelSnapshot current[MidiChannelCount]{};

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            const auto state = engine.ChannelState(channel);

            current[channel].BankMsb = state.BankMsb;
            current[channel].BankLsb = state.BankLsb;
            current[channel].Program = state.Program;
            current[channel].IsDrumChannel = state.IsDrumChannel;
        }

        const bool changed =
            !m_haveChannelSnapshot ||
            memcmp(current, m_lastNotifiedChannels, sizeof(current)) != 0;

        if (!changed)
        {
            return false;
        }

        memcpy(m_lastNotifiedChannels, current, sizeof(current));

        // The first pass only establishes the baseline. Sending an update for a state nobody has
        // asked about yet would notify a subscriber of a change that did not happen.
        if (!m_haveChannelSnapshot)
        {
            m_haveChannelSnapshot = true;
            return false;
        }

        for (size_t i = 0; i < m_subscriptionCount; i++)
        {
            m_subscriptions[i].NeedsUpdate = true;
        }

        return true;
    }

    _Use_decl_annotations_
    bool PropertyExchangeSource::BeginNextSubscriptionNotification() noexcept
    {
        for (size_t i = 0; i < m_subscriptionCount; i++)
        {
            if (!m_subscriptions[i].NeedsUpdate)
            {
                continue;
            }

            m_subscriptions[i].NeedsUpdate = false;

            // The command comes first: M2-103-UM section 7.1 requires it of every subscription
            // message, and every example in the specification is written that way.
            const auto headerLength = snprintf(
                m_updateHeader, sizeof(m_updateHeader),
                "{\"command\":\"notify\",\"subscribeId\":\"%s\"}", m_subscriptions[i].SubscribeId);

            if (headerLength <= 0 || static_cast<size_t>(headerLength) >= sizeof(m_updateHeader))
            {
                continue;
            }

            m_replyInitiatorMuid = m_subscriptions[i].InitiatorMuid;
            m_replyRequestId = m_nextUpdateRequestId++;

            if (m_nextUpdateRequestId > 0x7F)
            {
                m_nextUpdateRequestId = 1;
            }

            m_chunker = {};
            m_chunker.Type = ci::MessageType::PropertySubscriptionInquiry;
            m_chunker.Header = reinterpret_cast<const uint8_t*>(m_updateHeader);
            m_chunker.HeaderByteCount = static_cast<uint16_t>(headerLength);

            // A notify carries no body, so this is one message however large the resource is.
            m_nextChunk = m_chunker.Plan(AssumedInitiatorMaximumSysExSize) ? 1 : 0;

            return m_nextChunk != 0;
        }

        return false;
    }

    _Use_decl_annotations_
    void PropertyExchangeSource::SendSubscriptionReply(
        IUmpOutput& output,
        uint8_t group,
        uint32_t sourceMuid,
        const UmpDispatcher::PendingPropertyRequest& request,
        uint16_t status,
        const char* subscribeId) noexcept
    {
        char header[80]{};

        int headerLength{ 0 };

        if (subscribeId != nullptr && subscribeId[0] != '\0')
        {
            headerLength = snprintf(
                header, sizeof(header),
                "{\"status\":%u,\"subscribeId\":\"%s\"}", status, subscribeId);
        }
        else
        {
            headerLength = snprintf(header, sizeof(header), "{\"status\":%u}", status);
        }

        if (headerLength <= 0)
        {
            return;
        }

        ci::PropertyExchangeMessageFields fields{};

        fields.Type = ci::MessageType::PropertySubscriptionReply;
        fields.SourceMuid = sourceMuid;
        fields.DestinationMuid = request.InitiatorMuid;
        fields.RequestId = request.RequestId;
        fields.Header = reinterpret_cast<const uint8_t*>(header);
        fields.HeaderByteCount = static_cast<uint16_t>(headerLength);
        fields.ChunkCount = 1;
        fields.ChunkNumber = 1;

        uint8_t buffer[128]{};

        const auto written = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        if (written > 0)
        {
            UmpDispatcher::PacketizeSysEx7(output, group, buffer, written);
        }
    }

    _Use_decl_annotations_
    std::string PropertyExchangeSource::ToNarrow(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        // UTF-8, because the JSON writer escapes anything outside seven bit ASCII into the "\u"
        // form the specification asks for. Substituting a question mark here would throw the
        // character away before it ever got the chance.
        const auto required = WideCharToMultiByte(
            CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');

        (void)WideCharToMultiByte(
            CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            result.data(), required, nullptr, nullptr);

        return result;
    }
}
