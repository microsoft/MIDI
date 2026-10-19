// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthMuidTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;

namespace ci = WindowsMidiServicesCapabilityInquiry;

namespace
{
    constexpr uint32_t FirstMuid = 0x0123456;
    constexpr uint32_t SecondMuid = 0x0654321;
    constexpr uint32_t OtherDevice = 0x4A75301;

    struct CaptureOutput final : IUmpOutput
    {
        std::vector<uint32_t> Words;

        void SendUmp(_In_reads_(wordCount) const uint32_t* words, _In_ uint32_t wordCount) noexcept override
        {
            for (uint32_t i = 0; i < wordCount; i++)
            {
                Words.push_back(words[i]);
            }
        }
    };

    // A whole system exclusive message as the dispatcher receives one: seven bit packets, group 1.
    void Receive(_Inout_ UmpDispatcher& dispatcher, _In_reads_(count) const uint8_t* payload, _In_ size_t count)
    {
        CaptureOutput packets;
        UmpDispatcher::PacketizeSysEx7(packets, 0, payload, count);

        dispatcher.ProcessWords(packets.Words.data(), static_cast<uint32_t>(packets.Words.size()));
    }

    void Discover(_Inout_ UmpDispatcher& dispatcher, _In_ uint32_t sourceMuid)
    {
        ci::DiscoveryReplyFields fields{};
        fields.SourceMuid = sourceMuid;
        fields.ManufacturerSysExId[0] = 0x7D;
        fields.CapabilityCategories = ci::CategoryPropertyExchange;
        fields.ReceivableMaximumSysExSize = 4096;
        fields.MessageVersion = 0x02;

        uint8_t buffer[64]{};
        const auto size = ci::BuildDiscovery(fields, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        Receive(dispatcher, buffer, size);
    }

    void Invalidate(_Inout_ UmpDispatcher& dispatcher, _In_ uint32_t targetMuid)
    {
        uint8_t buffer[ci::InvalidateMuidByteCount]{};
        const auto size = ci::BuildInvalidateMuid(OtherDevice, targetMuid, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        Receive(dispatcher, buffer, size);
    }

    // Who the first message of a type that went out came from, or zero when none of that type did.
    uint32_t SentBy(_In_ const CaptureOutput& output, _In_ ci::MessageType type)
    {
        std::vector<uint8_t> message;

        for (size_t i = 0; i + 1 < output.Words.size(); i += 2)
        {
            const uint32_t w0 = output.Words[i];
            const uint32_t w1 = output.Words[i + 1];
            const auto status = (w0 >> 20) & 0xF;
            const auto count = (std::min)((w0 >> 16) & 0xFu, 6u);

            const uint8_t bytes[6]
            {
                static_cast<uint8_t>((w0 >> 8) & 0x7F),
                static_cast<uint8_t>(w0 & 0x7F),
                static_cast<uint8_t>((w1 >> 24) & 0x7F),
                static_cast<uint8_t>((w1 >> 16) & 0x7F),
                static_cast<uint8_t>((w1 >> 8) & 0x7F),
                static_cast<uint8_t>(w1 & 0x7F),
            };

            if (status == 0 || status == 1)
            {
                message.clear();
            }

            message.insert(message.end(), bytes, bytes + count);

            ci::ParsedMessage parsed{};

            if ((status == 0 || status == 3) &&
                ci::Parse(message.data(), message.size(), parsed) == ci::ParseStatus::Ok &&
                parsed.Type == type)
            {
                return parsed.SourceMuid;
            }
        }

        return 0;
    }
}

// Withdrawn by another device, the identifier goes quiet until the host hands over a new one. From
// then on the host must not make another, a worker's replies must carry the new one, and the next
// MIDI-CI message the dispatcher handles must be answered with it.
void MidiSynthMuidTests::TestWithdrawnMuidIsReplacedOnTheDispatchingThread()
{
    // Never initialized: MIDI-CI is answered before the engine has a sound set.
    SynthEngine engine;
    UmpDispatcher dispatcher;
    dispatcher.Initialize(&engine, 0, FirstMuid);

    CaptureOutput output;
    dispatcher.SetOutput(&output, SynthIdentity{});

    VERIFY_ARE_EQUAL(FirstMuid, dispatcher.Muid());
    VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());

    Invalidate(dispatcher, FirstMuid);

    VERIFY_IS_TRUE(dispatcher.MuidNeedsReplacement(), L"a withdrawn identifier needs replacing");
    VERIFY_ARE_EQUAL(0u, dispatcher.Muid(), L"and is no longer reported");

    output.Words.clear();
    Discover(dispatcher, OtherDevice);

    VERIFY_IS_TRUE(output.Words.empty(), L"nothing answers until there is a replacement");

    dispatcher.SetMuid(SecondMuid);

    VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement(), L"a replacement handed over is not asked for again");
    VERIFY_ARE_EQUAL(SecondMuid, dispatcher.Muid(), L"replies sent meanwhile carry the replacement");

    output.Words.clear();
    Discover(dispatcher, OtherDevice);

    VERIFY_ARE_EQUAL(SecondMuid, SentBy(output, ci::MessageType::DiscoveryReply), L"the next message is answered with it");
    VERIFY_ARE_EQUAL(SecondMuid, dispatcher.Muid());
    VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());

    Invalidate(dispatcher, SecondMuid);

    VERIFY_IS_TRUE(dispatcher.MuidNeedsReplacement(), L"and it can be withdrawn in turn");
}

// A device announcing itself with our identifier gets told it is withdrawn, and the synthesizer
// stops using it. One replacement is asked for, and answered with.
void MidiSynthMuidTests::TestCollisionAsksForOneReplacement()
{
    SynthEngine engine;
    UmpDispatcher dispatcher;
    dispatcher.Initialize(&engine, 0, FirstMuid);

    CaptureOutput output;
    dispatcher.SetOutput(&output, SynthIdentity{});

    Discover(dispatcher, FirstMuid);

    VERIFY_ARE_EQUAL(FirstMuid, SentBy(output, ci::MessageType::InvalidateMuid), L"the clash is announced");
    VERIFY_IS_TRUE(dispatcher.MuidNeedsReplacement());
    VERIFY_ARE_EQUAL(0u, dispatcher.Muid());

    dispatcher.SetMuid(SecondMuid);

    VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());

    output.Words.clear();
    Discover(dispatcher, OtherDevice);

    VERIFY_ARE_EQUAL(SecondMuid, SentBy(output, ci::MessageType::DiscoveryReply));
    VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());
}
