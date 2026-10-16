// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "SynthPropertySource.h"

namespace ci = ::WindowsMidiServicesCapabilityInquiry;

namespace SoundFontSynth
{
    namespace
    {
        constexpr uint8_t ReplyHeaderOk[]{ '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '2', '0', '0', '}' };

        // The bank cannot change while the synthesizer exists, so what is built from it is cacheable.
        constexpr uint8_t ReplyHeaderOkCacheable[]
        {
            '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '2', '0', '0', ',',
            '"', 'c', 'a', 'c', 'h', 'e', 'T', 'i', 'm', 'e', '"', ':', '3', '6', '0', '0', '}'
        };

        constexpr uint8_t ReplyHeaderNotFound[]{ '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '4', '0', '4', '}' };

        // What an initiator can be assumed to receive. 512 is the smallest seen in practice.
        constexpr size_t AssumedInitiatorMaximumSysExSize = 512;

        // M2-103-UM section 14: a ResourceList does not list itself.
        constexpr ci::ResourceListEntry ResourceEntryList[]
        {
            { "DeviceInfo", false, false, false },
            { "ChannelList", false, true, false },
            { "ProgramList", true, false, true },
            { "ChCtrlList", true, false, false },
        };

        // RP-003 Table 1, one group per eight programs. Used only for a bank laid out as General MIDI.
        constexpr char const* GeneralMidiInstrumentGroups[]
        {
            "Piano", "Chromatic Percussion", "Organ", "Guitar", "Bass", "Strings", "Ensemble", "Brass",
            "Reed", "Pipe", "Synth Lead", "Synth Pad", "Synth Effects", "Ethnic", "Percussive", "Sound Effects",
        };

        constexpr char const* DrumKitGroup[]{ "Drum Kit" };

        // A bank with most of the 128 General MIDI programs in bank 0 is treated as General MIDI.
        constexpr size_t GeneralMidiProgramThreshold = 100;
    }

    _Use_decl_annotations_
    std::string ToUtf8(std::wstring const& text)
    {
        if (text.empty())
        {
            return {};
        }

        auto const required = WideCharToMultiByte(
            CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');

        if (WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            result.data(), required, nullptr, nullptr) != required)
        {
            return {};
        }

        return result;
    }

    _Use_decl_annotations_
    ci::ResourceListEntry const* SynthPropertySource::ResourceEntries(size_t& count) noexcept
    {
        count = std::size(ResourceEntryList);
        return ResourceEntryList;
    }

