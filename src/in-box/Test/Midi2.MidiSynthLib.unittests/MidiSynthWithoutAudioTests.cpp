// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthWithoutAudioTests.h"
#include "MidiSynthTestRig.h"

#include "MidiSynth/DispatchWithoutAudio.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;
using namespace MidiSynthTestRig;

namespace ci = WindowsMidiServicesCapabilityInquiry;

namespace
{
    constexpr uint32_t OurMuid = 0x0123456;
    constexpr uint32_t OtherDevice = 0x4A75301;

    constexpr uint8_t ChurchOrgan = 19;

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

    QueuedUmp Midi1(_In_ uint8_t status, _In_ uint8_t channel, _In_ uint8_t data1, _In_ uint8_t data2)
    {
        QueuedUmp message{};
        message.Words[0] =
            (2u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
            (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7F);
        message.WordCount = 1;

        return message;
    }

    QueuedUmp Midi2(_In_ uint8_t status, _In_ uint8_t channel, _In_ uint8_t index1, _In_ uint32_t data)
    {
        QueuedUmp message{};
        message.Words[0] =
            (4u << 28) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
            (static_cast<uint32_t>(index1) << 8);
        message.Words[1] = data;
        message.WordCount = 2;

        return message;
    }

    // A MIDI-CI Discovery, one queued packet at a time, the way the transport queues it.
    void QueueDiscovery(_Inout_ UmpInboundQueue& queue)
    {
        ci::DiscoveryReplyFields fields{};
        fields.SourceMuid = OtherDevice;
        fields.ManufacturerSysExId[0] = 0x7D;
        fields.CapabilityCategories = ci::CategoryPropertyExchange;
        fields.ReceivableMaximumSysExSize = 4096;
        fields.MessageVersion = 0x02;

        uint8_t buffer[64]{};
        const auto size = ci::BuildDiscovery(fields, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        CaptureOutput packets;
        UmpDispatcher::PacketizeSysEx7(packets, 0, buffer, size);

        for (size_t i = 0; i + 1 < packets.Words.size(); i += 2)
        {
            QueuedUmp packet{};
            packet.Words[0] = packets.Words[i];
            packet.Words[1] = packets.Words[i + 1];
            packet.WordCount = 2;

            VERIFY_IS_TRUE(queue.TryPush(packet));
        }
    }

    size_t DiscoveryReplies(_In_ const CaptureOutput& output)
    {
        size_t replies{ 0 };
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
                parsed.Type == ci::MessageType::DiscoveryReply)
            {
                replies++;
            }
        }

        return replies;
    }
}

void MidiSynthWithoutAudioTests::TestPeekLeavesTheItemQueued()
{
    SpscRingBuffer<int, 4> ring;
    int item{ 0 };

    VERIFY_IS_FALSE(ring.TryPeek(item), L"nothing to see in an empty ring");

    VERIFY_IS_TRUE(ring.TryPush(1));
    VERIFY_IS_TRUE(ring.TryPush(2));

    VERIFY_IS_TRUE(ring.TryPeek(item));
    VERIFY_ARE_EQUAL(1, item);

    item = 0;
    VERIFY_IS_TRUE(ring.TryPeek(item));
    VERIFY_ARE_EQUAL(1, item, L"looking twice sees the same item");

    VERIFY_IS_TRUE(ring.TryPop(item));
    VERIFY_ARE_EQUAL(1, item, L"and taking it takes that one");

    VERIFY_IS_TRUE(ring.TryPeek(item));
    VERIFY_ARE_EQUAL(2, item);
}

