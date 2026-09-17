#include "MidiSynth/UmpDispatcher.h"

#include <libmidi2/utils.h>

#include <algorithm>

namespace MidiSynth
{
    namespace
    {
        constexpr uint32_t MessageTypeUtility = 0x0;
        constexpr uint32_t MessageTypeSystem = 0x1;
        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeSysEx7 = 0x3;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        constexpr uint8_t StatusRegisteredController = 0x2;

        // MIDI 2.0 channel voice statuses that have no MIDI 1.0 equivalent at all.
        constexpr uint8_t StatusRegisteredPerNoteController = 0x0;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;
        constexpr uint8_t StatusPerNoteManagement = 0xF;

        // Note On attribute types. Only the pitch attribute changes what is heard here; the others
        // describe the source instrument rather than the sound, so they are carried no further.
        constexpr uint8_t NoteAttributePitch79 = 0x03;

        // Per-Note Management option flags.
        constexpr uint8_t PerNoteManagementReset = 0x01;
        constexpr uint8_t PerNoteManagementDetach = 0x02;

        constexpr uint8_t SystemReset = 0xFF;
        constexpr uint8_t ActiveSensing = 0xFE;

        // A malformed or hostile stream must not be able to grow this without bound.
        constexpr size_t MaxSysExBytes = 1024;

        constexpr uint32_t MessageType(uint32_t word) noexcept { return (word >> 28) & 0xF; }
        constexpr uint8_t Group(uint32_t word) noexcept { return static_cast<uint8_t>((word >> 24) & 0xF); }
        constexpr uint8_t Status(uint32_t word) noexcept { return static_cast<uint8_t>((word >> 20) & 0xF); }
        constexpr uint8_t Channel(uint32_t word) noexcept { return static_cast<uint8_t>((word >> 16) & 0xF); }
        constexpr uint8_t Data1(uint32_t word) noexcept { return static_cast<uint8_t>((word >> 8) & 0x7F); }
        constexpr uint8_t Data2(uint32_t word) noexcept { return static_cast<uint8_t>(word & 0x7F); }
    }

    _Use_decl_annotations_
    void UmpDispatcher::Initialize(SynthEngine* engine, uint8_t group, uint32_t muid) noexcept
    {
        m_engine = engine;
        m_group = group & 0xF;
        m_sysexLength = 0;
        m_stats = {};

        ConfigureResponder(muid);
    }

    _Use_decl_annotations_
    void UmpDispatcher::SetOutput(IUmpOutput* output, const SynthIdentity& identity) noexcept
    {
        m_output = output;
        m_identity = identity;

        // The responder was configured before the identity arrived, so rebuild it.
        ConfigureResponder(m_responder.Muid());
    }

    _Use_decl_annotations_
    void UmpDispatcher::SetMuid(uint32_t muid) noexcept
    {
        m_responder.SetMuid(muid & 0x0FFFFFFF);
    }

    _Use_decl_annotations_
    void UmpDispatcher::SendIdentityReply(uint8_t requestedDeviceId) noexcept
    {
        // Answering with an identifier that is not ours would be worse than not answering.
        if (m_output == nullptr || !m_identity.IsConfigured())
        {
            m_stats.Ignored++;
            return;
        }

        const uint8_t deviceId = (requestedDeviceId == 0x7F) ? m_identity.DeviceId : requestedDeviceId;

        // A manufacturer identifier is one byte, or three bytes when the first is zero.
        const bool extendedId = (m_identity.ManufacturerSysExId[0] == 0);

        // Sized well past the fifteen bytes this builds, so adding a field later cannot overflow.
        uint8_t payload[32]{};
        size_t total = 0;

        payload[total++] = 0x7E;
        payload[total++] = static_cast<uint8_t>(deviceId & 0x7F);
        payload[total++] = 0x06;
        payload[total++] = 0x02;

        payload[total++] = m_identity.ManufacturerSysExId[0];

        if (extendedId)
        {
            payload[total++] = m_identity.ManufacturerSysExId[1];
            payload[total++] = m_identity.ManufacturerSysExId[2];
        }

        payload[total++] = static_cast<uint8_t>(m_identity.FamilyCode & 0x7F);
        payload[total++] = static_cast<uint8_t>((m_identity.FamilyCode >> 7) & 0x7F);
        payload[total++] = static_cast<uint8_t>(m_identity.FamilyMemberCode & 0x7F);
        payload[total++] = static_cast<uint8_t>((m_identity.FamilyMemberCode >> 7) & 0x7F);

        for (const auto revision : m_identity.SoftwareRevision)
        {
            payload[total++] = revision & 0x7F;
        }

        SendSysEx7(payload, total);

        m_stats.IdentityRepliesSent++;
    }