    _Use_decl_annotations_
    void SynthPropertySource::Build(SoundFont const& font, SynthIdentity const& identity, PropertyExchangeText const& text)
    {
        m_text = text;

        auto const& presets = font.Presets();

        m_melodicOrder.clear();
        m_drumOrder.clear();

        std::array<bool, 128> generalMidiPrograms{};

        for (uint32_t i = 0; i < presets.size(); i++)
        {
            if (presets[i].Bank == SoundFont::PercussionBank)
            {
                m_drumOrder.push_back(i);
            }
            else
            {
                m_melodicOrder.push_back(i);

                if (presets[i].Bank == 0)
                {
                    generalMidiPrograms[presets[i].Program & 0x7F] = true;
                }
            }
        }

        auto const byAddress = [&presets](uint32_t a, uint32_t b)
        {
            return (presets[a].Bank != presets[b].Bank)
                ? presets[a].Bank < presets[b].Bank
                : presets[a].Program < presets[b].Program;
        };

        std::stable_sort(m_melodicOrder.begin(), m_melodicOrder.end(), byAddress);
        std::stable_sort(m_drumOrder.begin(), m_drumOrder.end(), byAddress);

        m_generalMidiLayout =
            static_cast<size_t>(std::count(generalMidiPrograms.begin(), generalMidiPrograms.end(), true)) >= GeneralMidiProgramThreshold;

        m_melodicProgramListJson = BuildProgramListPage(font, ProgramListKind::Melodic, 0, SIZE_MAX);
        m_drumKitProgramListJson = BuildProgramListPage(font, ProgramListKind::DrumKits, 0, SIZE_MAX);

        ci::DeviceInfoFields info{};

        info.ManufacturerId[0] = identity.ManufacturerSysExId[0];
        info.ManufacturerId[1] = identity.ManufacturerSysExId[1];
        info.ManufacturerId[2] = identity.ManufacturerSysExId[2];
        info.Manufacturer = m_text.Manufacturer.c_str();

        info.FamilyId[0] = static_cast<uint8_t>(identity.FamilyCode & 0x7F);
        info.FamilyId[1] = static_cast<uint8_t>((identity.FamilyCode >> 7) & 0x7F);
        info.Family = m_text.Family.c_str();

        info.ModelId[0] = static_cast<uint8_t>(identity.FamilyMemberCode & 0x7F);
        info.ModelId[1] = static_cast<uint8_t>((identity.FamilyMemberCode >> 7) & 0x7F);
        info.Model = m_text.Model.c_str();

        // M2-105-UM requires these to match the software revision in the Discovery reply.
        for (size_t i = 0; i < 4; i++)
        {
            info.VersionId[i] = identity.SoftwareRevision[i];
        }

        char versionText[20]{};

        (void)snprintf(versionText, sizeof(versionText), "%u.%u.%u.%u",
            identity.SoftwareRevision[0], identity.SoftwareRevision[1],
            identity.SoftwareRevision[2], identity.SoftwareRevision[3]);

        info.Version = versionText;

        m_deviceInfoJson.resize(ci::BuildDeviceInfoJson(info, nullptr, 0));
        (void)ci::BuildDeviceInfoJson(info, m_deviceInfoJson.data(), m_deviceInfoJson.size());

        size_t resourceCount = 0;
        auto const* const resources = ResourceEntries(resourceCount);

        m_resourceListJson.resize(ci::BuildResourceListJson(resources, resourceCount, nullptr, 0));
        (void)ci::BuildResourceListJson(resources, resourceCount, m_resourceListJson.data(), m_resourceListJson.size());

        BuildControllerList();

        m_haveChannelSnapshot = false;
    }

