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

        constexpr uint8_t SystemReset = 0xFF;

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
        m_muid = muid & 0x0FFFFFFF;
        m_sysex.clear();
        m_stats = {};
    }

    _Use_decl_annotations_
    void UmpDispatcher::SetOutput(IUmpOutput* output, const SynthIdentity& identity) noexcept
    {
        m_output = output;
        m_identity = identity;
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

        uint8_t payload[15]{};
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
    void UmpDispatcher::HandleMidiCi(const uint8_t* message, size_t size) noexcept
    {
        // 7E <device id> 0D <sub id 2> <version> <source muid x4> <destination muid x4> ...
        if (size < 13)
        {
            m_stats.Malformed++;
            return;
        }

        const uint8_t subId2 = message[3];

        const uint32_t initiatorMuid =
            static_cast<uint32_t>(message[5]) |
            (static_cast<uint32_t>(message[6]) << 7) |
            (static_cast<uint32_t>(message[7]) << 14) |
            (static_cast<uint32_t>(message[8]) << 21);

        // Replying to Discovery is mandatory even for a device that supports no MIDI-CI categories.
        if (subId2 == 0x70)
        {
            // The output path id arrived with message version 2, so older initiators omit it.
            const uint8_t outputPathId = (size >= 30) ? (message[29] & 0x7F) : 0;

            SendDiscoveryReply(initiatorMuid, outputPathId);
            return;
        }

        m_stats.Ignored++;
    }

    _Use_decl_annotations_
    void UmpDispatcher::SendDiscoveryReply(uint32_t initiatorMuid, uint8_t outputPathId) noexcept
    {
        if (m_output == nullptr || !m_identity.IsConfigured() || m_muid == 0)
        {
            m_stats.Ignored++;
            return;
        }

        auto appendMuid = [](uint8_t* buffer, size_t& offset, uint32_t muid) noexcept
        {
            buffer[offset++] = static_cast<uint8_t>(muid & 0x7F);
            buffer[offset++] = static_cast<uint8_t>((muid >> 7) & 0x7F);
            buffer[offset++] = static_cast<uint8_t>((muid >> 14) & 0x7F);
            buffer[offset++] = static_cast<uint8_t>((muid >> 21) & 0x7F);
        };

        uint8_t payload[31]{};
        size_t total = 0;

        payload[total++] = 0x7E;
        payload[total++] = 0x7F;        // from the function block
        payload[total++] = 0x0D;
        payload[total++] = 0x71;        // reply to discovery
        payload[total++] = 0x02;        // message version

        appendMuid(payload, total, m_muid);
        appendMuid(payload, total, initiatorMuid);

        // MIDI-CI always carries three manufacturer bytes, unlike an Identity Reply.
        payload[total++] = m_identity.ManufacturerSysExId[0];
        payload[total++] = m_identity.ManufacturerSysExId[1];
        payload[total++] = m_identity.ManufacturerSysExId[2];

        payload[total++] = static_cast<uint8_t>(m_identity.FamilyCode & 0x7F);
        payload[total++] = static_cast<uint8_t>((m_identity.FamilyCode >> 7) & 0x7F);
        payload[total++] = static_cast<uint8_t>(m_identity.FamilyMemberCode & 0x7F);
        payload[total++] = static_cast<uint8_t>((m_identity.FamilyMemberCode >> 7) & 0x7F);

        for (const auto revision : m_identity.SoftwareRevision)
        {
            payload[total++] = revision & 0x7F;
        }

        // No Profile Configuration, Property Exchange or Process Inquiry yet.
        payload[total++] = 0x00;

        // Receivable maximum SysEx size, seven bits per byte, LSB first.
        payload[total++] = static_cast<uint8_t>(MaxSysExBytes & 0x7F);
        payload[total++] = static_cast<uint8_t>((MaxSysExBytes >> 7) & 0x7F);
        payload[total++] = static_cast<uint8_t>((MaxSysExBytes >> 14) & 0x7F);
        payload[total++] = static_cast<uint8_t>((MaxSysExBytes >> 21) & 0x7F);

        payload[total++] = outputPathId;
        payload[total++] = SynthEndpoint::FunctionBlockNumber;

        SendSysEx7(payload, total);

        m_stats.DiscoveryRepliesSent++;
    }

    _Use_decl_annotations_
    void UmpDispatcher::SendSysEx7(const uint8_t* payload, size_t total) noexcept
    {
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
                (static_cast<uint32_t>(m_group) << 24) |
                (static_cast<uint32_t>(status) << 20) |
                (static_cast<uint32_t>(count) << 16) |
                (static_cast<uint32_t>(bytes[0]) << 8) |
                bytes[1],

                (static_cast<uint32_t>(bytes[2]) << 24) |
                (static_cast<uint32_t>(bytes[3]) << 16) |
                (static_cast<uint32_t>(bytes[4]) << 8) |
                bytes[5],
            };

            m_output->SendUmp(words, 2);
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
            m_engine->NoteOn(channel, index1 & 0x7F, velocity);
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
            m_sysex.clear();
            return;
        }

        // Status 0 is a complete message and 1 is the start of one; both begin a new buffer.
        if (status == 0 || status == 1)
        {
            m_sysex.clear();
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
            if (m_sysex.size() >= MaxSysExBytes)
            {
                m_stats.Malformed++;
                m_sysex.clear();
                return;
            }

            m_sysex.push_back(bytes[i]);
        }

        if (status == 0 || status == 3)
        {
            HandleCompletedSysEx();
            m_sysex.clear();
        }
    }

    void UmpDispatcher::HandleCompletedSysEx() noexcept
    {
        // Byte counts are checked before every access; the payload is untrusted.
        const size_t size = m_sysex.size();

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

            // GM System On resets everything.
            if (m_sysex[2] == 0x09)
            {
                m_engine->SystemReset();
                return;
            }

            if (m_sysex[2] == 0x0D)
            {
                HandleMidiCi(m_sysex.data(), size);
                return;
            }
        }

        // Roland GS reset, address 40 00 7F.
        if (m_sysex[0] == 0x41 && size >= 8 &&
            m_sysex[2] == 0x42 && m_sysex[3] == 0x12 &&
            m_sysex[4] == 0x40 && m_sysex[5] == 0x00 && m_sysex[6] == 0x7F)
        {
            m_engine->SystemReset();
            return;
        }

        m_stats.Ignored++;
    }
}