    _Use_decl_annotations_
    void UmpDispatcher::HandleGlobalParameterControl(const uint8_t* message, size_t size) noexcept
    {
        // 7F <device> 04 05 <slot path length> <parameter id width> <value width> <slot path...>
        // then parameter and value pairs. Only the widths GM2 defines for reverb and chorus are
        // handled; anything else is left alone rather than guessed at.
        constexpr size_t HeaderSize = 7;

        if (size < HeaderSize + 2)
        {
            m_stats.Ignored++;
            return;
        }

        const size_t slotPathLength = message[4];
        const size_t parameterWidth = message[5];
        const size_t valueWidth = message[6];

        if (slotPathLength != 1 || parameterWidth != 1 || valueWidth != 1)
        {
            m_stats.Ignored++;
            return;
        }

        const size_t slotPathOffset = HeaderSize;

        if (size <= slotPathOffset)
        {
            m_stats.Ignored++;
            return;
        }

        // GM2 slot 1 is reverb, slot 2 is chorus.
        const uint8_t slot = message[slotPathOffset];

        IAudioEffect* effect = nullptr;

        if (slot == 0x01)
        {
            effect = m_engine->Reverb();
        }
        else if (slot == 0x02)
        {
            effect = m_engine->Chorus();
        }

        if (effect == nullptr)
        {
            m_stats.Ignored++;
            return;
        }

        for (size_t offset = slotPathOffset + 1; offset + 1 < size; offset += 2)
        {
            const uint8_t parameter = message[offset] & 0x7F;
            const double normalized = static_cast<double>(message[offset + 1] & 0x7F) / 127.0;

            switch (parameter)
            {
            case 0x00:
                // Type selects a preset character; mapped onto damping, which is what actually
                // distinguishes the GM2 reverb types audibly.
                effect->SetParameter(AudioEffectParameter::Damping, normalized);
                break;

            case 0x01:
                // Time, scaled across the useful range rather than the full parameter range.
                effect->SetParameter(AudioEffectParameter::Time, 0.2 + normalized * 4.0);
                break;

            default:
                break;
            }
        }
    }

    _Use_decl_annotations_
    void UmpDispatcher::HandleMidiCi(const uint8_t* message, size_t size) noexcept
    {
        namespace ci = WindowsMidiServicesCapabilityInquiry;

        ci::ParsedMessage parsed{};

        if (ci::Parse(message, size, parsed) != ci::ParseStatus::Ok)
        {
            m_stats.Malformed++;
            return;
        }

        uint8_t reply[ci::DiscoveryReplyByteCount]{};
        size_t replyBytes{ 0 };

        const auto action = m_responder.ProcessMessage(parsed, reply, sizeof(reply), &replyBytes);

        switch (action)
        {
        case ci::ResponderAction::Replied:
            // An identity we were never given would go out as a device claiming to be nothing.
            if (!m_identity.IsConfigured() || m_output == nullptr)
            {
                m_stats.Ignored++;
                return;
            }

            SendSysEx7(reply, replyBytes);
            m_stats.DiscoveryRepliesSent++;
            return;

        case ci::ResponderAction::MuidInvalidated:
            m_stats.MuidInvalidations++;
            return;

        case ci::ResponderAction::PropertyDataRequested:
            ParkPropertyRequest(parsed, message);
            return;

        default:
            m_stats.Ignored++;
            return;
        }
    }