    void SynthPropertySource::BuildControllerList()
    {
        // Defaults are what a reset returns a channel to, as the MIDI 2.0 Protocol carries them,
        // read from the engine so they cannot drift from what a reset really does.
        auto const powerUp = Synthesizer::PowerUpReport(0);

        auto const controllerDefault = [&powerUp](uint8_t controller) noexcept
            {
                for (size_t i = 0; i < ReportedControllerCount; i++)
                {
                    if (ReportedControllers[i] == controller)
                    {
                        return powerUp.Controllers[i];
                    }
                }

                return 0u;
            };

        constexpr uint32_t Center = 0x80000000u;
        constexpr uint32_t Full = 0xFFFFFFFFu;

        // Most useful first, as M2-117-UM asks, and all MIDI 2.0 Protocol: the bank select and
        // RPN selector controllers are left out because MIDI 2.0 has messages of its own for them.
        // The synth sends no controllers at all, which "transmit":"none" has to say. Values are
        // used at full resolution except where the engine reads only the top bits.
        ci::ControllerListEntry const entries[]
        {
            { .Title = m_text.Volume.c_str(), .ControllerType = "cc", .Index = { 7 }, .IndexCount = 1, .Priority = 1,
              .HasDefault = true, .Default = controllerDefault(7), .Transmit = "none",
              .ParameterPath = "/volume", .TypeHint = "continuous" },

            { .Title = m_text.Modulation.c_str(), .ControllerType = "cc", .Index = { 1 }, .IndexCount = 1, .Priority = 1,
              .HasDefault = true, .Default = controllerDefault(1), .Transmit = "none",
              .ParameterPath = "/modulation", .TypeHint = "continuous" },

            { .Title = m_text.PitchBend.c_str(), .ControllerType = "pBend", .Priority = 1,
              .HasDefault = true, .Default = powerUp.PitchBend, .Transmit = "none",
              .ParameterPath = "/pitchBend", .TypeHint = "continuous" },

            { .Title = m_text.SustainPedal.c_str(), .ControllerType = "cc", .Index = { 64 }, .IndexCount = 1, .Priority = 1,
              .HasDefault = true, .Default = controllerDefault(64), .Transmit = "none",
              .SignificantBits = 1, .ParameterPath = "/sustain", .TypeHint = "momentary" },

            { .Title = m_text.Pan.c_str(), .ControllerType = "cc", .Index = { 10 }, .IndexCount = 1, .Priority = 2,
              .HasDefault = true, .Default = controllerDefault(10), .Transmit = "none",
              .ParameterPath = "/pan", .TypeHint = "continuous" },

            { .Title = m_text.Expression.c_str(), .ControllerType = "cc", .Index = { 11 }, .IndexCount = 1, .Priority = 2,
              .HasDefault = true, .Default = controllerDefault(11), .Transmit = "none",
              .ParameterPath = "/expression", .TypeHint = "continuous" },

            { .Title = m_text.ReverbSend.c_str(), .ControllerType = "cc", .Index = { 91 }, .IndexCount = 1, .Priority = 3,
              .HasDefault = true, .Default = controllerDefault(91), .Transmit = "none",
              .ParameterPath = "/effects/reverb", .TypeHint = "continuous" },

            { .Title = m_text.ChorusSend.c_str(), .ControllerType = "cc", .Index = { 93 }, .IndexCount = 1, .Priority = 3,
              .HasDefault = true, .Default = controllerDefault(93), .Transmit = "none",
              .ParameterPath = "/effects/chorus", .TypeHint = "continuous" },

            // Vibrato depth, through the SoundFont default modulator.
            { .Title = m_text.ChannelPressure.c_str(), .ControllerType = "chPress", .Priority = 3,
              .HasDefault = true, .Default = powerUp.ChannelPressure, .Transmit = "none",
              .ParameterPath = "/pressure", .TypeHint = "continuous" },

            { .Title = m_text.SostenutoPedal.c_str(), .ControllerType = "cc", .Index = { 66 }, .IndexCount = 1, .Priority = 3,
              .HasDefault = true, .Default = controllerDefault(66), .Transmit = "none",
              .SignificantBits = 1, .ParameterPath = "/sostenuto", .TypeHint = "momentary" },

            { .Title = m_text.NotePitchBend.c_str(), .ControllerType = "pnp", .Priority = 3,
              .HasDefault = true, .Default = Center, .Transmit = "none",
              .ParameterPath = "/note/pitchBend", .TypeHint = "continuous" },

            // Semitones and cents, the MIDI 1.0 data entry MSB and LSB, in the top fourteen bits.
            { .Title = m_text.PitchBendSensitivity.c_str(), .ControllerType = "rpn", .Index = { 0, 0 }, .IndexCount = 2, .Priority = 4,
              .HasDefault = true, .Default = powerUp.PitchBendSensitivity, .Transmit = "none",
              .SignificantBits = 14, .ParameterPath = "/pitchBend/sensitivity", .TypeHint = "continuous" },

            { .Title = m_text.FineTuning.c_str(), .ControllerType = "rpn", .Index = { 0, 1 }, .IndexCount = 2, .Priority = 4,
              .HasDefault = true, .Default = powerUp.FineTuning, .Transmit = "none",
              .ParameterPath = "/tuning/fine", .TypeHint = "continuous" },

            // Whole semitones in the top seven bits.
            { .Title = m_text.CoarseTuning.c_str(), .ControllerType = "rpn", .Index = { 0, 2 }, .IndexCount = 2, .Priority = 4,
              .HasDefault = true, .Default = powerUp.CoarseTuning, .Transmit = "none",
              .SignificantBits = 7, .ParameterPath = "/tuning/coarse", .TypeHint = "continuous" },

            { .Title = m_text.NoteVolume.c_str(), .ControllerType = "pnrc", .Index = { PerNoteControllerVolume }, .IndexCount = 1, .Priority = 4,
              .HasDefault = true, .Default = Full, .Transmit = "none",
              .ParameterPath = "/note/volume", .TypeHint = "continuous" },

            { .Title = m_text.NotePan.c_str(), .ControllerType = "pnrc", .Index = { PerNoteControllerPan }, .IndexCount = 1, .Priority = 4,
              .HasDefault = true, .Default = Center, .Transmit = "none",
              .ParameterPath = "/note/pan", .TypeHint = "continuous" },

            // An absolute pitch, so the only default is the note's own.
            { .Title = m_text.NotePitch.c_str(), .ControllerType = "pnrc", .Index = { PerNoteControllerPitch }, .IndexCount = 1, .Priority = 4,
              .Transmit = "none", .ParameterPath = "/note/pitch", .TypeHint = "continuous" },
        };

        m_controllerListJson.resize(ci::BuildControllerListJson(entries, std::size(entries), nullptr, 0));
        (void)ci::BuildControllerListJson(entries, std::size(entries), m_controllerListJson.data(), m_controllerListJson.size());
    }

