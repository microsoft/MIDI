// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "SynthDispatcher.h"

namespace SoundFontSynth
{
    namespace
    {
        constexpr uint32_t MessageTypeUtility = 0x0;
        constexpr uint32_t MessageTypeSystem = 0x1;
        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeSysEx7 = 0x3;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;
        constexpr uint32_t MessageTypeStream = 0xF;

        constexpr uint8_t StatusRegisteredPerNoteController = 0x0;
        constexpr uint8_t StatusRegisteredController = 0x2;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;
        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;
        constexpr uint8_t StatusPerNoteManagement = 0xF;

        constexpr uint8_t NoteAttributePitch79 = 0x03;

        constexpr uint8_t PerNoteManagementReset = 0x01;
        constexpr uint8_t PerNoteManagementDetach = 0x02;

        constexpr uint8_t SystemReset = 0xFF;
        constexpr uint8_t ActiveSensing = 0xFE;

        constexpr uint32_t MessageType(_In_ uint32_t word) noexcept { return (word >> 28) & 0xF; }
        constexpr uint8_t Group(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 24) & 0xF); }
        constexpr uint8_t Status(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 20) & 0xF); }
        constexpr uint8_t Channel(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 16) & 0xF); }
        constexpr uint8_t Data1(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 8) & 0x7F); }
        constexpr uint8_t Data2(_In_ uint32_t word) noexcept { return static_cast<uint8_t>(word & 0x7F); }
    }

    _Use_decl_annotations_
    uint32_t SynthDispatcher::ScaleUp(uint32_t value, uint8_t sourceBits, uint8_t destinationBits) noexcept
    {
        if (sourceBits == 0 || destinationBits <= sourceBits || destinationBits > 32)
        {
            return value;
        }

        auto const scaleBits = static_cast<uint8_t>(destinationBits - sourceBits);
        auto shifted = value << scaleBits;
        auto const center = 1u << (sourceBits - 1);

        if (value <= center)
        {
            return shifted;
        }

        // Above the center, the bits below the top one are repeated into the new low bits so that
        // the maximum maps to the maximum.
        auto const repeatBits = static_cast<uint8_t>(sourceBits - 1);
        auto const repeatMask = (1u << repeatBits) - 1;
        auto repeat = value & repeatMask;

        if (scaleBits > repeatBits)
        {
            repeat <<= (scaleBits - repeatBits);
        }
        else
        {
            repeat >>= (repeatBits - scaleBits);
        }

        while (repeat != 0)
        {
            shifted |= repeat;
            repeat >>= repeatBits;
        }

        return shifted;
    }

    _Use_decl_annotations_
    void SynthDispatcher::Initialize(Synthesizer* synthesizer, uint8_t group, uint32_t muid) noexcept
    {
        m_synthesizer = synthesizer;
        m_group = group & 0xF;
        m_sysexLength = 0;
        m_statistics = {};

        ConfigureResponder(muid);
    }

    _Use_decl_annotations_
    void SynthDispatcher::SetOutput(ISysExSink* output, SynthIdentity const& identity) noexcept
    {
        m_output = output;
        m_identity = identity;

        ConfigureResponder(m_responder.Muid());
    }

    _Use_decl_annotations_
    void SynthDispatcher::SetProductInstanceId(char const* productInstanceId) noexcept
    {
        size_t length = 0;

        if (productInstanceId != nullptr)
        {
            while (length < SynthEndpointShape::MaximumProductInstanceIdBytes && productInstanceId[length] != '\0')
            {
                m_productInstanceId[length] = productInstanceId[length];
                length++;
            }
        }

        m_productInstanceId[length] = '\0';

        ConfigureResponder(m_responder.Muid());
    }

    _Use_decl_annotations_
    void SynthDispatcher::SetMuid(uint32_t muid) noexcept
    {
        m_responder.SetMuid(muid & 0x0FFFFFFF);
    }

    _Use_decl_annotations_
    void SynthDispatcher::SendSysEx(uint8_t const* payload, size_t count) noexcept
    {
        if (m_output != nullptr && count > 0)
        {
            m_output->SendSysEx(payload, count);
        }
    }

    _Use_decl_annotations_
    void SynthDispatcher::SendIdentityReply(uint8_t requestedDeviceId) noexcept
    {
        if (m_output == nullptr || !m_identity.IsConfigured())
        {
            m_statistics.Ignored++;
            return;
        }

        auto const deviceId = (requestedDeviceId == 0x7F) ? m_identity.DeviceId : requestedDeviceId;

        // A manufacturer identifier is one byte, or three when the first is zero.
        auto const extendedId = (m_identity.ManufacturerSysExId[0] == 0);

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

        for (auto const revision : m_identity.SoftwareRevision)
        {
            payload[total++] = revision & 0x7F;
        }

        SendSysEx(payload, total);

        m_statistics.IdentityRepliesSent++;
    }

    _Use_decl_annotations_
    void SynthDispatcher::HandleGlobalParameterControl(uint8_t const* message, size_t size) noexcept
    {
        // 7F <device> 04 05 <slot path length> <parameter id width> <value width> <slot path...>
        // then parameter and value pairs. Only the widths GM2 defines for reverb and chorus.
        constexpr size_t HeaderSize = 7;

        if (size < HeaderSize + 2 || message[4] != 1 || message[5] != 1 || message[6] != 1)
        {
            m_statistics.Ignored++;
            return;
        }

        auto const slot = message[HeaderSize];

        MidiSynth::IAudioEffect* effect = nullptr;

        if (slot == 0x01)
        {
            effect = m_synthesizer->Reverb();
        }
        else if (slot == 0x02)
        {
            effect = m_synthesizer->Chorus();
        }

        if (effect == nullptr)
        {
            m_statistics.Ignored++;
            return;
        }

        for (size_t offset = HeaderSize + 1; offset + 1 < size; offset += 2)
        {
            auto const parameter = static_cast<uint8_t>(message[offset] & 0x7F);
            auto const normalized = static_cast<double>(message[offset + 1] & 0x7F) / 127.0;

            switch (parameter)
            {
            case 0x00:
                effect->SetParameter(MidiSynth::AudioEffectParameter::Damping, normalized);
                break;

            case 0x01:
                effect->SetParameter(MidiSynth::AudioEffectParameter::Time, 0.2 + normalized * 4.0);
                break;

            default:
                break;
            }
        }
    }

    _Use_decl_annotations_
    void SynthDispatcher::HandleMidiCi(uint8_t const* message, size_t size) noexcept
    {
        namespace ci = WindowsMidiServicesCapabilityInquiry;

        ci::ParsedMessage parsed{};

        if (ci::Parse(message, size, parsed) != ci::ParseStatus::Ok)
        {
            m_statistics.Malformed++;
            return;
        }

        uint8_t reply[(std::max)({
            ci::DiscoveryReplyByteCount,
            ci::InvalidateMuidByteCount,
            ci::AcknowledgmentFixedByteCount,
            ci::PropertyExchangeCapabilitiesByteCount,
            ci::EndpointReplyFixedByteCount + ci::ProductInstanceIdMaximumByteCount })]{};

        size_t replyBytes{ 0 };

        auto const action = m_responder.ProcessMessage(parsed, reply, sizeof(reply), &replyBytes);

        switch (action)
        {
        case ci::ResponderAction::Replied:
            if (!m_identity.IsConfigured() || m_output == nullptr)
            {
                m_statistics.Ignored++;
                return;
            }

            SendSysEx(reply, replyBytes);
            m_statistics.DiscoveryRepliesSent++;
            return;

        case ci::ResponderAction::MuidInvalidated:
            m_statistics.MuidInvalidations++;
            return;

        case ci::ResponderAction::MuidCollision:
            // The other device holding our identifier has to hear about it too.
            m_statistics.MuidInvalidations++;
            SendSysEx(reply, replyBytes);
            return;

        case ci::ResponderAction::InitiatorMuidInvalidated:
            if (parsed.TargetMuid != 0)
            {
                m_invalidatedInitiatorMuid.store(parsed.TargetMuid, std::memory_order_release);
            }
            return;

        case ci::ResponderAction::PropertyDataRequested:
            ParkPropertyRequest(parsed, message, false);
            return;

        case ci::ResponderAction::PropertySubscriptionRequested:
            ParkPropertyRequest(parsed, message, true);
            return;

        default:
            m_statistics.Ignored++;
            return;
        }
    }

    _Use_decl_annotations_
    void SynthDispatcher::ParkPropertyRequest(
        WindowsMidiServicesCapabilityInquiry::ParsedMessage const& parsed,
        uint8_t const* message,
        bool isSubscription) noexcept
    {
        auto const headerBytes = parsed.PropertyExchange.HeaderByteCount;

        // Still holding the previous one. Dropping is correct: an initiator retries, and
        // overwriting would corrupt what the worker is reading.
        if (headerBytes > MaximumPropertyHeaderBytes || m_propertyRequestPending.load(std::memory_order_acquire))
        {
            m_statistics.Ignored++;
            return;
        }

        m_propertyRequest.InitiatorMuid = parsed.SourceMuid;
        m_propertyRequest.RequestId = parsed.PropertyExchange.RequestId;
        m_propertyRequest.HeaderByteCount = headerBytes;
        m_propertyRequest.IsSubscription = isSubscription;

        for (uint16_t i = 0; i < headerBytes; i++)
        {
            m_propertyRequest.Header[i] = message[parsed.PropertyExchange.HeaderOffset + i];
        }

        m_propertyRequestPending.store(true, std::memory_order_release);

        m_statistics.PropertyRequests++;
    }

    _Use_decl_annotations_
    bool SynthDispatcher::TakePendingPropertyRequest(PendingPropertyRequest& request) noexcept
    {
        request = {};

        if (!m_propertyRequestPending.load(std::memory_order_acquire))
        {
            return false;
        }

        request = m_propertyRequest;

        m_propertyRequestPending.store(false, std::memory_order_release);

        return true;
    }

    _Use_decl_annotations_
    bool SynthDispatcher::TakeInvalidatedInitiatorMuid(uint32_t& muid) noexcept
    {
        muid = m_invalidatedInitiatorMuid.exchange(0, std::memory_order_acq_rel);

        return muid != 0;
    }

    _Use_decl_annotations_
    void SynthDispatcher::ConfigureResponder(uint32_t muid) noexcept
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

        config.ReceivableMaximumSysExSize = MaximumSysExBytes;
        config.FunctionBlockNumber = SynthEndpointShape::FunctionBlockNumber;
        config.SupportsPropertyExchange = true;

        auto const productInstanceIdBytes = strnlen(m_productInstanceId, ci::ProductInstanceIdMaximumByteCount);

        for (size_t i = 0; i < productInstanceIdBytes; i++)
        {
            config.ProductInstanceId[i] = static_cast<uint8_t>(m_productInstanceId[i]);
        }

        config.ProductInstanceIdByteCount = static_cast<uint8_t>(productInstanceIdBytes);

        m_responder.Initialize(config);
    }

    _Use_decl_annotations_
    uint32_t SynthDispatcher::PacketWordCount(uint32_t firstWord) noexcept
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
    uint32_t SynthDispatcher::ProcessWords(uint32_t const* words, uint32_t wordCount) noexcept
    {
        if (m_synthesizer == nullptr || words == nullptr)
        {
            return 0;
        }

        uint32_t consumed = 0;

        while (consumed < wordCount)
        {
            auto const word0 = words[consumed];
            auto const packetWords = PacketWordCount(word0);

            if (packetWords == 0)
            {
                m_statistics.Malformed++;
                consumed++;
                continue;
            }

            if (consumed + packetWords > wordCount)
            {
                break;
            }

            // Other groups belong to other function blocks. Stream messages carry no group at all.
            if (Group(word0) != m_group &&
                MessageType(word0) != MessageTypeUtility &&
                MessageType(word0) != MessageTypeStream)
            {
                m_statistics.Ignored++;
                consumed += packetWords;
                continue;
            }

            switch (MessageType(word0))
            {
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
                // Utility messages, and stream messages, which the virtual device answers itself.
                m_statistics.Ignored++;
                break;
            }

            consumed += packetWords;
        }

        return consumed;
    }

    _Use_decl_annotations_
    void SynthDispatcher::HandleMidi1ChannelVoice(uint32_t word) noexcept
    {
        m_statistics.Midi1ChannelVoice++;

        auto const channel = Channel(word);
        auto const data1 = Data1(word);
        auto const data2 = Data2(word);

        switch (Status(word))
        {
        case StatusNoteOff:
            m_synthesizer->NoteOff(channel, data1);
            break;

        case StatusNoteOn:
            if (data2 == 0)
            {
                // In MIDI 1.0 a note on with zero velocity is a note off.
                m_synthesizer->NoteOff(channel, data1);
            }
            else
            {
                m_synthesizer->NoteOn(channel, data1, static_cast<uint16_t>(ScaleUp(data2, 7, 16)));
            }
            break;

        case StatusPolyPressure:
            m_synthesizer->PolyPressure(channel, data1, data2);
            break;

        case StatusControlChange:
            m_synthesizer->ControlChange(channel, data1, data2);
            break;

        case StatusProgramChange:
            m_synthesizer->ProgramChange(channel, data1);
            break;

        case StatusChannelPressure:
            m_synthesizer->ChannelPressure(channel, data1);
            break;

        case StatusPitchBend:
            m_synthesizer->PitchBend(channel, static_cast<uint16_t>((data2 << 7) | data1));
            break;

        default:
            m_statistics.Ignored++;
            break;
        }
    }

    _Use_decl_annotations_
    void SynthDispatcher::HandleMidi2ChannelVoice(uint32_t word0, uint32_t word1) noexcept
    {
        m_statistics.Midi2ChannelVoice++;

        auto const channel = Channel(word0);
        auto const index1 = static_cast<uint8_t>((word0 >> 8) & 0xFF);
        auto const index2 = static_cast<uint8_t>(word0 & 0xFF);

        switch (Status(word0))
        {
        case StatusNoteOff:
            m_synthesizer->NoteOff(channel, index1 & 0x7F);
            break;

        case StatusNoteOn:
        {
            // A MIDI 2.0 note on with zero velocity is still a note on.
            auto const velocity = static_cast<uint16_t>((word1 >> 16) & 0xFFFF);
            auto const note = static_cast<uint8_t>(index1 & 0x7F);
            auto const attributeData = static_cast<uint16_t>(word1 & 0xFFFF);

            if (index2 == NoteAttributePitch79)
            {
                auto const pitch =
                    static_cast<double>((attributeData >> 9) & 0x7F) +
                    static_cast<double>(attributeData & 0x01FF) / 512.0;

                m_synthesizer->NoteOnWithPitch(channel, note, velocity, pitch);
            }
            else
            {
                m_synthesizer->NoteOn(channel, note, velocity);
            }
            break;
        }

        case StatusPolyPressure:
            m_synthesizer->PolyPressure32(channel, index1 & 0x7F, word1);
            break;

        case StatusControlChange:
            m_synthesizer->ControlChange32(channel, index1 & 0x7F, word1);
            break;

        case StatusProgramChange:
        {
            auto const program = static_cast<uint8_t>((word1 >> 24) & 0x7F);

            if ((word0 & 0x1) != 0)
            {
                m_synthesizer->ProgramChangeWithBank(channel,
                    static_cast<uint8_t>((word1 >> 8) & 0x7F),
                    static_cast<uint8_t>(word1 & 0x7F),
                    program);
            }
            else
            {
                m_synthesizer->ProgramChange(channel, program);
            }
            break;
        }

        case StatusChannelPressure:
            m_synthesizer->ChannelPressure32(channel, word1);
            break;

        case StatusPitchBend:
            m_synthesizer->PitchBend32(channel, word1);
            break;

        case StatusRegisteredController:
            m_synthesizer->RegisteredController(channel, index1 & 0x7F, index2 & 0x7F, word1);
            break;

        case StatusRegisteredPerNoteController:
            m_synthesizer->PerNoteController(channel, index1 & 0x7F, index2, word1);
            break;

        case StatusPerNotePitchBend:
        {
            constexpr double center = 2147483648.0;

            m_synthesizer->PerNotePitchBend(channel, index1 & 0x7F, (static_cast<double>(word1) - center) / center);
            break;
        }

        case StatusPerNoteManagement:
            m_synthesizer->PerNoteManagement(
                channel,
                index1 & 0x7F,
                (index2 & PerNoteManagementDetach) != 0,
                (index2 & PerNoteManagementReset) != 0);
            break;

        default:
            m_statistics.Ignored++;
            break;
        }
    }

    _Use_decl_annotations_
    void SynthDispatcher::HandleSystem(uint32_t word) noexcept
    {
        m_statistics.SystemMessages++;

        auto const status = static_cast<uint8_t>((word >> 16) & 0xFF);

        if (status == SystemReset)
        {
            m_synthesizer->SystemReset();
        }
        else if (status == ActiveSensing)
        {
            m_synthesizer->ActiveSensing();
        }
        else
        {
            m_statistics.Ignored++;
        }
    }

    _Use_decl_annotations_
    void SynthDispatcher::HandleSysEx7(uint32_t word0, uint32_t word1) noexcept
    {
        m_statistics.SystemExclusive++;

        auto const status = Status(word0);
        auto const byteCount = static_cast<uint8_t>((word0 >> 16) & 0x0F);

        if (byteCount > 6)
        {
            m_statistics.Malformed++;
            m_sysexLength = 0;
            return;
        }

        if (status == 0 || status == 1)
        {
            m_sysexLength = 0;
        }

        uint8_t const bytes[6]
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
            if (m_sysexLength >= MaximumSysExBytes)
            {
                m_statistics.Malformed++;
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

    void SynthDispatcher::HandleCompletedSysEx() noexcept
    {
        auto const size = m_sysexLength;

        if (size < 3)
        {
            m_statistics.Ignored++;
            return;
        }

        // Universal non real time.
        if (m_sysex[0] == 0x7E && size >= 4)
        {
            if (m_sysex[2] == 0x06 && m_sysex[3] == 0x01)
            {
                SendIdentityReply(m_sysex[1]);
                return;
            }

            // GM System On. Sub-id 3 is GM2, which decides how later bank selects are read.
            if (m_sysex[2] == 0x09)
            {
                m_synthesizer->SystemReset();
                m_synthesizer->NotifyAddressingConvention(
                    (m_sysex[3] == 0x03) ? BankSelectMode::GeneralMidi2 : BankSelectMode::RolandGS);
                return;
            }

            if (m_sysex[2] == 0x0D)
            {
                HandleMidiCi(m_sysex, size);
                return;
            }
        }

        // Universal real time, Device Control, as General MIDI 2 requires.
        if (m_sysex[0] == 0x7F && size >= 6 && m_sysex[2] == 0x04)
        {
            auto const lsb = static_cast<uint16_t>(m_sysex[4] & 0x7F);
            auto const msb = static_cast<uint16_t>(m_sysex[5] & 0x7F);
            auto const combined = static_cast<uint16_t>((msb << 7) | lsb);

            switch (m_sysex[3])
            {
            case 0x01:
                m_synthesizer->SetMasterVolume(combined);
                return;

            case 0x03:
                m_synthesizer->SetMasterFineTuning(combined);
                return;

            case 0x04:
                m_synthesizer->SetMasterCoarseTuning(static_cast<uint8_t>(msb));
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
            m_synthesizer->SystemReset();
            m_synthesizer->NotifyAddressingConvention(BankSelectMode::RolandGS);
            return;
        }

        // Roland GS Use For Rhythm Part, address 40 1p 15. The part number is not the channel:
        // part 0 is channel 10, parts 1 to 9 are channels 1 to 9, and parts A to F are 11 to 16.
        if (m_sysex[0] == 0x41 && size >= 8 &&
            m_sysex[2] == 0x42 && m_sysex[3] == 0x12 &&
            m_sysex[4] == 0x40 && (m_sysex[5] & 0xF0) == 0x10 && m_sysex[6] == 0x15)
        {
            auto const part = static_cast<uint8_t>(m_sysex[5] & 0x0F);
            auto const channel = (part == 0) ? static_cast<uint8_t>(9) : ((part < 10) ? static_cast<uint8_t>(part - 1) : part);

            if (channel < MidiChannelCount)
            {
                m_synthesizer->SetDrumChannel(channel, (m_sysex[7] & 0x7F) != 0);
                return;
            }
        }

        // Yamaha XG System On, address 00 00 7E 00. One byte shorter than the GS messages above.
        if (m_sysex[0] == 0x43 && size >= 7 &&
            m_sysex[2] == 0x4C &&
            m_sysex[3] == 0x00 && m_sysex[4] == 0x00 && m_sysex[5] == 0x7E)
        {
            m_synthesizer->SystemReset();
            m_synthesizer->NotifyAddressingConvention(BankSelectMode::YamahaXG);
            return;
        }

        m_statistics.Ignored++;
    }
}
