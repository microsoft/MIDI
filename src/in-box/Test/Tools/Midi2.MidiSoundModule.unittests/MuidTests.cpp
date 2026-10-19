// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The identifier a synth answers MIDI-CI with, and replacing it after another device withdraws it
// or claims it too (M2-101-UM, Discovery and Invalidate MUID). The responder belongs to the thread
// that dispatches messages, so the replacement the worker makes is handed over and taken up there.

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;

namespace ci = WindowsMidiServicesCapabilityInquiry;

namespace
{
    constexpr uint32_t FirstMuid = 0x0123456;
    constexpr uint32_t SecondMuid = 0x0654321;
    constexpr uint32_t OtherDevice = 0x4A75301;

    struct CaptureSysEx final : ISysExSink
    {
        std::vector<std::vector<uint8_t>> Messages;

        void SendSysEx(_In_reads_(count) uint8_t const* payload, _In_ size_t count) noexcept override
        {
            try
            {
                Messages.emplace_back(payload, payload + count);
            }
            catch (...)
            {
            }
        }

        // Who the first message of a type came from, or zero when none of that type did.
        uint32_t SentBy(_In_ ci::MessageType type) const
        {
            for (auto const& message : Messages)
            {
                ci::ParsedMessage parsed{};

                if (ci::Parse(message.data(), message.size(), parsed) == ci::ParseStatus::Ok && parsed.Type == type)
                {
                    return parsed.SourceMuid;
                }
            }

            return 0;
        }
    };

    void Receive(_Inout_ SynthDispatcher& dispatcher, _In_reads_(count) uint8_t const* payload, _In_ size_t count)
    {
        std::vector<uint32_t> words(SysEx7WordCount(count));

        auto const written = BuildSysEx7Packets(0, payload, count, words.data(), words.size());

        VERIFY_ARE_EQUAL(words.size(), written);

        (void)dispatcher.ProcessWords(words.data(), static_cast<uint32_t>(written));
    }

    void Discover(_Inout_ SynthDispatcher& dispatcher, _In_ uint32_t sourceMuid)
    {
        ci::DiscoveryReplyFields fields{};
        fields.SourceMuid = sourceMuid;
        fields.ManufacturerSysExId[0] = 0x7D;
        fields.CapabilityCategories = ci::CategoryPropertyExchange;
        fields.ReceivableMaximumSysExSize = 4096;
        fields.MessageVersion = 0x02;

        uint8_t buffer[64]{};
        auto const size = ci::BuildDiscovery(fields, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        Receive(dispatcher, buffer, size);
    }

    void Invalidate(_Inout_ SynthDispatcher& dispatcher, _In_ uint32_t targetMuid)
    {
        uint8_t buffer[ci::InvalidateMuidByteCount]{};
        auto const size = ci::BuildInvalidateMuid(OtherDevice, targetMuid, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        Receive(dispatcher, buffer, size);
    }
}

class MuidTests
{
public:
    BEGIN_TEST_CLASS(MuidTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // Withdrawn by another device, the identifier goes quiet until the worker hands over a new one.
    // From then on the worker must not make another, its replies must carry the new one, and the
    // next MIDI-CI message the dispatcher handles must be answered with it.
    TEST_METHOD(WithdrawnMuidIsReplacedOnTheDispatchingThread)
    {
        // Not initialized: MIDI-CI is answered without a sound bank.
        Synthesizer synthesizer;
        SynthDispatcher dispatcher;
        dispatcher.Initialize(&synthesizer, 0, FirstMuid);

        CaptureSysEx output;
        dispatcher.SetOutput(&output, SynthIdentity{});

        VERIFY_ARE_EQUAL(FirstMuid, dispatcher.Muid());
        VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());

        Invalidate(dispatcher, FirstMuid);

        VERIFY_IS_TRUE(dispatcher.MuidNeedsReplacement(), L"a withdrawn identifier needs replacing");
        VERIFY_ARE_EQUAL(0u, dispatcher.Muid(), L"and is no longer reported");

        output.Messages.clear();
        Discover(dispatcher, OtherDevice);

        VERIFY_IS_TRUE(output.Messages.empty(), L"nothing answers until there is a replacement");

        dispatcher.SetMuid(SecondMuid);

        VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement(), L"a replacement handed over is not asked for again");
        VERIFY_ARE_EQUAL(SecondMuid, dispatcher.Muid(), L"replies sent meanwhile carry the replacement");

        output.Messages.clear();
        Discover(dispatcher, OtherDevice);

        VERIFY_ARE_EQUAL(SecondMuid, output.SentBy(ci::MessageType::DiscoveryReply), L"the next message is answered with it");
        VERIFY_ARE_EQUAL(SecondMuid, dispatcher.Muid());
        VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());

        Invalidate(dispatcher, SecondMuid);

        VERIFY_IS_TRUE(dispatcher.MuidNeedsReplacement(), L"and it can be withdrawn in turn");
    }

    // A device announcing itself with our identifier gets told it is withdrawn, and the synth stops
    // using it. One replacement is asked for, and answered with.
    TEST_METHOD(CollisionAsksForOneReplacement)
    {
        Synthesizer synthesizer;
        SynthDispatcher dispatcher;
        dispatcher.Initialize(&synthesizer, 0, FirstMuid);

        CaptureSysEx output;
        dispatcher.SetOutput(&output, SynthIdentity{});

        Discover(dispatcher, FirstMuid);

        VERIFY_ARE_EQUAL(FirstMuid, output.SentBy(ci::MessageType::InvalidateMuid), L"the clash is announced");
        VERIFY_IS_TRUE(dispatcher.MuidNeedsReplacement());
        VERIFY_ARE_EQUAL(0u, dispatcher.Muid());

        dispatcher.SetMuid(SecondMuid);

        VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());

        output.Messages.clear();
        Discover(dispatcher, OtherDevice);

        VERIFY_ARE_EQUAL(SecondMuid, output.SentBy(ci::MessageType::DiscoveryReply));
        VERIFY_IS_FALSE(dispatcher.MuidNeedsReplacement());
    }
};