    _Use_decl_annotations_
    std::vector<char> SynthPropertySource::BuildProgramListPage(
        SoundFont const& font,
        ProgramListKind kind,
        size_t offset,
        size_t limit) const
    {
        auto const& order = (kind == ProgramListKind::DrumKits) ? m_drumOrder : m_melodicOrder;
        auto const& presets = font.Presets();

        auto const first = (std::min)(offset, order.size());
        auto const count = (std::min)(limit, order.size() - first);

        std::vector<std::string> titles;
        std::vector<std::string> tags;
        std::vector<ci::ProgramListEntry> entries;

        titles.reserve(count);
        tags.reserve(count);
        entries.reserve(count);

        for (size_t i = 0; i < count; i++)
        {
            auto const& preset = presets[order[first + i]];

            titles.push_back(ToUtf8(preset.Name));

            if (kind == ProgramListKind::Melodic && preset.Bank != 0)
            {
                tags.push_back("Bank " + std::to_string(preset.Bank));
            }
            else
            {
                tags.emplace_back();
            }
        }

        for (size_t i = 0; i < count; i++)
        {
            auto const& preset = presets[order[first + i]];

            ci::ProgramListEntry entry{};

            entry.Title = titles[i].c_str();
            entry.Program = static_cast<uint8_t>(preset.Program & 0x7F);

            if (kind == ProgramListKind::Melodic)
            {
                entry.BankMsb = static_cast<uint8_t>(preset.Bank & 0x7F);
                entry.BankLsb = 0;
                entry.Tag = tags[i].empty() ? nullptr : tags[i].c_str();

                if (m_generalMidiLayout)
                {
                    entry.Categories = &GeneralMidiInstrumentGroups[entry.Program / 8];
                    entry.CategoryCount = 1;
                }
            }
            else
            {
                // A kit is selected with a program change on a drum channel, whatever the bank.
                entry.BankMsb = 0;
                entry.BankLsb = 0;
                entry.Categories = DrumKitGroup;
                entry.CategoryCount = std::size(DrumKitGroup);
            }

            entries.push_back(entry);
        }

        auto const required = ci::BuildProgramListJson(entries.data(), entries.size(), nullptr, 0);

        if (required == 0)
        {
            return {};
        }

        std::vector<char> json(required);

        if (ci::BuildProgramListJson(entries.data(), entries.size(), json.data(), json.size()) != required)
        {
            return {};
        }

        return json;
    }

    _Use_decl_annotations_
    std::vector<char> const& SynthPropertySource::RebuildChannelListJson(Synthesizer const& synthesizer, SoundFont const& font)
    {
        ci::ChannelListEntry entries[MidiChannelCount]{};
        std::string programTitles[MidiChannelCount];

        // One set of links per kind, shared by every channel that uses it. They outlive the build.
        ci::ResourceLink const MelodicLinks[]
        {
            { "ProgramList", MelodicProgramListResourceId, m_text.MelodicProgramList.c_str() },
            { "ChCtrlList", ControllerListResourceId, m_text.ControllerList.c_str() },
        };

        ci::ResourceLink const DrumKitLinks[]
        {
            { "ProgramList", DrumKitProgramListResourceId, m_text.DrumKitProgramList.c_str() },
            { "ChCtrlList", ControllerListResourceId, m_text.ControllerList.c_str() },
        };

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            auto const selection = synthesizer.Selection(channel);

            // A bank select can make any channel a drum channel under XG and GM2, so the engine's
            // own flag decides rather than the channel number.
            entries[channel].Title = selection.IsDrumChannel ? m_text.PercussionChannel.c_str() : m_text.MelodicChannel.c_str();
            entries[channel].Channel = static_cast<uint16_t>(channel + 1);
            entries[channel].BankMsb = selection.BankMsb;
            entries[channel].BankLsb = selection.BankLsb;
            entries[channel].Program = selection.Program;

            if (selection.PresetIndex >= 0 && static_cast<size_t>(selection.PresetIndex) < font.Presets().size())
            {
                programTitles[channel] = ToUtf8(font.Presets()[static_cast<size_t>(selection.PresetIndex)].Name);
                entries[channel].ProgramTitle = programTitles[channel].c_str();
            }

            entries[channel].Links = selection.IsDrumChannel ? DrumKitLinks : MelodicLinks;
            entries[channel].LinkCount = std::size(MelodicLinks);
        }