    _Use_decl_annotations_
    void UmpDispatcher::ParkPropertyRequest(
        const WindowsMidiServicesCapabilityInquiry::ParsedMessage& parsed,
        const uint8_t* message) noexcept
    {
        const auto headerBytes = parsed.PropertyExchange.HeaderByteCount;

        if (headerBytes > MaxPropertyHeaderBytes)
        {
            m_stats.Ignored++;
            return;
        }

        // Still holding the previous request. Dropping this one is correct: an initiator retries,
        // and overwriting would corrupt what the worker is already reading.
        if (m_propertyRequestPending.load(std::memory_order_acquire))
        {
            m_stats.Ignored++;
            return;
        }

        m_propertyRequest.InitiatorMuid = parsed.SourceMuid;
        m_propertyRequest.RequestId = parsed.PropertyExchange.RequestId;
        m_propertyRequest.HeaderByteCount = headerBytes;

        for (uint16_t i = 0; i < headerBytes; i++)
        {
            m_propertyRequest.Header[i] = message[parsed.PropertyExchange.HeaderOffset + i];
        }

        m_propertyRequestPending.store(true, std::memory_order_release);

        m_stats.PropertyRequests++;
    }

    _Use_decl_annotations_
    bool UmpDispatcher::TakePendingPropertyRequest(PendingPropertyRequest& request) noexcept
    {
        if (!m_propertyRequestPending.load(std::memory_order_acquire))
        {
            return false;
        }

        request = m_propertyRequest;

        m_propertyRequestPending.store(false, std::memory_order_release);

        return true;
    }

    _Use_decl_annotations_
    void UmpDispatcher::ConfigureResponder(uint32_t muid) noexcept
    {
        namespace ci = WindowsMidiServicesCapabilityInquiry;

        ci::ResponderConfig config{};

        config.Muid = muid & 0x0FFFFFFF;

        config.ManufacturerSysExId[0] = m_identity.ManufacturerSysExId[0];
        config.ManufacturerSysExId[1] = m_identity.ManufacturerSysExId[1];
        config.ManufacturerSysExId[2] = m_identity.ManufacturerSysExId[2];

        config.DeviceFamily = m_identity.FamilyCode;
        config.DeviceFamilyModelNumber = m_identity.FamilyMemberCode;

        for (size_t i = 0; i < 4; i++)
        {
            config.SoftwareRevisionLevel[i] = m_identity.SoftwareRevision[i];
        }

        config.ReceivableMaximumSysExSize = MaxSysExBytes;
        config.FunctionBlockNumber = SynthEndpoint::FunctionBlockNumber;
        config.SupportsPropertyExchange = true;

        m_responder.Initialize(config);
    }

    _Use_decl_annotations_
    void UmpDispatcher::SendSysEx7(const uint8_t* payload, size_t total) noexcept
    {
        if (m_output == nullptr || total == 0)
        {
            return;
        }

        PacketizeSysEx7(*m_output, m_group, payload, total);
    }

    _Use_decl_annotations_
    void UmpDispatcher::PacketizeSysEx7(
        IUmpOutput& output, uint8_t group, const uint8_t* payload, size_t total) noexcept
    {
        if (total == 0)
        {
            return;
        }

        constexpr size_t BytesPerPacket = 6;

        for (size_t offset = 0; offset < total; offset += BytesPerPacket)
        {
            const auto count = static_cast<uint8_t>((std::min)(BytesPerPacket, total - offset));
            const bool isFirst = (offset == 0);
            const bool isLast = (offset + count >= total);

            // 0 complete, 1 start, 2 continue, 3 end.
            const uint8_t status =
                (isFirst && isLast) ? 0 : isFirst ? 1 : isLast ? 3 : 2;

            uint8_t bytes[BytesPerPacket]{};

            for (uint8_t i = 0; i < count; i++)
            {
                bytes[i] = payload[offset + i];
            }

            const uint32_t words[2] =
            {
                (3u << 28) |
                (static_cast<uint32_t>(group) << 24) |
                (static_cast<uint32_t>(status) << 20) |
                (static_cast<uint32_t>(count) << 16) |
                (static_cast<uint32_t>(bytes[0]) << 8) |
                bytes[1],

                (static_cast<uint32_t>(bytes[2]) << 24) |
                (static_cast<uint32_t>(bytes[3]) << 16) |
                (static_cast<uint32_t>(bytes[4]) << 8) |
                bytes[5],
            };

            output.SendUmp(words, 2);
        }
    }