// Discovery is answered with no audio device, but a channel voice message stops everything behind
// it, in either protocol, and stays queued for the render thread however often this runs.
void MidiSynthWithoutAudioTests::TestChannelVoiceWaitsForAudio()
{
    // Never initialized: none of this needs a sound set.
    SynthEngine engine;
    UmpDispatcher dispatcher;
    dispatcher.Initialize(&engine, 0, OurMuid);

    CaptureOutput output;
    dispatcher.SetOutput(&output, SynthIdentity{});

    auto queue = std::make_unique<UmpInboundQueue>();

    const auto noteOn = Midi1(0x9, 0, 60, 100);

    QueueDiscovery(*queue);
    VERIFY_IS_TRUE(queue->TryPush(noteOn));
    QueueDiscovery(*queue);

    VERIFY_IS_TRUE(DispatchWithoutAudio(*queue, dispatcher, false) == WithoutAudioResult::NeedsAudio);
    VERIFY_ARE_EQUAL(size_t{ 1 }, DiscoveryReplies(output), L"what came before the note is answered");

    QueuedUmp head{};

    VERIFY_IS_TRUE(queue->TryPeek(head));
    VERIFY_ARE_EQUAL(noteOn.Words[0], head.Words[0], L"the note waits at the head of the queue");

    VERIFY_IS_TRUE(DispatchWithoutAudio(*queue, dispatcher, false) == WithoutAudioResult::NeedsAudio);
    VERIFY_ARE_EQUAL(size_t{ 1 }, DiscoveryReplies(output), L"and nothing behind it moves");

    auto midi2Queue = std::make_unique<UmpInboundQueue>();
    const auto controller = Midi2(0xB, 0, 7, 0x80000000u);

    VERIFY_IS_TRUE(midi2Queue->TryPush(controller));

    VERIFY_IS_TRUE(DispatchWithoutAudio(*midi2Queue, dispatcher, false) == WithoutAudioResult::NeedsAudio,
        L"a MIDI 2.0 channel voice message waits too");

    VERIFY_IS_TRUE(midi2Queue->TryPeek(head));
    VERIFY_ARE_EQUAL(controller.Words[0], head.Words[0]);

    auto emptyQueue = std::make_unique<UmpInboundQueue>();

    VERIFY_IS_TRUE(DispatchWithoutAudio(*emptyQueue, dispatcher, false) == WithoutAudioResult::Drained);
}

// With no audio device to be had, notes are dropped in both protocols, and everything else still
// reaches the engine and the responder.
void MidiSynthWithoutAudioTests::TestDiscardingDropsOnlyNoteOns()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    SynthEngine engine;
    engine.Initialize(collection, TestConfig());

    UmpDispatcher dispatcher;
    dispatcher.Initialize(&engine, 0, OurMuid);

    CaptureOutput output;
    dispatcher.SetOutput(&output, SynthIdentity{});

    auto queue = std::make_unique<UmpInboundQueue>();

    const auto noteOn = Midi1(0x9, 0, 60, 100);

    VERIFY_IS_TRUE(queue->TryPush(Midi1(0xC, 0, ChurchOrgan, 0)));
    VERIFY_IS_TRUE(queue->TryPush(Midi1(0xB, 0, 7, 20)));
    VERIFY_IS_TRUE(queue->TryPush(noteOn));
    VERIFY_IS_TRUE(queue->TryPush(Midi2(0x9, 0, 64, 0xC0000000u)));
    QueueDiscovery(*queue);

    VERIFY_IS_TRUE(DispatchWithoutAudio(*queue, dispatcher, true) == WithoutAudioResult::Drained);

    QueuedUmp left{};

    VERIFY_IS_FALSE(queue->TryPeek(left), L"nothing waits");
    VERIFY_ARE_EQUAL(0u, engine.ActiveVoiceCount(), L"no note started");
    VERIFY_ARE_EQUAL(static_cast<int>(ChurchOrgan), static_cast<int>(engine.ChannelState(0).Program), L"the program change arrived");
    VERIFY_IS_TRUE(std::fabs(engine.ChannelState(0).Volume - 20.0 / 127.0) < 1e-9, L"and the volume");
    VERIFY_ARE_EQUAL(size_t{ 1 }, DiscoveryReplies(output), L"and MIDI-CI was answered");

    // The same note dispatched does start a voice, so the check above means something.
    (void)dispatcher.ProcessWords(noteOn.Words, noteOn.WordCount);

    VERIFY_IS_TRUE(engine.ActiveVoiceCount() > 0);
}