        m_channelListJson.resize(ci::BuildChannelListJson(entries, MidiChannelCount, nullptr, 0));

        (void)ci::BuildChannelListJson(entries, MidiChannelCount, m_channelListJson.data(), m_channelListJson.size());

        return m_channelListJson;
    }

    _Use_decl_annotations_
    void SynthPropertySource::BeginReply(
        SynthDispatcher::PendingPropertyRequest const& request,
        std::vector<char> const& resource,
        bool cacheable) noexcept
    {
        m_replyInitiatorMuid = request.InitiatorMuid;
        m_replyRequestId = request.RequestId;

        m_chunker = {};
        m_chunker.Resource = reinterpret_cast<uint8_t const*>(resource.data());
        m_chunker.ResourceByteCount = resource.size();
        m_chunker.Header = cacheable ? ReplyHeaderOkCacheable : ReplyHeaderOk;
        m_chunker.HeaderByteCount = static_cast<uint16_t>(cacheable ? sizeof(ReplyHeaderOkCacheable) : sizeof(ReplyHeaderOk));

        m_nextChunk = m_chunker.Plan(AssumedInitiatorMaximumSysExSize) ? 1 : 0;
    }

    _Use_decl_annotations_
    void SynthPropertySource::BeginProgramListReply(
        SoundFont const& font,
        SynthDispatcher::PendingPropertyRequest const& request,
        std::string const& resourceId,
        size_t offset,
        size_t limit) noexcept
    {
        try
        {
            auto const kind = (resourceId == DrumKitProgramListResourceId) ? ProgramListKind::DrumKits : ProgramListKind::Melodic;
            auto const total = ProgramCount(kind);
            auto const wholeList = (offset == 0 && limit >= total);

            if (!wholeList)
            {
                m_programPageJson = BuildProgramListPage(font, kind, offset, limit);
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
            m_chunker.Resource = reinterpret_cast<uint8_t const*>(resource.data());
            m_chunker.ResourceByteCount = resource.size();
            m_chunker.Header = reinterpret_cast<uint8_t const*>(m_replyHeader);
            m_chunker.HeaderByteCount = static_cast<uint16_t>(headerLength);

            m_nextChunk = m_chunker.Plan(AssumedInitiatorMaximumSysExSize) ? 1 : 0;
        }
        catch (...)
        {
            m_nextChunk = 0;
        }
    }

    _Use_decl_annotations_
    bool SynthPropertySource::SendNextChunk(ISysExSink& output, uint32_t sourceMuid) noexcept
    {
        if (m_nextChunk == 0)
        {
            return false;
        }

        uint8_t buffer[640]{};

        auto const written = m_chunker.BuildChunk(
            m_nextChunk, sourceMuid, m_replyInitiatorMuid, m_replyRequestId, buffer, sizeof(buffer));

        if (written == 0)
        {
            m_nextChunk = 0;
            return false;
        }

        output.SendSysEx(buffer, written);

        m_nextChunk = (m_nextChunk >= m_chunker.ChunkCount) ? 0 : static_cast<uint16_t>(m_nextChunk + 1);

        return m_nextChunk != 0;
    }

    _Use_decl_annotations_
    void SynthPropertySource::SendNotFound(
        ISysExSink& output,
        uint32_t sourceMuid,
        SynthDispatcher::PendingPropertyRequest const& request) noexcept
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

        auto const written = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        if (written > 0)
        {
            output.SendSysEx(buffer, written);
        }
    }

    _Use_decl_annotations_
    char const* SynthPropertySource::AddChannelListSubscription(uint32_t initiatorMuid) noexcept
    {
        for (size_t i = 0; i < m_subscriptionCount; i++)
        {
            if (m_subscriptions[i].InitiatorMuid == initiatorMuid)
            {
                m_subscriptions[i].NeedsUpdate = false;
                return m_subscriptions[i].SubscribeId;
            }
        }

        if (m_subscriptionCount >= MaximumSubscriptions)
        {
            return "";
        }

        auto& added = m_subscriptions[m_subscriptionCount];

        added = {};
        added.InitiatorMuid = initiatorMuid;

        // A subscribeId is at most eight characters, so the counter wraps before it would need a
        // sixth digit.
        if (m_nextSubscribeId > 99999)
        {
            m_nextSubscribeId = 1;
        }

        (void)snprintf(added.SubscribeId, sizeof(added.SubscribeId), "ch%u", m_nextSubscribeId++);

        m_subscriptionCount++;

        return added.SubscribeId;
    }

    _Use_decl_annotations_
    bool SynthPropertySource::RemoveSubscription(uint32_t initiatorMuid, std::string const& subscribeId) noexcept
    {
        bool removed{ false };

        for (size_t i = 0; i < m_subscriptionCount; )
        {
            auto const matches =
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
    bool SynthPropertySource::ChannelListChanged(Synthesizer const& synthesizer) noexcept
    {
        ChannelSelection current[MidiChannelCount]{};
        bool changed = !m_haveChannelSnapshot;

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            current[channel] = synthesizer.Selection(channel);

            auto const& last = m_lastNotifiedChannels[channel];

            changed = changed ||
                current[channel].BankMsb != last.BankMsb ||
                current[channel].BankLsb != last.BankLsb ||
                current[channel].Program != last.Program ||
                current[channel].IsDrumChannel != last.IsDrumChannel ||
                current[channel].PresetIndex != last.PresetIndex;
        }

        if (!changed)
        {
            return false;
        }

        std::copy(std::begin(current), std::end(current), std::begin(m_lastNotifiedChannels));

        // The first pass only sets the baseline. Nobody has asked about anything yet.
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

    bool SynthPropertySource::BeginNextSubscriptionNotification() noexcept
    {
        for (size_t i = 0; i < m_subscriptionCount; i++)
        {
            if (!m_subscriptions[i].NeedsUpdate)
            {
                continue;
            }

            m_subscriptions[i].NeedsUpdate = false;

            // M2-103-UM section 7.1: the command comes first in every subscription message.
            auto const headerLength = snprintf(
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
            m_chunker.Header = reinterpret_cast<uint8_t const*>(m_updateHeader);
            m_chunker.HeaderByteCount = static_cast<uint16_t>(headerLength);

            m_nextChunk = m_chunker.Plan(AssumedInitiatorMaximumSysExSize) ? 1 : 0;

            return m_nextChunk != 0;
        }

        return false;
    }

    _Use_decl_annotations_
    void SynthPropertySource::SendSubscriptionReply(
        ISysExSink& output,
        uint32_t sourceMuid,
        SynthDispatcher::PendingPropertyRequest const& request,
        uint16_t status,
        char const* subscribeId) noexcept
    {
        char header[80]{};

        int headerLength{ 0 };

        if (subscribeId != nullptr && subscribeId[0] != '\0')
        {
            headerLength = snprintf(header, sizeof(header), "{\"status\":%u,\"subscribeId\":\"%s\"}", status, subscribeId);
        }
        else
        {
            headerLength = snprintf(header, sizeof(header), "{\"status\":%u}", status);
        }

        if (headerLength <= 0 || static_cast<size_t>(headerLength) >= sizeof(header))
        {
            return;
        }

        ci::PropertyExchangeMessageFields fields{};

        fields.Type = ci::MessageType::PropertySubscriptionReply;
        fields.SourceMuid = sourceMuid;
        fields.DestinationMuid = request.InitiatorMuid;
        fields.RequestId = request.RequestId;
        fields.Header = reinterpret_cast<uint8_t const*>(header);
        fields.HeaderByteCount = static_cast<uint16_t>(headerLength);
        fields.ChunkCount = 1;
        fields.ChunkNumber = 1;

        uint8_t buffer[128]{};

        auto const written = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        if (written > 0)
        {
            output.SendSysEx(buffer, written);
        }
    }
}
