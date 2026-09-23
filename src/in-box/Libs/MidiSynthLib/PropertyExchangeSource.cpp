// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/PropertyExchangeSource.h"

#include "MidiSynth/ProgramList.h"

#include "MidiCiProgramList.h"

#include <cstdio>
#include <cstring>
#include <iterator>

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

    constexpr ci::ResourceListEntry ResourceEntryList[]
    {
        { "ResourceList", false, false },
        { "DeviceInfo", false, false },
        { "ChannelList", false, true },
        { "ProgramList", true, false },
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
    bool PropertyExchangeSource::BeginNextSubscriptionUpdate(
        const SynthEngine& engine,
        const DlsCollection& collection) noexcept
    {
        for (size_t i = 0; i < m_subscriptionCount; i++)
        {
            if (!m_subscriptions[i].NeedsUpdate)
            {
                continue;
            }

            m_subscriptions[i].NeedsUpdate = false;

            // Rebuilt here rather than when the change was noticed, because the chunker points
            // into this buffer and a resize while a reply is in flight would move it.
            const auto& resource = RebuildChannelListJson(engine, collection);

            const auto headerLength = snprintf(
                m_updateHeader, sizeof(m_updateHeader),
                "{\"subscribeId\":\"%s\",\"command\":\"full\"}", m_subscriptions[i].SubscribeId);

            if (headerLength <= 0)
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
            m_chunker.Resource = reinterpret_cast<const uint8_t*>(resource.data());
            m_chunker.ResourceByteCount = resource.size();
            m_chunker.Header = reinterpret_cast<const uint8_t*>(m_updateHeader);
            m_chunker.HeaderByteCount = static_cast<uint16_t>(headerLength);

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
        std::string result;

        for (const auto character : text)
        {
            result += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
        }

        return result;
    }
}