// Nothing a request asks for happens until the render thread renders, or, once the stream has
// stopped, until the owner applies what is left.
void MidiSynthWithoutAudioTests::TestRenderThreadMakesRequestedChanges()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    SynthEngine engine;
    engine.Initialize(collection, TestConfig());

    UmpDispatcher dispatcher;
    dispatcher.Initialize(&engine, 0, OurMuid);

    auto queue = std::make_unique<UmpInboundQueue>();

    constexpr uint32_t BlockFrames = 256;

    UmpRenderSource source(engine, dispatcher, *queue, TestConfig().RenderSampleRate(), BlockFrames, nullptr);
    std::vector<float> block(static_cast<size_t>(BlockFrames) * 2);

    const auto render = [&](_In_ double seconds)
    {
        const auto blocks = static_cast<uint32_t>(seconds * TestConfig().RenderSampleRate() / BlockFrames) + 1;

        for (uint32_t i = 0; i < blocks; i++)
        {
            source.RenderAudio(block.data(), BlockFrames);
        }
    };

    source.RequestDrumChannel(9, false);
    source.RequestDrumChannel(3, true);
    source.RequestDrumChannel(4, true);
    source.RequestDrumChannel(4, false);

    VERIFY_IS_TRUE(engine.IsDrumChannel(9), L"nothing changes before the render thread runs");
    VERIFY_IS_FALSE(engine.IsDrumChannel(3));

    render(0.0);

    VERIFY_IS_FALSE(engine.IsDrumChannel(9));
    VERIFY_IS_TRUE(engine.IsDrumChannel(3));
    VERIFY_IS_FALSE(engine.IsDrumChannel(4), L"the latest request for a channel wins");
    VERIFY_IS_FALSE(engine.IsDrumChannel(0), L"and the others are left alone");

    const auto before = engine.Config().BankSelect;
    const auto requested = (before == BankSelectMode::YamahaXG) ? BankSelectMode::RolandGS : BankSelectMode::YamahaXG;

    source.RequestBankSelectMode(requested);

    VERIFY_IS_TRUE(engine.Config().BankSelect == before);

    render(0.0);

    VERIFY_IS_TRUE(engine.Config().BankSelect == requested);

    // Choosing a convention also sets the one Automatic follows. XG and then Automatic, both asked
    // for before the render thread runs, must still leave XG behind for Automatic to follow. Read as
    // XG, bank 0/16 is program 1's variation, "Piano 1d"; read as GS, it's the capital tone.
    const auto* const variation = collection->FindInstrument(16, 0, 0, false);
    const auto* const capitalTone = collection->FindInstrument(0, 0, 0, false);

    VERIFY_IS_TRUE(variation != nullptr && capitalTone != nullptr && variation != capitalTone);

    source.RequestBankSelectMode(BankSelectMode::RolandGS);
    render(0.0);
    engine.ProgramChangeWithBank(0, 0, 16, 0);

    VERIFY_IS_TRUE(engine.ChannelState(0).Instrument == capitalTone, L"read as GS, the LSB chooses nothing");

    source.RequestBankSelectMode(BankSelectMode::YamahaXG);
    source.RequestBankSelectMode(BankSelectMode::Automatic);
    render(0.0);

    VERIFY_IS_TRUE(engine.Config().BankSelect == BankSelectMode::Automatic, L"the latest request is the setting");
    VERIFY_IS_TRUE(engine.ChannelState(0).Instrument == variation, L"and Automatic follows the XG convention chosen just before it");

    engine.ProgramChangeWithBank(0, 0, 0, 0);

    // An organ holds its note for as long as the key is down, so only silence ends it.
    VERIFY_IS_TRUE(queue->TryPush(Midi1(0xC, 0, ChurchOrgan, 0)));
    VERIFY_IS_TRUE(queue->TryPush(Midi1(0x9, 0, 60, 100)));

    render(0.3);

    VERIFY_IS_TRUE(engine.ActiveVoiceCount() > 0);

    source.RequestSilence();
    render(0.5);

    VERIFY_ARE_EQUAL(0u, engine.ActiveVoiceCount(), L"silence fades every voice out");

    // The stream has stopped, so the owner finishes what the render thread didn't get to.
    source.RequestDrumChannel(5, true);

    VERIFY_IS_FALSE(engine.IsDrumChannel(5));

    source.ApplyRequests();

    VERIFY_IS_TRUE(engine.IsDrumChannel(5));
}