    _Use_decl_annotations_
    uint32_t UmpDispatcher::PacketWordCount(uint32_t firstWord) noexcept
    {
        switch (MessageType(firstWord))
        {
        case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
            return 1;
        case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
            return 2;
        case 0xB: case 0xC:
            return 3;
        case 0x5: case 0xD: case 0xE: case 0xF:
            return 4;
        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    uint32_t UmpDispatcher::ProcessWords(const uint32_t* words, uint32_t wordCount) noexcept
    {
        if (m_engine == nullptr || words == nullptr)
        {
            return 0;
        }

        uint32_t consumed = 0;

        while (consumed < wordCount)
        {
            const uint32_t word0 = words[consumed];
            const uint32_t packetWords = PacketWordCount(word0);

            if (packetWords == 0)
            {
                m_stats.Malformed++;
                consumed++;
                continue;
            }

            // A partial packet at the end is left for the caller to complete.
            if (consumed + packetWords > wordCount)
            {
                break;
            }

            // Other groups belong to other function blocks and are not ours to act on.
            if (Group(word0) != m_group && MessageType(word0) != MessageTypeUtility)
            {
                m_stats.Ignored++;
                consumed += packetWords;
                continue;
            }

            switch (MessageType(word0))
            {
            case MessageTypeUtility:
                m_stats.Utility++;
                break;

            case MessageTypeSystem:
                HandleSystem(word0);
                break;

            case MessageTypeMidi1ChannelVoice:
                HandleMidi1ChannelVoice(word0);
                break;

            case MessageTypeSysEx7:
                HandleSysEx7(word0, words[consumed + 1]);
                break;

            case MessageTypeMidi2ChannelVoice:
                HandleMidi2ChannelVoice(word0, words[consumed + 1]);
                break;

            default:
                m_stats.Ignored++;
                break;
            }

            consumed += packetWords;
        }

        return consumed;
    }

    _Use_decl_annotations_
    void UmpDispatcher::HandleMidi1ChannelVoice(uint32_t word) noexcept
    {
        m_stats.Midi1ChannelVoice++;

        const uint8_t channel = Channel(word);
        const uint8_t data1 = Data1(word);
        const uint8_t data2 = Data2(word);

        switch (Status(word))
        {
        case StatusNoteOff:
            m_engine->NoteOff(channel, data1);
            break;

        case StatusNoteOn:
            if (data2 == 0)
            {
                // In MIDI 1.0 a note on with zero velocity is a note off.
                m_engine->NoteOff(channel, data1);
            }
            else
            {
                // The specification's scaling preserves the center value, which a plain bit
                // repeat does not. Use the library the rest of the stack uses.
                m_engine->NoteOn(channel, data1,
                    static_cast<uint16_t>(M2Utils::scaleUp(data2, 7, 16)));
            }
            break;

        case StatusControlChange:
            m_engine->ControlChange(channel, data1, data2);
            break;

        case StatusProgramChange:
            m_engine->ProgramChange(channel, data1);
            break;

        case StatusPitchBend:
            m_engine->PitchBend(channel, (data2 << 7) | data1);
            break;

        case StatusPolyPressure:
        case StatusChannelPressure:
        default:
            m_stats.Ignored++;
            break;
        }
    }

    _Use_decl_annotations_
    void UmpDispatcher::HandleMidi2ChannelVoice(uint32_t word0, uint32_t word1) noexcept
    {
        m_stats.Midi2ChannelVoice++;

        const uint8_t channel = Channel(word0);
        const uint8_t index1 = static_cast<uint8_t>((word0 >> 8) & 0xFF);

        switch (Status(word0))
        {
        case StatusNoteOff:
            m_engine->NoteOff(channel, index1 & 0x7F);
            break;

        case StatusNoteOn:
        {
            // Unlike MIDI 1.0, a MIDI 2.0 note on with zero velocity is still a note on.
            const auto velocity = static_cast<uint16_t>((word1 >> 16) & 0xFFFF);
            const uint8_t note = index1 & 0x7F;
            const uint8_t attributeType = static_cast<uint8_t>(word0 & 0xFF);
            const auto attributeData = static_cast<uint16_t>(word1 & 0xFFFF);

            if (attributeType == NoteAttributePitch79)
            {
                // Pitch 7.9: the top seven bits are a note number and the low nine bits are the
                // fraction of a semitone above it. The note number in the message still chooses
                // the region, so a microtonal scale keeps the articulation belonging to the key.
                const double pitch =
                    static_cast<double>((attributeData >> 9) & 0x7F) +
                    static_cast<double>(attributeData & 0x01FF) / 512.0;

                m_engine->NoteOnWithPitch(channel, note, velocity, pitch);
            }
            else
            {
                m_engine->NoteOn(channel, note, velocity);
            }
            break;
        }

        case StatusControlChange:
            m_engine->ControlChange32(channel, index1 & 0x7F, word1);
            break;

        case StatusProgramChange:
        {
            const uint8_t program = static_cast<uint8_t>((word1 >> 24) & 0x7F);
            const bool bankValid = (word0 & 0x1) != 0;

            if (bankValid)
            {
                m_engine->ProgramChangeWithBank(channel,
                    static_cast<uint8_t>((word1 >> 8) & 0x7F),
                    static_cast<uint8_t>(word1 & 0x7F),
                    program);
            }
            else
            {
                m_engine->ProgramChange(channel, program);
            }
            break;
        }

        case StatusPitchBend:
            m_engine->PitchBend32(channel, word1);
            break;

        case StatusRegisteredController:
        {
            const uint8_t bank = index1 & 0x7F;
            const uint8_t index = static_cast<uint8_t>(word0 & 0x7F);

            // RPN 0,0 is pitch bend sensitivity. The top seven bits carry whole semitones, which
            // is the same information the MIDI 1.0 data entry most significant byte carries.
            if (bank == 0 && index == 0)
            {
                m_engine->ControlChange(channel, 101, 0);
                m_engine->ControlChange(channel, 100, 0);
                m_engine->ControlChange(channel, 6, static_cast<uint8_t>((word1 >> 25) & 0x7F));
            }
            else
            {
                m_stats.Ignored++;
            }
            break;
        }

        case StatusRegisteredPerNoteController:
            m_engine->PerNoteController(
                channel,
                index1 & 0x7F,
                static_cast<uint8_t>(word0 & 0xFF),
                word1);
            break;

        case StatusPerNotePitchBend:
        {
            // Centered at the midpoint of the thirty two bit range, exactly like the channel bend.
            constexpr double center = 2147483648.0;
            const double normalized = (static_cast<double>(word1) - center) / center;

            m_engine->PerNotePitchBend(channel, index1 & 0x7F, normalized);
            break;
        }

        case StatusPerNoteManagement:
        {
            const uint8_t flags = static_cast<uint8_t>(word0 & 0xFF);

            m_engine->PerNoteManagement(
                channel,
                index1 & 0x7F,
                (flags & PerNoteManagementDetach) != 0,
                (flags & PerNoteManagementReset) != 0);
            break;
        }

        case StatusPolyPressure:
        case StatusChannelPressure:
        default:
            m_stats.Ignored++;
            break;
        }
    }

    _Use_decl_annotations_
    void UmpDispatcher::HandleSystem(uint32_t word) noexcept
    {
        m_stats.SystemMessages++;

        const auto status = static_cast<uint8_t>((word >> 16) & 0xFF);

        if (status == SystemReset)
        {
            m_engine->SystemReset();
        }
        else if (status == ActiveSensing)
        {
            m_engine->ActiveSensing();
        }
        else
        {
            m_stats.Ignored++;
        }
    }

    _Use_decl_annotations_
    void UmpDispatcher::HandleSysEx7(uint32_t word0, uint32_t word1) noexcept
    {
        m_stats.SystemExclusive++;

        const uint8_t status = Status(word0);
        const uint8_t byteCount = static_cast<uint8_t>((word0 >> 16) & 0x0F);

        if (byteCount > 6)
        {
            m_stats.Malformed++;
            m_sysexLength = 0;
            return;
        }

        // Status 0 is a complete message and 1 is the start of one; both begin a new buffer.
        if (status == 0 || status == 1)
        {
            m_sysexLength = 0;
        }

        const uint8_t bytes[6] =
        {
            static_cast<uint8_t>((word0 >> 8) & 0x7F),
            static_cast<uint8_t>(word0 & 0x7F),
            static_cast<uint8_t>((word1 >> 24) & 0x7F),
            static_cast<uint8_t>((word1 >> 16) & 0x7F),
            static_cast<uint8_t>((word1 >> 8) & 0x7F),
            static_cast<uint8_t>(word1 & 0x7F),
        };

        for (uint8_t i = 0; i < byteCount; i++)
        {
            if (m_sysexLength >= MaxSysExBytes)
            {
                m_stats.Malformed++;
                m_sysexLength = 0;
                return;
            }

            m_sysex[m_sysexLength++] = bytes[i];
        }

        if (status == 0 || status == 3)
        {
            HandleCompletedSysEx();
            m_sysexLength = 0;
        }
    }

    void UmpDispatcher::HandleCompletedSysEx() noexcept
    {
        // Byte counts are checked before every access; the payload is untrusted.
        const size_t size = m_sysexLength;

        if (size < 3)
        {
            m_stats.Ignored++;
            return;
        }

        // Universal non real time.
        if (m_sysex[0] == 0x7E && size >= 4)
        {
            // General Information, Identity Request.
            if (m_sysex[2] == 0x06 && m_sysex[3] == 0x01)
            {
                SendIdentityReply(m_sysex[1]);
                return;
            }

            // GM System On resets everything. Sub-id 2 distinguishes GM1 from GM2, and which one
            // it is decides how a later bank select should be read.
            if (m_sysex[2] == 0x09)
            {
                m_engine->SystemReset();

                if (size >= 4 && m_sysex[3] == 0x03)
                {
                    m_engine->NotifyAddressingConvention(BankSelectMode::GeneralMidi2);
                }
                else
                {
                    m_engine->NotifyAddressingConvention(BankSelectMode::RolandGS);
                }

                return;
            }

            if (m_sysex[2] == 0x0D)
            {
                HandleMidiCi(m_sysex, size);
                return;
            }
        }

        // Universal real time, Device Control. These three are required by General MIDI 2.
        if (m_sysex[0] == 0x7F && size >= 6 && m_sysex[2] == 0x04)
        {
            const auto lsb = static_cast<uint16_t>(m_sysex[4] & 0x7F);
            const auto msb = static_cast<uint16_t>(m_sysex[5] & 0x7F);
            const auto combined = static_cast<uint16_t>((msb << 7) | lsb);

            switch (m_sysex[3])
            {
            case 0x01:
                m_engine->SetMasterVolume(combined);
                return;

            case 0x03:
                m_engine->SetMasterFineTuning(combined);
                return;

            case 0x04:
                // The least significant byte is defined as always zero here.
                m_engine->SetMasterCoarseTuning(static_cast<uint8_t>(msb));
                return;

            case 0x05:
                HandleGlobalParameterControl(m_sysex, size);
                return;

            default:
                break;
            }
        }

        // Roland GS reset, address 40 00 7F.
        if (m_sysex[0] == 0x41 && size >= 8 &&
            m_sysex[2] == 0x42 && m_sysex[3] == 0x12 &&
            m_sysex[4] == 0x40 && m_sysex[5] == 0x00 && m_sysex[6] == 0x7F)
        {
            m_engine->SystemReset();
            m_engine->NotifyAddressingConvention(BankSelectMode::RolandGS);
            return;
        }

        // Roland GS "Use For Rhythm Part", address 40 1p 15, where p is the part number and the
        // value is 0 for a melodic part or 1 or 2 to select a drum map. This is the only way a
        // file can put a kit on a channel other than 10, and without it eight of the nine kits in
        // this sound set can never be heard alongside the tenth.
        if (m_sysex[0] == 0x41 && size >= 8 &&
            m_sysex[2] == 0x42 && m_sysex[3] == 0x12 &&
            m_sysex[4] == 0x40 && (m_sysex[5] & 0xF0) == 0x10 && m_sysex[6] == 0x15)
        {
            // The GS part number in the address is not the MIDI channel: part 0 addresses channel
            // 10, and parts 1 to 9 address channels 1 to 9.
            const uint8_t part = m_sysex[5] & 0x0F;
            const uint8_t channel = (part == 0) ? 9u : ((part < 10) ? static_cast<uint8_t>(part - 1) : part);

            if (channel < MidiChannelCount)
            {
                m_engine->SetDrumChannel(channel, (m_sysex[7] & 0x7F) != 0);
                return;
            }
        }

        // Yamaha XG System On, address 00 00 7E 00. Seven bytes, one shorter than the GS messages
        // above, so it does not share their length guard.
        if (m_sysex[0] == 0x43 && size >= 7 &&
            m_sysex[2] == 0x4C &&
            m_sysex[3] == 0x00 && m_sysex[4] == 0x00 && m_sysex[5] == 0x7E)
        {
            m_engine->SystemReset();
            m_engine->NotifyAddressingConvention(BankSelectMode::YamahaXG);
            return;
        }

        m_stats.Ignored++;
    }
}
