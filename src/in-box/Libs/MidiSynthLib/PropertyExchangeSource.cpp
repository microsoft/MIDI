// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/PropertyExchangeSource.h"

#include "MidiSynth/ProgramList.h"

#include "MidiCiProgramList.h"

#include <cstdio>
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

    constexpr char const* ResourceNameList[]{ "ResourceList", "DeviceInfo", "ChannelList", "ProgramList" };
}

namespace MidiSynth
{
    _Use_decl_annotations_
    const char* const* PropertyExchangeSource::ResourceNames(size_t& count) noexcept
    {
        count = std::size(ResourceNameList);
        return ResourceNameList;
    }

    _Use_decl_annotations_
    void PropertyExchangeSource::Build(const DlsCollection& collection, const SynthIdentity& identity)
    {
        m_programListJson = BuildProgramListJson(collection);

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
        auto const* const names = ResourceNames(nameCount);

        m_resourceListJson.resize(ci::BuildResourceListJson(names, nameCount, nullptr, 0));
        (void)ci::BuildResourceListJson(names, nameCount, m_resourceListJson.data(), m_resourceListJson.size());
    }

    _Use_decl_annotations_
    const std::vector<char>& PropertyExchangeSource::RebuildChannelListJson(
        const SynthEngine& engine,
        const DlsCollection& collection)
    {
        ci::ChannelListEntry entries[MidiChannelCount]{};
        char titles[MidiChannelCount][16]{};
        std::string programTitles[MidiChannelCount];

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            const auto state = engine.ChannelState(channel);

            snprintf(titles[channel], sizeof(titles[channel]), "Channel %u", channel + 1u);

            entries[channel].Title = titles[channel];
            entries[channel].Channel = static_cast<uint16_t>(channel + 1);
            entries[channel].BankMsb = state.BankMsb;
            entries[channel].BankLsb = state.BankLsb;
            entries[channel].Program = state.Program;

            // Channel 10 is the drum channel by convention, and kits are addressed by a flag in
            // this sound set rather than by a bank.
            const auto* instrument = collection.FindInstrument(
                state.BankMsb, state.BankLsb, state.Program, channel == 9);

            if (instrument != nullptr)
            {
                programTitles[channel] = ToNarrow(instrument->Name);
                entries[channel].ProgramTitle = programTitles[channel].c_str();
            }
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
