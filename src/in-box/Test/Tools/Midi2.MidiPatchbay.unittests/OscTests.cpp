// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscTests.h"

#include "OscAddress.h"
#include "OscConnection.h"
#include "OscPacket.h"
#include "OscRules.h"
#include "OscStream.h"
#include "OscTime.h"
#include "OscTranslate.h"
#include "PatchSerializer.h"
#include "RouteGraph.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace midipatchbay;
using namespace midipatchbay::osc;
using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    struct ReadArgument
    {
        char Type{ 0 };
        int64_t Integer{ 0 };
        double Real{ 0 };
        uint64_t Time{ 0 };
        std::string Text{};
        std::vector<uint8_t> Bytes{};
    };

    struct ReadMessageCopy
    {
        std::string Address{};
        std::string TypeTags{};
        std::vector<ReadArgument> Arguments{};
        uint64_t TimeTag{ 0 };
    };

    // Copies everything a packet holds, so the test can look at it after the packet is gone.
    class Collector final : public MessageVisitor
    {
    public:
        std::vector<ReadMessageCopy> Messages{};
        std::vector<ReadProblem> Problems{};

        void OnMessage(_In_ MessageView const& message, _In_ uint64_t timeTag) noexcept override
        {
            ReadMessageCopy copy{};
            copy.Address = std::string{ message.Address };
            copy.TypeTags = std::string{ message.TypeTags };
            copy.TimeTag = timeTag;

            for (size_t i = 0; i < message.ArgumentCount; i++)
            {
                auto const& argument = message.Arguments[i];
                copy.Arguments.push_back(ReadArgument{
                    argument.Type, argument.Integer, argument.Real, argument.Time,
                    std::string{ argument.Text }, std::vector<uint8_t>{ argument.Bytes.begin(), argument.Bytes.end() } });
            }

            Messages.push_back(std::move(copy));
        }

        void OnUnreadable(_In_ ReadProblem problem) noexcept override
        {
            Problems.push_back(problem);
        }
    };

    class UmpCollector final : public UmpReceiver
    {
    public:
        std::vector<std::vector<uint32_t>> Umps{};

        void OnUmp(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept override
        {
            Umps.emplace_back(words, words + wordCount);
        }

        std::vector<uint32_t> FirstWords() const
        {
            std::vector<uint32_t> words{};

            for (auto const& ump : Umps)
            {
                words.push_back(ump[0]);
            }

            return words;
        }
    };

    std::vector<uint8_t> Bytes(_In_ std::initializer_list<int> values)
    {
        std::vector<uint8_t> bytes{};

        for (auto const v : values)
        {
            bytes.push_back(static_cast<uint8_t>(v));
        }

        return bytes;
    }

    // Sends one message through the writer, the reader and then the rows, as the receive thread does.
    InboundOutcome Receive(
        _In_ std::vector<OscRow> const& rows,
        _In_ MessageBuilder const& builder,
        _Inout_ UmpCollector& umps)
    {
        auto const packet = MakePacket(builder);

        MessageView view{};
        VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::None), static_cast<int32_t>(ReadMessage(packet.data(), packet.size(), view)));

        return TranslateInbound(rows, view, umps);
    }

    std::vector<uint32_t> ReceiveWords(_In_ std::vector<OscRow> const& rows, _In_ MessageBuilder const& builder)
    {
        UmpCollector umps{};
        auto const outcome = Receive(rows, builder, umps);

        VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Translated), static_cast<int32_t>(outcome.Result));

        return umps.FirstWords();
    }

    OscRow Preset(_In_ OscPreset preset, _In_ uint8_t group = 0)
    {
        return OscRowFromPreset(preset, group);
    }

    // What one OSC message looks like on the wire, read back.
    ReadMessageCopy Reread(_In_ MessageBuilder const& builder)
    {
        auto const packet = MakePacket(builder);

        Collector collector{};
        ReadPacket(packet.data(), packet.size(), collector);

        VERIFY_ARE_EQUAL(size_t{ 1 }, collector.Messages.size());
        return collector.Messages[0];
    }

    // The 3-byte MIDI 1.0 message in a UMP type 2 word, on group 0.
    uint32_t Cv(_In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2, _In_ uint8_t group = 0)
    {
        return (0x2u << 28) | (static_cast<uint32_t>(group) << 24) | (static_cast<uint32_t>(status) << 16) |
            (static_cast<uint32_t>(data1) << 8) | data2;
    }

    uint32_t Sys(_In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2, _In_ uint8_t group = 0)
    {
        return (0x1u << 28) | (static_cast<uint32_t>(group) << 24) | (static_cast<uint32_t>(status) << 16) |
            (static_cast<uint32_t>(data1) << 8) | data2;
    }
}

void OscTests::TheSpecExamplesAreByteExact()
{
    // The two examples on the OSC 1.0 specification's examples page.
    auto const first = MakePacket(MessageBuilder{ "/oscillator/4/frequency" }.Float(440.0f));

    auto const firstExpected = Bytes({
        0x2f, 0x6f, 0x73, 0x63, 0x69, 0x6c, 0x6c, 0x61, 0x74, 0x6f, 0x72, 0x2f, 0x34, 0x2f, 0x66, 0x72,
        0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x79, 0x00, 0x2c, 0x66, 0x00, 0x00, 0x43, 0xdc, 0x00, 0x00 });

    VERIFY_IS_TRUE(first == firstExpected);

    auto const second = MakePacket(MessageBuilder{ "/foo" }.Int(1000).Int(-1).String("hello").Float(1.234f).Float(5.678f));

    auto const secondExpected = Bytes({
        0x2f, 0x66, 0x6f, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x69, 0x69, 0x73, 0x66, 0x66, 0x00, 0x00,
        0x00, 0x00, 0x03, 0xe8, 0xff, 0xff, 0xff, 0xff, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0x00, 0x00,
        0x3f, 0x9d, 0xf3, 0xb6, 0x40, 0xb5, 0xb2, 0x2d });

    VERIFY_IS_TRUE(second == secondExpected);

    Collector collector{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::None), static_cast<int32_t>(ReadPacket(second.data(), second.size(), collector)));
    VERIFY_ARE_EQUAL(size_t{ 1 }, collector.Messages.size());

    auto const& message = collector.Messages[0];
    VERIFY_ARE_EQUAL(std::string{ "/foo" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "iisff" }, message.TypeTags);
    VERIFY_ARE_EQUAL(int64_t{ 1000 }, message.Arguments[0].Integer);
    VERIFY_ARE_EQUAL(int64_t{ -1 }, message.Arguments[1].Integer);
    VERIFY_ARE_EQUAL(std::string{ "hello" }, message.Arguments[2].Text);
    VERIFY_ARE_EQUAL(1.234f, static_cast<float>(message.Arguments[3].Real));
    VERIFY_ARE_EQUAL(5.678f, static_cast<float>(message.Arguments[4].Real));
    VERIFY_ARE_EQUAL(Immediately, message.TimeTag);
}

void OscTests::BundlesNestAndCarryTheirTimeTags()
{
    std::vector<MessageBuilder> inner{ MessageBuilder{ "/b" }.Int(2) };
    auto const innerBundle = MakeBundle(0x0000'0002'0000'0000ull, inner);

    std::vector<MessageBuilder> outer{ MessageBuilder{ "/a" }.Int(1) };
    auto packet = MakeBundle(0x0000'0001'8000'0000ull, outer);

    // A bundle inside a bundle, after the outer bundle's own message.
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(static_cast<uint8_t>(innerBundle.size() >> 8));
    packet.push_back(static_cast<uint8_t>(innerBundle.size()));
    packet.insert(packet.end(), innerBundle.begin(), innerBundle.end());

    Collector collector{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::None), static_cast<int32_t>(ReadPacket(packet.data(), packet.size(), collector)));
    VERIFY_ARE_EQUAL(size_t{ 2 }, collector.Messages.size());
    VERIFY_ARE_EQUAL(std::string{ "/a" }, collector.Messages[0].Address);
    VERIFY_ARE_EQUAL(0x0000'0001'8000'0000ull, collector.Messages[0].TimeTag);
    VERIFY_ARE_EQUAL(std::string{ "/b" }, collector.Messages[1].Address);
    VERIFY_ARE_EQUAL(0x0000'0002'0000'0000ull, collector.Messages[1].TimeTag);

    // Nine levels deep is one too many.
    std::vector<uint8_t> deep = MakePacket(MessageBuilder{ "/x" });

    for (int level = 0; level < 9; level++)
    {
        auto wrapped = MakeBundle(Immediately, {});
        wrapped.push_back(0);
        wrapped.push_back(0);
        wrapped.push_back(static_cast<uint8_t>(deep.size() >> 8));
        wrapped.push_back(static_cast<uint8_t>(deep.size()));
        wrapped.insert(wrapped.end(), deep.begin(), deep.end());
        deep = std::move(wrapped);
    }

    Collector deepCollector{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::TooDeep), static_cast<int32_t>(ReadPacket(deep.data(), deep.size(), deepCollector)));
    VERIFY_ARE_EQUAL(size_t{ 0 }, deepCollector.Messages.size());
}

void OscTests::EveryArgumentTypeReads()
{
    // Built by hand: the writer doesn't make h, d, t, c, r, N, I or arrays.
    std::vector<uint8_t> packet = Bytes({ '/', 'a', 0, 0, ',', 'h', 'd', 't', 'c', 'r', 'N', 'I', '[', 'T', 'F', ']', 'm', 'b', 'S', 0 });

    auto const add = [&](std::initializer_list<int> values) { auto const b = Bytes(values); packet.insert(packet.end(), b.begin(), b.end()); };

    add({ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE });                 // h -2
    add({ 0x40, 0x09, 0x21, 0xFB, 0x54, 0x44, 0x2D, 0x18 });                 // d pi
    add({ 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01 });                 // t
    add({ 0x00, 0x00, 0x00, 'Z' });                                          // c
    add({ 0x11, 0x22, 0x33, 0x44 });                                         // r
    add({ 0x01, 0x90, 0x3C, 0x64 });                                         // m
    add({ 0x00, 0x00, 0x00, 0x03, 0xF0, 0x01, 0xF7, 0x00 });                 // b, 3 bytes and one of padding
    add({ 'S', 'y', 'm', 0 });                                               // S

    MessageView view{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::None), static_cast<int32_t>(ReadMessage(packet.data(), packet.size(), view)));
    VERIFY_ARE_EQUAL(size_t{ 14 }, view.ArgumentCount);
    VERIFY_ARE_EQUAL(int64_t{ -2 }, view.Arguments[0].Integer);
    VERIFY_IS_TRUE(std::fabs(view.Arguments[1].Real - 3.141592653589793) < 1e-15);
    VERIFY_ARE_EQUAL(0x0000'0005'0000'0001ull, view.Arguments[2].Time);
    VERIFY_ARE_EQUAL(int64_t{ 'Z' }, view.Arguments[3].Integer);
    VERIFY_ARE_EQUAL(int64_t{ 0x11223344 }, view.Arguments[4].Integer);
    VERIFY_ARE_EQUAL('N', view.Arguments[5].Type);
    VERIFY_ARE_EQUAL('I', view.Arguments[6].Type);
    VERIFY_ARE_EQUAL('[', view.Arguments[7].Type);
    VERIFY_ARE_EQUAL(int64_t{ 1 }, view.Arguments[8].Integer);
    VERIFY_ARE_EQUAL(int64_t{ 0 }, view.Arguments[9].Integer);
    VERIFY_ARE_EQUAL(']', view.Arguments[10].Type);
    VERIFY_ARE_EQUAL(int64_t{ 0x01903C64 }, view.Arguments[11].Integer);
    VERIFY_ARE_EQUAL(size_t{ 3 }, view.Arguments[12].Bytes.size());
    VERIFY_ARE_EQUAL(uint8_t{ 0xF7 }, view.Arguments[12].Bytes[2]);
    VERIFY_ARE_EQUAL(std::string_view{ "Sym" }, view.Arguments[13].Text);
}

void OscTests::BrokenPacketsAreRefused()
{
    auto const good = MakePacket(MessageBuilder{ "/abc" }.Int(1).String("hi").Blob(Bytes({ 1, 2, 3 })));

    auto const read = [](std::vector<uint8_t> const& bytes)
        {
            MessageView view{};
            return ReadMessage(bytes.data(), bytes.size(), view);
        };

    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::None), static_cast<int32_t>(read(good)));

    // Every shorter length is refused, whether or not it is a multiple of four.
    for (size_t length = 0; length < good.size(); length++)
    {
        std::vector<uint8_t> cut{ good.begin(), good.begin() + length };
        VERIFY_ARE_NOT_EQUAL(static_cast<int32_t>(ReadProblem::None), static_cast<int32_t>(read(cut)));
    }

    // A blob that claims more than the packet holds, including a length that would wrap.
    auto blobTooLong = MakePacket(MessageBuilder{ "/a" }.Blob(Bytes({ 1, 2, 3, 4 })));
    blobTooLong[8] = 0;
    blobTooLong[9] = 0;
    blobTooLong[10] = 0;
    blobTooLong[11] = 9;
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::Malformed), static_cast<int32_t>(read(blobTooLong)));

    blobTooLong[8] = 0xFF;
    blobTooLong[9] = 0xFF;
    blobTooLong[10] = 0xFF;
    blobTooLong[11] = 0xFE;
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::Malformed), static_cast<int32_t>(read(blobTooLong)));

    // A string with no terminating zero.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::Malformed), static_cast<int32_t>(read(Bytes({ '/', 'a', 'b', 'c' }))));

    // An address with no type tags, as old senders do.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::NoTypeTags), static_cast<int32_t>(read(Bytes({ '/', 'a', 0, 0 }))));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::NoTypeTags), static_cast<int32_t>(read(Bytes({ '/', 'a', 0, 0, 0, 0, 0, 1 }))));

    // A type nobody knows the size of.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::UnknownType), static_cast<int32_t>(read(Bytes({ '/', 'a', 0, 0, ',', 'x', 0, 0 }))));

    // Not starting with a slash.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::Malformed), static_cast<int32_t>(read(Bytes({ 'a', 'b', 0, 0, ',', 0, 0, 0 }))));

    // 65 arguments is one too many.
    MessageBuilder many{ "/m" };

    for (int i = 0; i < 65; i++)
    {
        many.Bool(true);
    }

    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::TooManyArguments), static_cast<int32_t>(read(MakePacket(many))));

    // A bundle element with a size that isn't a multiple of four, or runs past the end. The
    // message before it is still read.
    std::vector<MessageBuilder> one{ MessageBuilder{ "/ok" } };
    auto bundle = MakeBundle(Immediately, one);
    bundle.insert(bundle.end(), { 0, 0, 0, 6, '/', 'x', 0, 0 });

    Collector collector{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(ReadProblem::Malformed), static_cast<int32_t>(ReadPacket(bundle.data(), bundle.size(), collector)));
    VERIFY_ARE_EQUAL(size_t{ 1 }, collector.Messages.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, collector.Problems.size());
}

void OscTests::MutatedPacketsNeverReadOutOfBounds()
{
    std::vector<std::vector<uint8_t>> seeds{};

    seeds.push_back(MakePacket(MessageBuilder{ "/vkb_midi/0/note/60" }.Int(100)));
    seeds.push_back(MakePacket(MessageBuilder{ "/midi" }.Midi(0, 0x90, 60, 100).Midi(1, 0xB0, 7, 64)));
    seeds.push_back(MakePacket(MessageBuilder{ "/sysex" }.String("f0 7e 7f 06 01 f7")));
    seeds.push_back(MakePacket(MessageBuilder{ "/midi/sysex" }.Blob(Bytes({ 0xF0, 1, 2, 3, 4, 0xF7 }))));

    std::vector<MessageBuilder> messages{ MessageBuilder{ "/note" }.Int(1).Int(60).Float(0.5f), MessageBuilder{ "/x/*/y" }.Bool(true) };
    auto const bundle = MakeBundle(0x1234'5678'9ABC'DEF0ull, messages);
    seeds.push_back(bundle);

    std::vector<OscRow> rows
    {
        Preset(OscPreset::ReaperVirtualKeyboard, 0),
        Preset(OscPreset::MidiType, 1),
        Preset(OscPreset::OpenStageControl, 2),
    };

    std::mt19937 random{ 20261022 };

    // Checks every view the reader hands out lies inside the packet, then translates the message.
    class BoundsChecker final : public MessageVisitor
    {
    public:
        uint8_t const* Begin{ nullptr };
        uint8_t const* End{ nullptr };
        std::vector<OscRow> const* Rows{ nullptr };
        size_t Messages{ 0 };
        bool Outside{ false };

        void Check(void const* data, size_t size) noexcept
        {
            auto const start = static_cast<uint8_t const*>(data);

            if (size > 0 && (start < Begin || start + size > End))
            {
                Outside = true;
            }
        }

        void OnMessage(_In_ MessageView const& message, _In_ uint64_t) noexcept override
        {
            Messages++;
            Check(message.Address.data(), message.Address.size());
            Check(message.TypeTags.data(), message.TypeTags.size());

            for (size_t i = 0; i < message.ArgumentCount; i++)
            {
                Check(message.Arguments[i].Text.data(), message.Arguments[i].Text.size());
                Check(message.Arguments[i].Bytes.data(), message.Arguments[i].Bytes.size());
            }

            class Sink final : public UmpReceiver
            {
            public:
                void OnUmp(_In_reads_(wordCount) uint32_t const*, _In_ uint8_t wordCount) noexcept override
                {
                    Count += wordCount;
                }

                size_t Count{ 0 };
            } sink;

            TranslateInbound(*Rows, message, sink);
        }

        void OnUnreadable(_In_ ReadProblem) noexcept override
        {
        }
    };

    size_t delivered{ 0 };
    constexpr int Rounds = 200000;

    for (int round = 0; round < Rounds; round++)
    {
        auto bytes = seeds[random() % seeds.size()];
        auto const mutations = 1 + random() % 4;

        for (uint32_t m = 0; m < mutations; m++)
        {
            switch (random() % 4)
            {
            case 0: bytes[random() % bytes.size()] = static_cast<uint8_t>(random()); break;
            case 1: bytes[random() % bytes.size()] ^= static_cast<uint8_t>(1u << (random() % 8)); break;
            case 2:
            {
                auto const cut = static_cast<size_t>(random() % 3) * 4;
                bytes.resize(bytes.size() > cut + 4 ? bytes.size() - cut : 4);
                break;
            }
            case 3: bytes[random() % bytes.size()] = static_cast<uint8_t>("/*?[]{},0xF7"[random() % 12]); break;
            }
        }

        // An exact-size copy, so reading one byte past the end would leave the allocation.
        auto const packet = std::make_unique<uint8_t[]>(bytes.size());
        std::copy(bytes.begin(), bytes.end(), packet.get());

        BoundsChecker checker{};
        checker.Begin = packet.get();
        checker.End = packet.get() + bytes.size();
        checker.Rows = &rows;

        ReadPacket(packet.get(), bytes.size(), checker);

        VERIFY_IS_FALSE(checker.Outside);
        delivered += checker.Messages;
    }

    // Proof the mutations leave plenty of packets readable, so the translation was exercised too.
    Log::Comment(String().Format(L"%zu messages read from %d mutated packets", delivered, Rounds));
    VERIFY_IS_TRUE(delivered > Rounds / 4);
}

void OscTests::PatternsMatchAsTheSpecSays()
{
    VERIFY_IS_TRUE(PartMatches("abc", "abc"));
    VERIFY_IS_FALSE(PartMatches("abc", "abd"));
    VERIFY_IS_TRUE(PartMatches("a?c", "abc"));
    VERIFY_IS_FALSE(PartMatches("a?c", "ac"));
    VERIFY_IS_TRUE(PartMatches("*", ""));
    VERIFY_IS_TRUE(PartMatches("*", "anything"));
    VERIFY_IS_TRUE(PartMatches("a*", "abc"));
    VERIFY_IS_TRUE(PartMatches("*c", "abc"));
    VERIFY_IS_TRUE(PartMatches("a*b*c", "aXbYc"));
    VERIFY_IS_FALSE(PartMatches("a*b*c", "aXbY"));
    VERIFY_IS_TRUE(PartMatches("[abc]", "b"));
    VERIFY_IS_FALSE(PartMatches("[a-c]", "d"));
    VERIFY_IS_TRUE(PartMatches("[!a-c]", "d"));
    VERIFY_IS_FALSE(PartMatches("[!a]", "a"));
    VERIFY_IS_TRUE(PartMatches("[-a]", "-"));
    VERIFY_IS_TRUE(PartMatches("[a-]", "-"));
    VERIFY_IS_TRUE(PartMatches("{foo,bar}", "bar"));
    VERIFY_IS_FALSE(PartMatches("{foo,bar}", "baz"));

    // Alternatives of different lengths need every way tried, not only the first that fits.
    VERIFY_IS_TRUE(PartMatches("x{a,ab}c", "xabc"));
    VERIFY_IS_TRUE(PartMatches("{,x}y", "y"));

    // Unclosed brackets and braces match nothing.
    VERIFY_IS_FALSE(PartMatches("[abc", "a"));
    VERIFY_IS_FALSE(PartMatches("{abc", "abc"));

    // A pattern that makes a naive matcher backtrack for ever finishes at once here.
    std::string const many(200, 'a');
    VERIFY_IS_FALSE(PartMatches("*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b", many));

    VERIFY_IS_TRUE(PatternMatches("/vkb_midi/*/cc/7", "/vkb_midi/3/cc/7"));
    VERIFY_IS_FALSE(PatternMatches("/vkb_midi/*", "/vkb_midi/3/cc"));
    VERIFY_IS_TRUE(PatternMatches("//cc/7", "/vkb_midi/3/cc/7"));
    VERIFY_IS_TRUE(PatternMatches("/vkb_midi//7", "/vkb_midi/3/cc/7"));
    VERIFY_IS_TRUE(PatternMatches("/a//b", "/a/b"));
    VERIFY_IS_TRUE(PatternMatches("/a//b", "/a/x/y/b"));
    VERIFY_IS_FALSE(PatternMatches("/a//b", "/a/x/y/c"));
    VERIFY_IS_FALSE(PatternMatches("/a/", "/a"));
    VERIFY_IS_FALSE(PatternMatches("/a/b", "/a//b"));
    VERIFY_IS_FALSE(PatternMatches("a/b", "/a/b"));

    VERIFY_IS_TRUE(HasPatternCharacters("/a/*"));
    VERIFY_IS_TRUE(HasPatternCharacters("//a"));
    VERIFY_IS_FALSE(HasPatternCharacters("/vkb_midi/0/note/60"));

    VERIFY_IS_TRUE(IsValidRoot(""));
    VERIFY_IS_TRUE(IsValidRoot("/vkb_midi"));
    VERIFY_IS_TRUE(IsValidRoot("/a/b"));
    VERIFY_IS_FALSE(IsValidRoot("/"));
    VERIFY_IS_FALSE(IsValidRoot("/a/"));
    VERIFY_IS_FALSE(IsValidRoot("a"));
    VERIFY_IS_FALSE(IsValidRoot("/a b"));
    VERIFY_IS_FALSE(IsValidRoot("/a*"));
    VERIFY_IS_FALSE(IsValidRoot("/a//b"));
    VERIFY_IS_FALSE(IsValidRoot("/caf\xC3\xA9"));
}

void OscTests::TimeTagsBecomeTimestamps()
{
    // January 1, 1970 is 2,208,988,800 NTP seconds and 116,444,736,000,000,000 FILETIME units.
    constexpr uint64_t Epoch1970 = 116'444'736'000'000'000ull;

    VERIFY_ARE_EQUAL(2'208'988'800ull << 32, TimeTagFromFileTime(Epoch1970));
    VERIFY_ARE_EQUAL(Epoch1970, FileTimeFromTimeTag(2'208'988'800ull << 32));

    // Half a second is half the fraction.
    VERIFY_ARE_EQUAL((2'208'988'800ull << 32) | 0x8000'0000ull, TimeTagFromFileTime(Epoch1970 + 5'000'000));

    // Every FILETIME comes back exactly.
    std::mt19937_64 random{ 7 };

    for (int i = 0; i < 100000; i++)
    {
        auto const fileTime = Epoch1970 + random() % (100ull * 365 * 86400 * FileTimeTicksPerSecond);
        VERIFY_ARE_EQUAL(fileTime, FileTimeFromTimeTag(TimeTagFromFileTime(fileTime)));
    }

    // Ticks: half a second after the clock pair, at 10 MHz and at 24 MHz.
    for (uint64_t const rate : { 10'000'000ull, 24'000'000ull })
    {
        ClockPair const clock{ Epoch1970 + 55 * 365ull * 86400 * FileTimeTicksPerSecond, 123'456'789'000ull, rate };

        auto const tag = TimeTagFromFileTime(clock.FileTime + 5'000'000);

        VERIFY_ARE_EQUAL(clock.Ticks + rate / 2, TicksFromTimeTag(tag, clock));
        VERIFY_ARE_EQUAL(tag, TimeTagFromTicks(clock.Ticks + rate / 2, clock));

        // Earlier, too.
        auto const before = TimeTagFromFileTime(clock.FileTime - 20'000'000);
        VERIFY_ARE_EQUAL(clock.Ticks - rate * 2, TicksFromTimeTag(before, clock));

        // "Immediately" is now, and two days away is held to one.
        VERIFY_ARE_EQUAL(clock.Ticks, TicksFromTimeTag(Immediately, clock));

        auto const farAway = TimeTagFromFileTime(clock.FileTime + 2ull * 86400 * FileTimeTicksPerSecond);
        VERIFY_ARE_EQUAL(clock.Ticks + rate * 86400, TicksFromTimeTag(farAway, clock));
    }
}

void OscTests::TimeTagsWrapIn2036()
{
    // NTP's seconds count reaches 2^32 on February 7, 2036 at 06:28:16 UTC.
    constexpr uint64_t Wrap = (4'294'967'296ull + SecondsFrom1601To1900) * FileTimeTicksPerSecond;

    VERIFY_ARE_EQUAL(uint64_t{ 0 }, TimeTagFromFileTime(Wrap));
    VERIFY_ARE_EQUAL(1ull << 32, TimeTagFromFileTime(Wrap + FileTimeTicksPerSecond));
    VERIFY_ARE_EQUAL(Wrap + FileTimeTicksPerSecond, FileTimeFromTimeTag(1ull << 32));

    // The second before it is still in the first era.
    VERIFY_ARE_EQUAL(0xFFFF'FFFFull << 32, TimeTagFromFileTime(Wrap - FileTimeTicksPerSecond));
    VERIFY_ARE_EQUAL(Wrap - FileTimeTicksPerSecond, FileTimeFromTimeTag(0xFFFF'FFFFull << 32));

    // Ticks carry on across it.
    ClockPair const clock{ Wrap - FileTimeTicksPerSecond / 2, 1'000'000'000ull, 10'000'000ull };
    VERIFY_ARE_EQUAL(clock.Ticks + 10'000'000ull, TicksFromTimeTag(TimeTagFromFileTime(Wrap + FileTimeTicksPerSecond / 2), clock));
}

void OscTests::SpacingIsKeptAndJitterRemoved()
{
    constexpr uint64_t Rate = 10'000'000;
    constexpr uint64_t Ms = Rate / 1000;

    // 1/128 of a second, exact in both units.
    constexpr uint64_t StepTicks = Rate / 128;
    constexpr uint64_t StepTag = 1ull << 25;

    SpacingKeeper keeper{};

    // Sent every step by a device whose clock started at 1900 when it powered on (no real-time
    // clock), so its tags are more than a century from ours. They arrive with up to 8 ms of
    // jitter; the first one has none, so the gap is known from the start.
    uint64_t const tagStart = 5ull << 32;
    uint64_t const arrivalStart = 1'000'000 * Ms;

    std::mt19937 random{ 3 };
    uint64_t previous{ 0 };

    for (uint64_t i = 0; i < 300; i++)
    {
        auto const tag = tagStart + i * StepTag;
        auto const jitter = i == 0 ? 0 : (random() % 9) * Ms;
        auto const arrival = arrivalStart + i * StepTicks + jitter;

        auto const playAt = keeper.PlayAt(tag, arrival, 10 * Ms, Rate);

        VERIFY_IS_TRUE(playAt >= arrival);
        VERIFY_ARE_EQUAL(arrivalStart + i * StepTicks + 10 * Ms, playAt);

        if (i > 0)
        {
            VERIFY_ARE_EQUAL(StepTicks, playAt - previous);
        }

        previous = playAt;
    }

    // After a long silence the sender's clock has moved. What was learned is forgotten, so the
    // new messages don't wait for the old gap.
    auto const later = arrivalStart + 10'000 * Ms;
    auto const playAt = keeper.PlayAt(tagStart, later, 10 * Ms, Rate);
    VERIFY_ARE_EQUAL(later + 10 * Ms, playAt);

    // A clock two seconds before NTP's 32-bit seconds count wraps keeps its spacing across the wrap.
    SpacingKeeper wrapping{};
    uint64_t const wrapTag = 0xFFFF'FFFEull << 32;

    for (uint64_t i = 0; i < 8; i++)
    {
        auto const tag = wrapTag + i * 64 * StepTag;
        auto const arrival = arrivalStart + i * 64 * StepTicks + (i == 0 ? 0 : 3 * Ms);

        VERIFY_ARE_EQUAL(arrivalStart + i * 64 * StepTicks + 10 * Ms, wrapping.PlayAt(tag, arrival, 10 * Ms, Rate));
    }
}

void OscTests::ReaperPathsBecomeMidi()
{
    std::vector<OscRow> const rows{ Preset(OscPreset::ReaperVirtualKeyboard, 3) };

    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/note/60" }.Int(100)) == std::vector<uint32_t>{ Cv(0x90, 60, 100, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/15/note/60" }.Int(0)) == std::vector<uint32_t>{ Cv(0x8F, 60, 0, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/2/cc/7" }.Float(64.0f)) == std::vector<uint32_t>{ Cv(0xB2, 7, 64, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/polyaftertouch/61" }.Int(30)) == std::vector<uint32_t>{ Cv(0xA0, 61, 30, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/program" }.Int(5)) == std::vector<uint32_t>{ Cv(0xC0, 5, 0, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/channelpressure" }.Int(64)) == std::vector<uint32_t>{ Cv(0xD0, 64, 0, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/pitch" }.Int(8192)) == std::vector<uint32_t>{ Cv(0xE0, 0, 64, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/pitch" }.Int(16383)) == std::vector<uint32_t>{ Cv(0xE0, 127, 127, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/0/noteoff/60" }.Int(64)) == std::vector<uint32_t>{ Cv(0x80, 60, 64, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/start" }) == std::vector<uint32_t>{ Sys(0xFA, 0, 0, 3) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/songposition" }.Int(300)) == std::vector<uint32_t>{ Sys(0xF2, 300 & 0x7F, 300 >> 7, 3) });

    // REAPER counts channels from 0, so 16 isn't one.
    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched),
        static_cast<int32_t>(Receive(rows, MessageBuilder{ "/vkb_midi/16/note/60" }.Int(1), umps).Result));

    // A row counting from 1 takes 16 and not 0.
    auto fromOne = Preset(OscPreset::Paths, 0);
    VERIFY_IS_TRUE(ReceiveWords({ fromOne }, MessageBuilder{ "/midi/16/cc/1" }.Int(1)) == std::vector<uint32_t>{ Cv(0xBF, 1, 1) });
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched),
        static_cast<int32_t>(Receive({ fromOne }, MessageBuilder{ "/midi/0/cc/1" }.Int(1), umps).Result));
}

void OscTests::ReaperAddressesWithoutAChannelUseTheRowsChannel()
{
    auto row = Preset(OscPreset::ReaperVirtualKeyboard, 1);
    row.Channel = 9;

    std::vector<OscRow> const rows{ row };

    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/note/36" }.Int(90)) == std::vector<uint32_t>{ Cv(0x99, 36, 90, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/cc/1" }.Int(5)) == std::vector<uint32_t>{ Cv(0xB9, 1, 5, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/pitch" }.Int(0)) == std::vector<uint32_t>{ Cv(0xE9, 0, 0, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/vkb_midi/program" }.Int(3)) == std::vector<uint32_t>{ Cv(0xC9, 3, 0, 1) });
}

void OscTests::TheMidiTypePicksTheRowByPort()
{
    std::vector<OscRow> const rows{ Preset(OscPreset::MidiType, 0), Preset(OscPreset::MidiType, 5) };

    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/midi" }.Midi(5, 0x91, 60, 100)) == std::vector<uint32_t>{ Cv(0x91, 60, 100, 5) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/midi" }.Midi(0, 0xF8, 0, 0)) == std::vector<uint32_t>{ Sys(0xF8, 0, 0, 0) });

    // Two in one message, for two rows.
    auto const both = ReceiveWords(rows, MessageBuilder{ "/midi" }.Midi(0, 0xB0, 7, 1).Midi(5, 0xB0, 7, 2));
    VERIFY_ARE_EQUAL(size_t{ 2 }, both.size());
    VERIFY_IS_TRUE(std::find(both.begin(), both.end(), Cv(0xB0, 7, 1, 0)) != both.end());
    VERIFY_IS_TRUE(std::find(both.begin(), both.end(), Cv(0xB0, 7, 2, 5)) != both.end());

    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched),
        static_cast<int32_t>(Receive(rows, MessageBuilder{ "/midi" }.Midi(7, 0x90, 60, 1), umps).Result));

    // m can't carry SysEx.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable),
        static_cast<int32_t>(Receive(rows, MessageBuilder{ "/midi" }.Midi(0, 0xF0, 1, 2), umps).Result));
}

void OscTests::ArgumentFormatsBecomeMidi()
{
    std::vector<OscRow> const openStage{ Preset(OscPreset::OpenStageControl, 0) };

    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/note" }.Int(1).Int(60).Int(100)) == std::vector<uint32_t>{ Cv(0x90, 60, 100) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/note" }.Int(1).Int(60).Int(0)) == std::vector<uint32_t>{ Cv(0x80, 60, 0) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/note_off" }.Int(2).Int(60)) == std::vector<uint32_t>{ Cv(0x81, 60, 0) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/control" }.Int(16).Int(7).Int(64)) == std::vector<uint32_t>{ Cv(0xBF, 7, 64) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/program" }.Int(1).Int(9)) == std::vector<uint32_t>{ Cv(0xC0, 9, 0) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/pitch" }.Int(1).Int(16383)) == std::vector<uint32_t>{ Cv(0xE0, 127, 127) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/channel_pressure" }.Int(1).Int(3)) == std::vector<uint32_t>{ Cv(0xD0, 3, 0) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/key_pressure" }.Int(1).Int(60).Int(3)) == std::vector<uint32_t>{ Cv(0xA0, 60, 3) });
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/rpn" }.Int(1).Int(0).Int(8192)) ==
        (std::vector<uint32_t>{ Cv(0xB0, 101, 0), Cv(0xB0, 100, 0), Cv(0xB0, 6, 64), Cv(0xB0, 38, 0) }));
    VERIFY_IS_TRUE(ReceiveWords(openStage, MessageBuilder{ "/nrpn" }.Int(1).Int(129).Int(1)) ==
        (std::vector<uint32_t>{ Cv(0xB0, 99, 1), Cv(0xB0, 98, 1), Cv(0xB0, 6, 0), Cv(0xB0, 38, 1) }));

    // Channel 0 isn't one when counting from 1.
    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable),
        static_cast<int32_t>(Receive(openStage, MessageBuilder{ "/note" }.Int(0).Int(60).Int(1), umps).Result));

    auto oscMidiOsc = Preset(OscPreset::OscMidiOsc, 2);
    oscMidiOsc.Channel = 3;
    std::vector<OscRow> const named{ oscMidiOsc };

    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/noteOn" }.Int(60).Int(100)) == std::vector<uint32_t>{ Cv(0x93, 60, 100, 2) });
    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/noteOff" }.Int(60)) == std::vector<uint32_t>{ Cv(0x83, 60, 0, 2) });
    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/cc" }.Int(1).Int(2)) == std::vector<uint32_t>{ Cv(0xB3, 1, 2, 2) });
    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/Aftertouch" }.Int(7)) == std::vector<uint32_t>{ Cv(0xD3, 7, 0, 2) });
    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/PolyAftertouch" }.Int(60).Int(7)) == std::vector<uint32_t>{ Cv(0xA3, 60, 7, 2) });
    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/Pitchbend" }.Int(0)) == std::vector<uint32_t>{ Cv(0xE3, 0, 0, 2) });
    VERIFY_IS_TRUE(ReceiveWords(named, MessageBuilder{ "/ProgramChange" }.Int(4)) == std::vector<uint32_t>{ Cv(0xC3, 4, 0, 2) });

    // Under a root of its own.
    auto rooted = Preset(OscPreset::OpenStageControl, 0);
    rooted.Root = "/osc/midi";
    VERIFY_IS_TRUE(ReceiveWords({ rooted }, MessageBuilder{ "/osc/midi/note" }.Int(1).Int(60).Int(100)) == std::vector<uint32_t>{ Cv(0x90, 60, 100) });
}

void OscTests::ValuesAreReadGenerously()
{
    auto unit = Preset(OscPreset::Paths, 0);
    unit.Values = OscValueKind::UnitFloats;

    // A TouchOSC fader left at 0 to 1, and a button sending true and false.
    VERIFY_IS_TRUE(ReceiveWords({ unit }, MessageBuilder{ "/midi/1/cc/7" }.Float(0.5f)) == std::vector<uint32_t>{ Cv(0xB0, 7, 64) });
    VERIFY_IS_TRUE(ReceiveWords({ unit }, MessageBuilder{ "/midi/1/cc/7" }.Float(1.0f)) == std::vector<uint32_t>{ Cv(0xB0, 7, 127) });
    VERIFY_IS_TRUE(ReceiveWords({ unit }, MessageBuilder{ "/midi/1/note/60" }.Bool(true)) == std::vector<uint32_t>{ Cv(0x90, 60, 127) });
    VERIFY_IS_TRUE(ReceiveWords({ unit }, MessageBuilder{ "/midi/1/note/60" }.Bool(false)) == std::vector<uint32_t>{ Cv(0x80, 60, 0) });
    VERIFY_IS_TRUE(ReceiveWords({ unit }, MessageBuilder{ "/midi/1/pitch" }.Float(0.5f)) == std::vector<uint32_t>{ Cv(0xE0, 0, 64) });

    // Integers are taken as they are, even on a 0 to 1 row.
    VERIFY_IS_TRUE(ReceiveWords({ unit }, MessageBuilder{ "/midi/1/cc/7" }.Int(64)) == std::vector<uint32_t>{ Cv(0xB0, 7, 64) });

    // A row of integers rounds floats and holds values to the range.
    auto const integers = Preset(OscPreset::Paths, 0);
    VERIFY_IS_TRUE(ReceiveWords({ integers }, MessageBuilder{ "/midi/1/cc/7" }.Float(64.4f)) == std::vector<uint32_t>{ Cv(0xB0, 7, 64) });
    VERIFY_IS_TRUE(ReceiveWords({ integers }, MessageBuilder{ "/midi/1/cc/7" }.Int(300)) == std::vector<uint32_t>{ Cv(0xB0, 7, 127) });
    VERIFY_IS_TRUE(ReceiveWords({ integers }, MessageBuilder{ "/midi/1/cc/7" }.Int(-5)) == std::vector<uint32_t>{ Cv(0xB0, 7, 0) });

    // Pitch bend centered on 0.
    auto centered = Preset(OscPreset::Paths, 0);
    centered.PitchBend = OscPitchBendRange::Signed;
    VERIFY_IS_TRUE(ReceiveWords({ centered }, MessageBuilder{ "/midi/1/pitch" }.Int(0)) == std::vector<uint32_t>{ Cv(0xE0, 0, 64) });
    VERIFY_IS_TRUE(ReceiveWords({ centered }, MessageBuilder{ "/midi/1/pitch" }.Int(-8192)) == std::vector<uint32_t>{ Cv(0xE0, 0, 0) });

    // A string isn't a value.
    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable),
        static_cast<int32_t>(Receive({ integers }, MessageBuilder{ "/midi/1/cc/7" }.String("64"), umps).Result));

    // Nor is nothing at all.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable),
        static_cast<int32_t>(Receive({ integers }, MessageBuilder{ "/midi/1/cc/7" }, umps).Result));
}

void OscTests::WildcardsExpandAndAreCapped()
{
    std::vector<OscRow> const rows{ Preset(OscPreset::ReaperVirtualKeyboard, 0) };

    auto const all = ReceiveWords(rows, MessageBuilder{ "/vkb_midi/*/cc/7" }.Int(64));
    VERIFY_ARE_EQUAL(size_t{ 16 }, all.size());

    for (uint8_t channel = 0; channel < 16; channel++)
    {
        VERIFY_IS_TRUE(std::find(all.begin(), all.end(), Cv(static_cast<uint8_t>(0xB0 | channel), 7, 64)) != all.end());
    }

    auto const some = ReceiveWords(rows, MessageBuilder{ "/vkb_midi/{0,3}/note/[6][0-1]" }.Int(100));
    VERIFY_ARE_EQUAL(size_t{ 4 }, some.size());

    auto const across = ReceiveWords(rows, MessageBuilder{ "//cc/7" }.Int(1));
    VERIFY_ARE_EQUAL(size_t{ 16 }, across.size());

    // 2,048 notes asked for, 128 sent.
    UmpCollector umps{};
    auto const capped = Receive(rows, MessageBuilder{ "/vkb_midi/*/note/*" }.Int(1), umps);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::TooManyMessages), static_cast<int32_t>(capped.Result));
    VERIFY_ARE_EQUAL(MaximumMessagesFromOneOscMessage, umps.Umps.size());
}

void OscTests::UnknownAddressesMatchNoRow()
{
    std::vector<OscRow> const rows{ Preset(OscPreset::ReaperVirtualKeyboard, 0), Preset(OscPreset::MidiType, 1) };

    UmpCollector umps{};

    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/foo" }.Int(1), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/vkb_midi/0/bogus/1" }.Int(1), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/vkb_midix/0/note/1" }.Int(1), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/1/fader3" }.Float(0.5f), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/vkb_midi/0/note/060" }.Int(1), umps).Result));

    // The address is a row's, but note 128 isn't a note.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/vkb_midi/0/note/128" }.Int(1), umps).Result));

    VERIFY_ARE_EQUAL(size_t{ 0 }, umps.Umps.size());
}

void OscTests::SysExIsReadEveryWay()
{
    std::vector<OscRow> const rows{ Preset(OscPreset::Paths, 4) };
    std::vector<uint32_t> const identityRequest{ 0x34047E7Fu, 0x06010000u };

    auto const read = [&](std::vector<OscRow> const& r, MessageBuilder const& builder)
        {
            UmpCollector umps{};
            auto const outcome = Receive(r, builder, umps);
            VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Translated), static_cast<int32_t>(outcome.Result));
            VERIFY_ARE_EQUAL(uint32_t{ 1 }, outcome.Messages);
            return umps.Umps;
        };

    auto const blob = read(rows, MessageBuilder{ "/midi/sysex" }.Blob(Bytes({ 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 })));
    VERIFY_ARE_EQUAL(size_t{ 1 }, blob.size());
    VERIFY_IS_TRUE(blob[0] == identityRequest);

    // Without F0 and F7.
    VERIFY_IS_TRUE(read(rows, MessageBuilder{ "/midi/sysex" }.Blob(Bytes({ 0x7E, 0x7F, 0x06, 0x01 })))[0] == identityRequest);

    // As numbers.
    VERIFY_IS_TRUE(read(rows, MessageBuilder{ "/midi/sysex" }.Int(0x7E).Int(0x7F).Int(6).Int(1))[0] == identityRequest);

    // As Open Stage Control's hex string, with and without spaces.
    auto openStage = Preset(OscPreset::OpenStageControl, 4);
    VERIFY_IS_TRUE(read({ openStage }, MessageBuilder{ "/sysex" }.String("f0 7e 7f 06 01 f7"))[0] == identityRequest);
    VERIFY_IS_TRUE(read({ openStage }, MessageBuilder{ "/sysex" }.String("F07E7F0601F7"))[0] == identityRequest);

    // Eight data bytes take two packets: a start with six and an end with two.
    auto const longer = read(rows, MessageBuilder{ "/midi/sysex" }.Blob(Bytes({ 0xF0, 1, 2, 3, 4, 5, 6, 7, 8, 0xF7 })));
    VERIFY_ARE_EQUAL(size_t{ 2 }, longer.size());
    VERIFY_ARE_EQUAL(0x34160102u, longer[0][0]);
    VERIFY_ARE_EQUAL(0x03040506u, longer[0][1]);
    VERIFY_ARE_EQUAL(0x34320708u, longer[1][0]);

    // An empty one is one packet with nothing in it.
    VERIFY_ARE_EQUAL(0x34000000u, read(rows, MessageBuilder{ "/midi/sysex" }.Blob(Bytes({ 0xF0, 0xF7 })))[0][0]);

    // A data byte with its top bit set isn't SysEx.
    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable),
        static_cast<int32_t>(Receive(rows, MessageBuilder{ "/midi/sysex" }.Blob(Bytes({ 0xF0, 0x90, 0xF7 })), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable),
        static_cast<int32_t>(Receive({ openStage }, MessageBuilder{ "/sysex" }.String("f0 zz f7"), umps).Result));
}

void OscTests::EveryPresetRoundTripsEveryMessage()
{
    std::vector<uint32_t> messages{};

    for (uint8_t channel = 0; channel < 16; channel++)
    {
        messages.push_back(Cv(static_cast<uint8_t>(0x90 | channel), 60, 100, 2));
        messages.push_back(Cv(static_cast<uint8_t>(0x80 | channel), 61, 0, 2));
        messages.push_back(Cv(static_cast<uint8_t>(0xA0 | channel), 62, 33, 2));
        messages.push_back(Cv(static_cast<uint8_t>(0xB0 | channel), 74, 127, 2));
        messages.push_back(Cv(static_cast<uint8_t>(0xC0 | channel), 12, 0, 2));
        messages.push_back(Cv(static_cast<uint8_t>(0xD0 | channel), 99, 0, 2));

        for (uint32_t const bend : { 0u, 1234u, 8192u, 16383u })
        {
            messages.push_back(Cv(static_cast<uint8_t>(0xE0 | channel), static_cast<uint8_t>(bend & 0x7F), static_cast<uint8_t>(bend >> 7), 2));
        }
    }

    for (uint32_t const word : {
        Sys(0xF1, 0x35, 0, 2), Sys(0xF2, 0x10, 0x20, 2), Sys(0xF3, 9, 0, 2), Sys(0xF6, 0, 0, 2),
        Sys(0xF8, 0, 0, 2), Sys(0xFA, 0, 0, 2), Sys(0xFB, 0, 0, 2), Sys(0xFC, 0, 0, 2), Sys(0xFE, 0, 0, 2), Sys(0xFF, 0, 0, 2) })
    {
        messages.push_back(word);
    }

    for (int32_t preset = 0; preset < OscPresetCount; preset++)
    {
        // Custom rules only carry the messages they name: their own tests cover them.
        if (static_cast<OscPreset>(preset) == OscPreset::TouchOsc)
        {
            continue;
        }

        for (auto const values : { OscValueKind::Integers, OscValueKind::Floats, OscValueKind::UnitFloats })
        {
            auto row = OscRowFromPreset(static_cast<OscPreset>(preset), 2);
            row.Values = values;
            row.SendClock = true;

            std::vector<OscRow> const rows{ row };
            OutboundTranslator translator{};

            for (auto const word : messages)
            {
                std::vector<MessageBuilder> out{};
                VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(translator.Translate(rows, &word, 1, out)));
                VERIFY_ARE_EQUAL(size_t{ 1 }, out.size());

                auto expected = word;

                // OSC_MIDI_OSC has no channel: what comes back is on the row's channel.
                if (row.Format == OscFormat::Arguments && row.Names == OscArgumentNames::OscMidiOsc && (word >> 28) == 0x2)
                {
                    expected = (word & 0xFFF0FFFFu) | (static_cast<uint32_t>(row.Channel) << 16);
                }

                auto const back = ReceiveWords(rows, out[0]);

                if (back.size() != 1 || back[0] != expected)
                {
                    Log::Error(String().Format(L"Preset %d, values %d: %08X came back as %08X",
                        preset, static_cast<int32_t>(values), word, back.empty() ? 0u : back[0]));
                }

                VERIFY_ARE_EQUAL(size_t{ 1 }, back.size());
                VERIFY_ARE_EQUAL(expected, back[0]);
            }
        }
    }
}

void OscTests::MessagesGoOutAsTheRowSays()
{
    auto const send = [](OscRow const& row, uint32_t word)
        {
            OutboundTranslator translator{};
            std::vector<MessageBuilder> out{};
            VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(translator.Translate({ &row, 1 }, &word, 1, out)));
            VERIFY_ARE_EQUAL(size_t{ 1 }, out.size());
            return Reread(out[0]);
        };

    // REAPER's virtual keyboard: channels from 0, integers, velocity 0 for note off.
    auto const reaper = Preset(OscPreset::ReaperVirtualKeyboard, 0);

    auto message = send(reaper, Cv(0x90, 60, 100));
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/0/note/60" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "i" }, message.TypeTags);
    VERIFY_ARE_EQUAL(int64_t{ 100 }, message.Arguments[0].Integer);

    message = send(reaper, Cv(0x85, 60, 64));
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/5/note/60" }, message.Address);
    VERIFY_ARE_EQUAL(int64_t{ 0 }, message.Arguments[0].Integer);

    message = send(reaper, Cv(0xE0, 0, 64));
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/0/pitch" }, message.Address);
    VERIFY_ARE_EQUAL(int64_t{ 8192 }, message.Arguments[0].Integer);

    message = send(reaper, Sys(0xFA, 0, 0));
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/start" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "" }, message.TypeTags);

    // Channels from 1 and a note off of its own.
    auto paths = Preset(OscPreset::Paths, 0);
    paths.NoteOff = OscNoteOff::OwnAddress;

    message = send(paths, Cv(0x80, 60, 64));
    VERIFY_ARE_EQUAL(std::string{ "/midi/1/noteoff/60" }, message.Address);
    VERIFY_ARE_EQUAL(int64_t{ 64 }, message.Arguments[0].Integer);

    message = send(paths, Cv(0x9F, 60, 0));
    VERIFY_ARE_EQUAL(std::string{ "/midi/16/noteoff/60" }, message.Address);

    // 0 to 1, and pitch bend centered on 0.
    paths.Values = OscValueKind::UnitFloats;
    message = send(paths, Cv(0xB0, 7, 127));
    VERIFY_ARE_EQUAL(std::string{ "f" }, message.TypeTags);
    VERIFY_ARE_EQUAL(1.0, message.Arguments[0].Real);

    paths.Values = OscValueKind::Integers;
    paths.PitchBend = OscPitchBendRange::Signed;
    message = send(paths, Cv(0xE0, 0, 0));
    VERIFY_ARE_EQUAL(int64_t{ -8192 }, message.Arguments[0].Integer);

    paths.Values = OscValueKind::Floats;
    message = send(paths, Cv(0xE0, 127, 127));
    VERIFY_ARE_EQUAL(std::string{ "f" }, message.TypeTags);
    VERIFY_ARE_EQUAL(8191.0, message.Arguments[0].Real);

    // The MIDI type, with the row's group as the port.
    message = send(Preset(OscPreset::MidiType, 0), Cv(0x93, 60, 100));
    VERIFY_ARE_EQUAL(std::string{ "/midi" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "m" }, message.TypeTags);
    VERIFY_ARE_EQUAL(int64_t{ 0x00933C64 }, message.Arguments[0].Integer);

    auto onPort = Preset(OscPreset::MidiType, 6);
    auto const onGroup6 = Cv(0x93, 60, 100, 6);
    message = send(onPort, onGroup6);
    VERIFY_ARE_EQUAL(int64_t{ 0x06933C64 }, message.Arguments[0].Integer);

    // Open Stage Control: channel first, from 1.
    message = send(Preset(OscPreset::OpenStageControl, 0), Cv(0xB1, 7, 64));
    VERIFY_ARE_EQUAL(std::string{ "/control" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "iii" }, message.TypeTags);
    VERIFY_ARE_EQUAL(int64_t{ 2 }, message.Arguments[0].Integer);
    VERIFY_ARE_EQUAL(int64_t{ 7 }, message.Arguments[1].Integer);
    VERIFY_ARE_EQUAL(int64_t{ 64 }, message.Arguments[2].Integer);

    // OSC_MIDI_OSC: no channel, and the note off carries only the note.
    message = send(Preset(OscPreset::OscMidiOsc, 0), Cv(0x84, 60, 10));
    VERIFY_ARE_EQUAL(std::string{ "/noteOff" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "i" }, message.TypeTags);
    VERIFY_ARE_EQUAL(int64_t{ 60 }, message.Arguments[0].Integer);
}

void OscTests::Midi2IsTranslatedTheSpecsWay()
{
    std::array<Midi1Message, 4> out{};

    auto const midi2 = [](uint8_t opcode, uint8_t channel, uint8_t index, uint8_t extra, uint32_t data)
        {
            return std::array<uint32_t, 2>{ (0x4u << 28) | (static_cast<uint32_t>(opcode) << 20) | (static_cast<uint32_t>(channel) << 16) |
                (static_cast<uint32_t>(index) << 8) | extra, data };
        };

    // A note on's velocity loses its low 9 bits, and is never 0.
    auto words = midi2(0x9, 3, 60, 0, 0x0000'0000u);
    VERIFY_ARE_EQUAL(size_t{ 1 }, Midi1FromMidi2(words.data(), out.data()));
    VERIFY_ARE_EQUAL(uint8_t{ 0x93 }, out[0].Status);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, out[0].Data1);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, out[0].Data2);

    words = midi2(0x9, 0, 60, 0, 0xFFFF'0000u);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, out[0].Data2);

    words = midi2(0x9, 0, 60, 0, 0x8000'0000u);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, out[0].Data2);

    // A note off keeps velocity 0.
    words = midi2(0x8, 0, 60, 0, 0x0000'0000u);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_ARE_EQUAL(uint8_t{ 0x80 }, out[0].Status);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, out[0].Data2);

    // 32-bit values keep their top 7 bits; pitch bend its top 14.
    words = midi2(0xB, 1, 7, 0, 0xFFFF'FFFFu);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_ARE_EQUAL(uint8_t{ 0xB1 }, out[0].Status);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, out[0].Data2);

    words = midi2(0xE, 0, 0, 0, 0x8000'0000u);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, out[0].Data1);
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, out[0].Data2);

    words = midi2(0xD, 0, 0, 0, 0x4000'0000u);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_ARE_EQUAL(uint8_t{ 0xD0 }, out[0].Status);
    VERIFY_ARE_EQUAL(uint8_t{ 32 }, out[0].Data1);

    // Program change: bank select MSB, then LSB, then the program, when the bank is valid.
    words = midi2(0xC, 2, 0, 1, (5u << 24) | (3u << 8) | 9u);
    VERIFY_ARE_EQUAL(size_t{ 3 }, Midi1FromMidi2(words.data(), out.data()));
    VERIFY_IS_TRUE(out[0].Status == 0xB2 && out[0].Data1 == 0 && out[0].Data2 == 3);
    VERIFY_IS_TRUE(out[1].Status == 0xB2 && out[1].Data1 == 32 && out[1].Data2 == 9);
    VERIFY_IS_TRUE(out[2].Status == 0xC2 && out[2].Data1 == 5);

    words = midi2(0xC, 2, 0, 0, (5u << 24) | (3u << 8) | 9u);
    VERIFY_ARE_EQUAL(size_t{ 1 }, Midi1FromMidi2(words.data(), out.data()));

    // Registered and assignable controllers become four control changes.
    words = midi2(0x2, 0, 0, 1, 0x8000'0000u);
    VERIFY_ARE_EQUAL(size_t{ 4 }, Midi1FromMidi2(words.data(), out.data()));
    VERIFY_IS_TRUE(out[0].Data1 == 101 && out[0].Data2 == 0);
    VERIFY_IS_TRUE(out[1].Data1 == 100 && out[1].Data2 == 1);
    VERIFY_IS_TRUE(out[2].Data1 == 6 && out[2].Data2 == 64);
    VERIFY_IS_TRUE(out[3].Data1 == 38 && out[3].Data2 == 0);

    words = midi2(0x3, 0, 2, 3, 0xFFFF'FFFFu);
    Midi1FromMidi2(words.data(), out.data());
    VERIFY_IS_TRUE(out[0].Data1 == 99 && out[0].Data2 == 2);
    VERIFY_IS_TRUE(out[1].Data1 == 98 && out[1].Data2 == 3);
    VERIFY_IS_TRUE(out[3].Data1 == 38 && out[3].Data2 == 127);

    // Per-note controllers, per-note pitch bend, relative controllers and per-note management
    // have no MIDI 1.0 form.
    for (uint8_t const opcode : { uint8_t{ 0x0 }, uint8_t{ 0x1 }, uint8_t{ 0x4 }, uint8_t{ 0x5 }, uint8_t{ 0x6 }, uint8_t{ 0xF } })
    {
        words = midi2(opcode, 0, 60, 0, 0x8000'0000u);
        VERIFY_ARE_EQUAL(size_t{ 0 }, Midi1FromMidi2(words.data(), out.data()));
    }

    // Through the translator: a program change with a bank is three OSC messages, and a per-note
    // pitch bend isn't sendable.
    auto const row = Preset(OscPreset::ReaperVirtualKeyboard, 0);
    OutboundTranslator translator{};
    std::vector<MessageBuilder> sent{};

    words = midi2(0xC, 0, 0, 1, (5u << 24) | (3u << 8) | 9u);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(translator.Translate({ &row, 1 }, words.data(), 2, sent)));
    VERIFY_ARE_EQUAL(size_t{ 3 }, sent.size());
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/0/cc/0" }, sent[0].Address());
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/0/cc/32" }, sent[1].Address());
    VERIFY_ARE_EQUAL(std::string{ "/vkb_midi/0/program" }, sent[2].Address());

    words = midi2(0x6, 0, 60, 0, 0x8000'0000u);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::NotSendable), static_cast<int32_t>(translator.Translate({ &row, 1 }, words.data(), 2, sent)));
}

void OscTests::SysExIsCollectedAcrossPackets()
{
    auto const row = Preset(OscPreset::Paths, 1);
    OutboundTranslator translator{};
    std::vector<MessageBuilder> out{};

    auto const sysex = [](uint8_t status, std::initializer_list<uint8_t> data, uint8_t group = 1)
        {
            std::array<uint8_t, 6> bytes{};
            std::copy(data.begin(), data.end(), bytes.begin());

            return std::array<uint32_t, 2>{
                (0x3u << 28) | (static_cast<uint32_t>(group) << 24) | (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(data.size()) << 16) |
                    (static_cast<uint32_t>(bytes[0]) << 8) | bytes[1],
                (static_cast<uint32_t>(bytes[2]) << 24) | (static_cast<uint32_t>(bytes[3]) << 16) | (static_cast<uint32_t>(bytes[4]) << 8) | bytes[5] };
        };

    auto const send = [&](std::array<uint32_t, 2> const& words)
        {
            return translator.Translate({ &row, 1 }, words.data(), 2, out);
        };

    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Waiting), static_cast<int32_t>(send(sysex(1, { 1, 2, 3, 4, 5, 6 }))));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Waiting), static_cast<int32_t>(send(sysex(2, { 7, 8, 9, 10, 11, 12 }))));

    // Another group's SysEx in the middle doesn't disturb this one, and a row-less group's is
    // simply not sent.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::NoRow), static_cast<int32_t>(send(sysex(0, { 1 }, 5))));

    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(send(sysex(3, { 13 }))));
    VERIFY_ARE_EQUAL(size_t{ 1 }, out.size());

    auto const message = Reread(out[0]);
    VERIFY_ARE_EQUAL(std::string{ "/midi/sysex" }, message.Address);
    VERIFY_ARE_EQUAL(std::string{ "b" }, message.TypeTags);
    VERIFY_IS_TRUE(message.Arguments[0].Bytes == Bytes({ 0xF0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0xF7 }));

    // Open Stage Control takes a hex string.
    auto const openStage = Preset(OscPreset::OpenStageControl, 1);
    out.clear();
    auto const complete = sysex(0, { 0x7E, 0x7F, 6, 1 });
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(translator.Translate({ &openStage, 1 }, complete.data(), 2, out)));
    auto const hex = Reread(out[0]);
    VERIFY_ARE_EQUAL(std::string{ "/sysex" }, hex.Address);
    VERIFY_ARE_EQUAL(std::string{ "f0 7e 7f 06 01 f7" }, hex.Arguments[0].Text);

    // A continue or an end with no start is not sent.
    out.clear();
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::SysExNotSent), static_cast<int32_t>(send(sysex(3, { 1 }))));
    VERIFY_ARE_EQUAL(size_t{ 0 }, out.size());

    // A start before the last one ended: the first is lost, the second still goes.
    send(sysex(1, { 1, 2, 3, 4, 5, 6 }));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::SysExNotSent), static_cast<int32_t>(send(sysex(1, { 9, 9, 9, 9, 9, 9 }))));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(send(sysex(3, { 8 }))));
    VERIFY_IS_TRUE(Reread(out.back()).Arguments[0].Bytes == Bytes({ 0xF0, 9, 9, 9, 9, 9, 9, 8, 0xF7 }));

    // Past 32 KB, nothing is sent, and the next one is fine again.
    out.clear();
    send(sysex(1, { 1, 2, 3, 4, 5, 6 }));

    for (size_t i = 0; i < MaximumSysExBytes / 6; i++)
    {
        send(sysex(2, { 1, 2, 3, 4, 5, 6 }));
    }

    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::SysExNotSent), static_cast<int32_t>(send(sysex(3, { 1 }))));
    VERIFY_ARE_EQUAL(size_t{ 0 }, out.size());
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), static_cast<int32_t>(send(sysex(0, { 1 }))));
}

void OscTests::RowSettingsLeaveMessagesOut()
{
    auto row = Preset(OscPreset::Paths, 0);
    OutboundTranslator translator{};
    std::vector<MessageBuilder> out{};

    auto const result = [&](std::vector<uint32_t> const& words)
        {
            return static_cast<int32_t>(translator.Translate({ &row, 1 }, words.data(), static_cast<uint8_t>(words.size()), out));
        };

    // Clock and active sensing are off unless asked for. Start isn't.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::LeftOut), result({ Sys(0xF8, 0, 0) }));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::LeftOut), result({ Sys(0xFE, 0, 0) }));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), result({ Sys(0xFA, 0, 0) }));

    row.SendClock = true;
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::Sent), result({ Sys(0xF8, 0, 0) }));

    row.SendSysEx = false;
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::LeftOut), result({ 0x30020102u, 0u }));

    // No row on group 4, and messages without a group, or with no MIDI 1.0 form.
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::NoRow), result({ Cv(0x90, 60, 1, 4) }));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::NotSendable), result({ 0x00000000u }));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::NotSendable), result({ 0xF0000000u, 0u, 0u, 0u }));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::NotSendable), result({ 0xD0000000u, 0u, 0u, 0u }));
}

void OscTests::UnitPitchBendIsCenteredExactly()
{
    VERIFY_ARE_EQUAL(0.0f, UnitFromPitchBend(0));
    VERIFY_ARE_EQUAL(0.5f, UnitFromPitchBend(8192));
    VERIFY_ARE_EQUAL(1.0f, UnitFromPitchBend(16383));

    VERIFY_ARE_EQUAL(uint16_t{ 8192 }, PitchBendFromUnit(0.5));
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, PitchBendFromUnit(-3.0));
    VERIFY_ARE_EQUAL(uint16_t{ 16383 }, PitchBendFromUnit(7.0));

    for (uint16_t value = 0; value <= 16383; value++)
    {
        VERIFY_ARE_EQUAL(value, PitchBendFromUnit(UnitFromPitchBend(value)));
    }
}

void OscTests::ConnectionsSurviveTheFile()
{
    OscConnectionSettings settings{};
    settings.ReceivePort = 9100;
    settings.ListenOn = OscListenOn::ThisPc;
    settings.AcceptFrom = OscAcceptFrom::List;
    settings.AcceptAddresses = { L"192.168.1.20", L"ipad.local" };
    settings.SendHost = L"petes-ipad.local";
    settings.SendPort = 9200;
    settings.SendToLastSender = true;
    settings.ReceivedTimeTags = OscReceivedTimeTags::KeepSenderSpacing;
    settings.SpacingDelayMilliseconds = 25;
    settings.SendTiming = OscSendTiming::EarlyWithTimeTags;
    settings.BundleTogether = true;
    settings.ReceiveRateLimit = 1234;

    auto row = OscRowFromPreset(OscPreset::OscMidiOsc, 7);
    row.Root = "/my/synth";
    row.ChannelsFromZero = true;
    row.Values = OscValueKind::UnitFloats;
    row.PitchBend = OscPitchBendRange::Signed;
    row.SendSysEx = false;
    row.SendClock = true;
    row.Channel = 11;

    settings.Rows = { OscRowFromPreset(OscPreset::ReaperVirtualKeyboard, 0), OscRowFromPreset(OscPreset::MidiType, 3), row };

    auto const text = OscConnectionToJson(settings).Stringify();
    auto const back = OscConnectionFromJson(json::JsonObject::Parse(text));

    VERIFY_ARE_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));
    VERIFY_ARE_EQUAL(size_t{ 3 }, back.Rows.size());
    VERIFY_IS_TRUE(OscPresetOf(back.Rows[0]) == OscPreset::ReaperVirtualKeyboard);
    VERIFY_IS_TRUE(OscPresetOf(back.Rows[1]) == OscPreset::MidiType);
    VERIFY_IS_FALSE(OscPresetOf(back.Rows[2]).has_value());

    // Every preset is itself, and makes a connection that can do something.
    for (int32_t preset = 0; preset < OscPresetCount; preset++)
    {
        VERIFY_IS_TRUE(OscPresetOf(OscRowFromPreset(static_cast<OscPreset>(preset), 0)) == static_cast<OscPreset>(preset));

        auto const fresh = OscConnectionFromPreset(static_cast<OscPreset>(preset));
        VERIFY_ARE_EQUAL(size_t{ 1 }, fresh.Rows.size());
        VERIFY_IS_TRUE(OscConnectionReceives(fresh) || fresh.SendPort != 0);

        auto const freshBack = OscConnectionFromJson(json::JsonObject::Parse(OscConnectionToJson(fresh).Stringify()));
        VERIFY_ARE_EQUAL(OscConnectionSignature(fresh), OscConnectionSignature(freshBack));
    }
}

void OscTests::BadConnectionSettingsAreRepaired()
{
    std::wstring text = LR"({
        "receive": { "port": 70000, "listenOn": "everywhere", "messagesPerSecond": 0, "acceptAddresses": [ "a b", 5, "ok" ] },
        "send": { "host": "\u0001host\u0007", "port": -1 },
        "receivedTimeTags": 3,
        "spacingDelayMilliseconds": 99999,
        "rows": [
            { "group": 0, "root": "/a", "format": "bogus" },
            { "group": 0, "root": "/duplicate" },
            { "group": 16, "root": "/too/high" },
            { "group": 1, "root": "/has space" },
            { "group": 2, "root": "/caf\u00e9" },
            { "group": 3, "root": "", "format": "midiType" },
            { "group": 4.5, "root": "/half" },
            { "group": 5, "root": "/ok", "channel": 99, "values": "zeroToOne" })";

    for (int i = 6; i < 40; i++)
    {
        text += L", { \"group\": " + std::to_wstring(i % 16) + L", \"root\": \"/r" + std::to_wstring(i) + L"\" }";
    }

    text += L"] }";

    auto const settings = OscConnectionFromJson(json::JsonObject::Parse(text));

    VERIFY_ARE_EQUAL(uint16_t{ 0 }, settings.ReceivePort);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OscListenOn::EveryNetwork), static_cast<int32_t>(settings.ListenOn));
    VERIFY_ARE_EQUAL(DefaultOscReceiveRateLimit, settings.ReceiveRateLimit);
    VERIFY_ARE_EQUAL(DefaultOscSpacingDelayMilliseconds, settings.SpacingDelayMilliseconds);
    VERIFY_ARE_EQUAL(std::wstring{ L"host" }, settings.SendHost);
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, settings.SendPort);
    VERIFY_ARE_EQUAL(size_t{ 2 }, settings.AcceptAddresses.size());

    // At most sixteen rows, one per group, in group order, each with a root OSC allows.
    VERIFY_IS_TRUE(settings.Rows.size() <= MaximumOscRows);

    for (size_t i = 0; i < settings.Rows.size(); i++)
    {
        VERIFY_IS_TRUE(IsValidRoot(settings.Rows[i].Root));
        VERIFY_IS_TRUE(settings.Rows[i].Group < 16);
        VERIFY_IS_TRUE(settings.Rows[i].Channel < 16);

        if (i > 0)
        {
            VERIFY_IS_TRUE(settings.Rows[i - 1].Group < settings.Rows[i].Group);
        }
    }

    auto const first = OscRowOnGroup(settings, 0);
    VERIFY_IS_NOT_NULL(first);
    VERIFY_ARE_EQUAL(std::string{ "/a" }, first->Root);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OscFormat::Paths), static_cast<int32_t>(first->Format));

    // A row whose root OSC doesn't allow is dropped, so a later good row on its group takes it.
    VERIFY_ARE_EQUAL(std::string{ "/r17" }, OscRowOnGroup(settings, 1)->Root);
    VERIFY_ARE_EQUAL(std::string{ "/r18" }, OscRowOnGroup(settings, 2)->Root);

    // The MIDI type can't live at the top level.
    auto const midiType = OscRowOnGroup(settings, 3);
    VERIFY_IS_NOT_NULL(midiType);
    VERIFY_ARE_EQUAL(std::string{ "/midi" }, midiType->Root);

    auto const ok = OscRowOnGroup(settings, 5);
    VERIFY_IS_NOT_NULL(ok);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, ok->Channel);
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OscValueKind::UnitFloats), static_cast<int32_t>(ok->Values));
}

void OscTests::RowsAreSuggestedFromWhatArrives()
{
    auto const suggest = [](MessageBuilder const& builder)
        {
            auto const packet = MakePacket(builder);
            MessageView view{};
            ReadMessage(packet.data(), packet.size(), view);
            return SuggestRow(view, 2);
        };

    auto row = suggest(MessageBuilder{ "/vkb_midi/0/note/60" }.Int(100));
    VERIFY_IS_TRUE(row.has_value() && OscPresetOf(*row) == OscPreset::ReaperVirtualKeyboard && row->Group == 2);

    row = suggest(MessageBuilder{ "/synth2/1/cc/7" }.Int(1));
    VERIFY_IS_TRUE(row.has_value() && row->Format == OscFormat::Paths && row->Root == "/synth2" && !row->ChannelsFromZero);

    row = suggest(MessageBuilder{ "/midi/port" }.Midi(3, 0x90, 60, 1));
    VERIFY_IS_TRUE(row.has_value() && row->Format == OscFormat::MidiType && row->Root == "/midi/port" && row->Group == 3);

    row = suggest(MessageBuilder{ "/note" }.Int(1).Int(60).Int(100));
    VERIFY_IS_TRUE(row.has_value() && OscPresetOf(*row) == OscPreset::OpenStageControl);

    row = suggest(MessageBuilder{ "/noteOn" }.Int(60).Int(100));
    VERIFY_IS_TRUE(row.has_value() && OscPresetOf(*row) == OscPreset::OscMidiOsc);

    VERIFY_IS_FALSE(suggest(MessageBuilder{ "/1/fader3" }.Float(0.5f)).has_value());
    VERIFY_IS_FALSE(suggest(MessageBuilder{ "/a/*/note/1" }.Int(1)).has_value());

    // What the inspector shows for each argument.
    auto const packet = MakePacket(MessageBuilder{ "/x" }.Int(60).Float(0.5f).String("hi").Midi(1, 0x90, 60, 100).Bool(true));
    MessageView view{};
    ReadMessage(packet.data(), packet.size(), view);
    VERIFY_ARE_EQUAL(std::wstring{ L"i 60, f 0.5, s hi, m 01 90 3C 64, T" }, ArgumentsText(view));
}

void OscTests::OscConnectionsSurviveThePatchFile()
{
    PatchDocument patch{};
    patch.Name = L"OSC";

    PatchEndpoint device{};
    device.Id = L"d";
    device.DisplayName = L"Synth";
    device.Match.EndpointDeviceId = L"\\\\?\\swd#midisrv#synth";
    patch.Endpoints.push_back(device);

    // Without an OSC connection, the file stays at version 2 for earlier versions of the app.
    auto const plain = WritePatchJson(patch);
    VERIFY_IS_TRUE(plain.find(L"\"fileVersion\":2") != std::wstring::npos);
    VERIFY_IS_TRUE(plain.find(L"oscConnections") == std::wstring::npos);

    PatchEndpoint osc{};
    osc.Id = L"o";
    osc.DisplayName = L"iPad TouchOSC";
    osc.CanvasX = 40;
    osc.CanvasY = 80;
    osc.Osc = OscConnectionFromPreset(OscPreset::Paths);
    osc.Osc->Rows[0].Group = 3;
    osc.Osc->SendHost = L"petes-ipad.local";
    patch.Endpoints.push_back(osc);

    patch.Connections.push_back(PatchConnection{ L"c1", L"o", 3, L"d", AllGroups, false });
    patch.Connections.push_back(PatchConnection{ L"c2", L"d", AllGroups, L"o", 3, false });

    auto const text = WritePatchJson(patch);
    VERIFY_IS_TRUE(text.find(L"\"fileVersion\":3") != std::wstring::npos);

    auto const back = ReadPatchJson(text, L"fallback");
    VERIFY_IS_TRUE(back.has_value());
    VERIFY_IS_FALSE(back->IsFromNewerVersion);
    VERIFY_ARE_EQUAL(OscPatchFileVersion, back->LoadedFileVersion);
    VERIFY_ARE_EQUAL(size_t{ 2 }, back->Endpoints.size());

    auto const* oscBack = back->FindEndpoint(L"o");
    VERIFY_IS_NOT_NULL(oscBack);
    VERIFY_IS_TRUE(oscBack->IsOsc());
    VERIFY_ARE_EQUAL(std::wstring{ L"iPad TouchOSC" }, oscBack->DisplayName);
    VERIFY_ARE_EQUAL(40.0, oscBack->CanvasX);
    VERIFY_ARE_EQUAL(OscConnectionSignature(*osc.Osc), OscConnectionSignature(*oscBack->Osc));
    VERIFY_IS_FALSE(back->FindEndpoint(L"d")->IsOsc());

    VERIFY_ARE_EQUAL(size_t{ 2 }, back->Connections.size());
    VERIFY_ARE_EQUAL(3, back->FindConnection(L"c1")->SourceGroupIndex);
    VERIFY_ARE_EQUAL(3, back->FindConnection(L"c2")->DestinationGroupIndex);

    // A later version's file is read, and never saved.
    auto newer = text;
    newer.replace(newer.find(L"\"fileVersion\":3"), 15, L"\"fileVersion\":4");
    VERIFY_IS_TRUE(ReadPatchJson(newer, L"fallback")->IsFromNewerVersion);

    // An OSC connection can't share an id with a device, and a patch holds at most sixteen.
    std::wstring crowded = LR"({ "fileVersion": 3, "endpoints": [ { "id": "a", "match": {} } ], "oscConnections": [ { "id": "a", "rows": [] })";

    for (int i = 0; i < 20; i++)
    {
        crowded += L", { \"id\": \"o" + std::to_wstring(i) + L"\", \"rows\": [ { \"group\": 0, \"root\": \"/x\" } ] }";
    }

    crowded += L"] }";

    auto const limited = ReadPatchJson(crowded, L"fallback");
    VERIFY_IS_TRUE(limited.has_value());
    VERIFY_IS_FALSE(limited->FindEndpoint(L"a")->IsOsc());
    VERIFY_ARE_EQUAL(MaximumOscConnectionsPerPatch, limited->OscConnectionCount());
}

void OscTests::OscConnectionsRouteLikeEndpoints()
{
    PatchDocument patch{};

    PatchEndpoint device{};
    device.Id = L"d";
    patch.Endpoints.push_back(device);

    PatchEndpoint osc{};
    osc.Id = L"O";
    osc.Osc = OscConnectionFromPreset(OscPreset::MidiType);
    patch.Endpoints.push_back(osc);

    PatchEndpoint idle{};
    idle.Id = L"idle";
    idle.Osc = OscConnectionFromPreset(OscPreset::Paths);
    patch.Endpoints.push_back(idle);

    patch.Connections.push_back(PatchConnection{ L"c1", L"O", 0, L"d", AllGroups, false });
    patch.Connections.push_back(PatchConnection{ L"c2", L"d", AllGroups, L"O", 0, false });

    RoutePatch input{};
    input.Key = L"Patch-Key";
    input.Patch = &patch;
    input.DeviceIds[L"d"] = L"\\\\?\\SWD#MIDISRV#SYNTH";

    auto const graph = CompileRoutes({ input });
    auto const oscId = OscDeviceId(L"Patch-Key", L"O");

    VERIFY_IS_TRUE(IsOscDeviceId(oscId));
    VERIFY_IS_FALSE(IsOscDeviceId(L"\\\\?\\swd#midisrv#synth"));
    VERIFY_ARE_EQUAL(std::wstring{ L"osc:patch-key|o" }, oscId);

    // The OSC connection is a source and a destination, under its own id, with its row as group.
    VERIFY_ARE_EQUAL(size_t{ 2 }, graph.Roots.size());

    auto const fromOsc = std::find_if(graph.Roots.begin(), graph.Roots.end(), [&](RouteRoot const& root) { return root.SourceDeviceId == oscId; });
    VERIFY_IS_TRUE(fromOsc != graph.Roots.end());
    VERIFY_ARE_EQUAL(0, fromOsc->SourceGroupIndex);

    auto const toOsc = std::find_if(graph.Leaves.begin(), graph.Leaves.end(), [&](RouteLeaf const& leaf) { return leaf.DestinationDeviceId == oscId; });
    VERIFY_IS_TRUE(toOsc != graph.Leaves.end());
    VERIFY_ARE_EQUAL(0, toOsc->DestinationGroupIndex);

    // Both connections listen, the one with nothing wired to it too.
    VERIFY_ARE_EQUAL(size_t{ 2 }, graph.OscConnections.size());
    VERIFY_ARE_EQUAL(OscDeviceId(L"Patch-Key", L"idle"), graph.OscConnections[1].DeviceId);

    // A change to an OSC connection's settings changes the plan, so the engine applies it.
    auto changed = patch;
    changed.Endpoints[2].Osc->ReceivePort = 9999;

    RoutePatch changedInput = input;
    changedInput.Patch = &changed;

    VERIFY_ARE_NOT_EQUAL(graph.Signature, CompileRoutes({ changedInput }).Signature);
    VERIFY_ARE_EQUAL(graph.Signature, CompileRoutes({ input }).Signature);
}

void OscTests::RuleAddressesAndNumbersRead()
{
    VERIFY_IS_TRUE(IsValidRuleAddress("/1/fader{n}"));
    VERIFY_IS_TRUE(IsValidRuleAddress("/{p}/push{n}"));
    VERIFY_IS_TRUE(IsValidRuleAddress("/track/{t}/volume"));
    VERIFY_IS_TRUE(IsValidRuleAddress("/play"));
    VERIFY_IS_FALSE(IsValidRuleAddress("fader{n}"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/a/{n}{m}"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/a/{n}5"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/a/{N}"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/a/{}"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/a/*"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/a b"));
    VERIFY_IS_FALSE(IsValidRuleAddress("/{a}/{b}/{c}/{d}/{e}"));

    RuleParts parts{};
    VERIFY_IS_TRUE(MatchRuleAddress("/{p}/push{n}", "/2/push16", parts));
    VERIFY_ARE_EQUAL(2, parts.Find("p").value_or(-1));
    VERIFY_ARE_EQUAL(16, parts.Find("n").value_or(-1));
    VERIFY_IS_FALSE(MatchRuleAddress("/1/fader{n}", "/1/fader", parts));
    VERIFY_IS_FALSE(MatchRuleAddress("/1/fader{n}", "/1/faderx", parts));
    VERIFY_IS_FALSE(MatchRuleAddress("/1/fader{n}", "/1/fader3/z", parts));
    VERIFY_IS_FALSE(MatchRuleAddress("/{n}/x{n}", "/1/x2", parts));
    VERIFY_IS_TRUE(MatchRuleAddress("/{n}/x{n}", "/3/x3", parts));
    VERIFY_IS_FALSE(MatchRuleAddress("/a{n}", "/a1234567", parts));

    RuleParts fill{};
    fill.Set("p", 4);
    fill.Set("n", 12);
    VERIFY_ARE_EQUAL(std::string{ "/4/push12" }, BuildRuleAddress("/{p}/push{n}", fill).value_or(""));
    fill = RuleParts{};
    fill.Set("p", 4);
    VERIFY_IS_FALSE(BuildRuleAddress("/{p}/push{n}", fill).has_value());

    auto number = ParseRuleNumber(L"{n}+70");
    VERIFY_IS_TRUE(number.has_value() && number->Part == "n" && number->Offset == 70);
    number = ParseRuleNumber(L"{n} - 1");
    VERIFY_IS_TRUE(number.has_value() && number->Part == "n" && number->Offset == -1);
    number = ParseRuleNumber(L"74");
    VERIFY_IS_TRUE(number.has_value() && number->Part.empty() && number->Offset == 74);
    VERIFY_IS_FALSE(ParseRuleNumber(L"").has_value());
    VERIFY_IS_FALSE(ParseRuleNumber(L"{n}*2").has_value());
    VERIFY_IS_FALSE(ParseRuleNumber(L"seven").has_value());

    VERIFY_ARE_EQUAL(std::wstring{ L"{n}+70" }, RuleNumberText(OscRuleNumber{ "n", 70 }));
    VERIFY_ARE_EQUAL(std::wstring{ L"{c}" }, RuleNumberText(OscRuleNumber{ "c", -1 }, 1));
    VERIFY_ARE_EQUAL(std::wstring{ L"5" }, RuleNumberText(OscRuleNumber{ {}, 4 }, 1));

    // Out needs every part from the channel or the number; in needs every number's part in the address.
    OscRule rule{};
    rule.Address = "/{p}/push{n}";
    rule.Message = OscRuleMessage::Note;
    rule.Number = OscRuleNumber{ "n", 59 };
    VERIFY_IS_TRUE(RuleReceives(rule));
    VERIFY_IS_FALSE(RuleSends(rule));
    rule.Channel = OscRuleNumber{ "p", -1 };
    VERIFY_IS_TRUE(RuleSends(rule));
    rule.Number = OscRuleNumber{ "q", 0 };
    VERIFY_IS_FALSE(RuleReceives(rule));
}

void OscTests::CustomRulesBecomeMidi()
{
    auto row = Preset(OscPreset::TouchOsc, 1);

    OscRule bend{};
    bend.Address = "/2/bend";
    bend.Message = OscRuleMessage::PitchBend;
    bend.Channel = OscRuleNumber{ {}, 3 };
    row.Rules.push_back(bend);

    OscRule program{};
    program.Address = "/3/prog{n}";
    program.Message = OscRuleMessage::ProgramChange;
    program.Number = OscRuleNumber{ "n", -1 };
    program.Low = 0;
    program.High = 127;
    row.Rules.push_back(program);

    OscRule upsideDown{};
    upsideDown.Address = "/xy";
    upsideDown.Argument = 1;
    upsideDown.Number = OscRuleNumber{ {}, 74 };
    upsideDown.Low = 1;
    upsideDown.High = 0;
    row.Rules.push_back(upsideDown);

    std::vector<OscRow> const rows{ row };

    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/fader3" }.Float(0.5f)) == std::vector<uint32_t>{ Cv(0xB0, 73, 64, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/fader1" }.Float(1.0f)) == std::vector<uint32_t>{ Cv(0xB0, 71, 127, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/fader1" }.Float(-3.0f)) == std::vector<uint32_t>{ Cv(0xB0, 71, 0, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/toggle2" }.Float(1.0f)) == std::vector<uint32_t>{ Cv(0xB0, 81, 127, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/toggle2" }.Bool(false)) == std::vector<uint32_t>{ Cv(0xB0, 81, 0, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/push1" }.Float(1.0f)) == std::vector<uint32_t>{ Cv(0x90, 60, 127, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/1/push1" }.Float(0.0f)) == std::vector<uint32_t>{ Cv(0x80, 60, 0, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/2/bend" }.Float(0.5f)) == std::vector<uint32_t>{ Cv(0xE3, 0x00, 0x40, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/2/bend" }.Int(1)) == std::vector<uint32_t>{ Cv(0xE3, 0x7F, 0x7F, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/3/prog5" }.Int(127)) == std::vector<uint32_t>{ Cv(0xC0, 4, 0, 1) });
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/3/prog5" }.Int(0)).empty());
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/xy" }.Float(0.9f).Float(0.25f)) == std::vector<uint32_t>{ Cv(0xB0, 74, 95, 1) });

    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/1/rotary1" }.Float(0.5f), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/1/fader90" }.Float(0.5f), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/1/fader2" }.String("x"), umps).Result));
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable), static_cast<int32_t>(Receive(rows, MessageBuilder{ "/1/fader2" }, umps).Result));

    // A pattern reaches the rules without parts.
    VERIFY_IS_TRUE(ReceiveWords(rows, MessageBuilder{ "/2/ben?" }.Float(0.0f)) == std::vector<uint32_t>{ Cv(0xE3, 0, 0, 1) });

    // A rule that only sends takes nothing.
    auto sendOnly = row;
    for (auto& rule : sendOnly.Rules) { rule.Direction = OscRuleDirection::Out; }
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::NoRowMatched), static_cast<int32_t>(Receive({ sendOnly }, MessageBuilder{ "/1/fader3" }.Float(0.5f), umps).Result));
}

void OscTests::CustomRulesSendOsc()
{
    auto row = Preset(OscPreset::TouchOsc, 1);

    OscRule bend{};
    bend.Address = "/ch{c}/bend";
    bend.Message = OscRuleMessage::PitchBend;
    bend.Channel = OscRuleNumber{ "c", -1 };
    bend.Low = -1;
    bend.High = 1;
    row.Rules.push_back(bend);

    std::vector<OscRow> const rows{ row };
    OutboundTranslator translator{};

    auto const send = [&](uint32_t word)
        {
            std::vector<MessageBuilder> out{};
            translator.Translate(rows, &word, 1, out);
            return out;
        };

    auto out = send(Cv(0xB0, 73, 127, 1));
    VERIFY_ARE_EQUAL(size_t{ 1 }, out.size());
    auto copy = Reread(out[0]);
    VERIFY_ARE_EQUAL(std::string{ "/1/fader3" }, copy.Address);
    VERIFY_ARE_EQUAL(std::string{ "f" }, copy.TypeTags);
    VERIFY_ARE_EQUAL(1.0, copy.Arguments[0].Real);

    copy = Reread(send(Cv(0x90, 61, 127, 1))[0]);
    VERIFY_ARE_EQUAL(std::string{ "/1/push2" }, copy.Address);
    VERIFY_ARE_EQUAL(1.0, copy.Arguments[0].Real);

    copy = Reread(send(Cv(0x80, 61, 64, 1))[0]);
    VERIFY_ARE_EQUAL(0.0, copy.Arguments[0].Real);

    copy = Reread(send(Cv(0xE4, 0x00, 0x00, 1))[0]);
    VERIFY_ARE_EQUAL(std::string{ "/ch5/bend" }, copy.Address);
    VERIFY_ARE_EQUAL(-1.0, copy.Arguments[0].Real);

    // Nothing for a controller no rule names, for a channel the rules don't take, or for a
    // group with no row.
    VERIFY_IS_TRUE(send(Cv(0xB0, 7, 100, 1)).empty());
    VERIFY_IS_TRUE(send(Cv(0xB1, 73, 100, 1)).empty());
    VERIFY_IS_TRUE(send(Cv(0xB0, 73, 100, 2)).empty());

    std::vector<MessageBuilder> sysEx{};
    uint32_t const words[2]{ 0x31020102, 0 };
    VERIFY_ARE_EQUAL(static_cast<int32_t>(OutboundResult::LeftOut), static_cast<int32_t>(translator.Translate(rows, words, 2, sysEx)));

    // What goes out comes back as the same MIDI.
    for (auto const word : { Cv(0xB0, 75, 100, 1), Cv(0x90, 66, 127, 1), Cv(0xB0, 80, 127, 1) })
    {
        auto const back = ReceiveWords(rows, send(word)[0]);
        VERIFY_IS_TRUE(back == std::vector<uint32_t>{ word });
    }
}

void OscTests::CustomRulesSurviveTheFile()
{
    auto settings = OscConnectionFromPreset(OscPreset::TouchOsc);

    OscRule odd{};
    odd.Address = "/{p}/push{n}";
    odd.Argument = 2;
    odd.Direction = OscRuleDirection::In;
    odd.Message = OscRuleMessage::PolyPressure;
    odd.Channel = OscRuleNumber{ "p", -1 };
    odd.Number = OscRuleNumber{ "n", 35 };
    odd.Low = -0.5;
    odd.High = 2.25;
    settings.Rows[0].Rules.push_back(odd);

    auto const json = OscConnectionToJson(settings);
    auto const back = OscConnectionFromJson(json);

    VERIFY_ARE_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));
    VERIFY_IS_TRUE(OscPresetOf(back.Rows[0]) == std::nullopt);
    VERIFY_ARE_EQUAL(size_t{ 4 }, back.Rows[0].Rules.size());
    VERIFY_IS_TRUE(OscPresetOf(OscConnectionFromPreset(OscPreset::TouchOsc).Rows[0]) == OscPreset::TouchOsc);

    // Rules that can't work are dropped; what's left keeps its order.
    auto const text = LR"({ "rows": [ { "group": 4, "root": "/ignored", "format": "custom", "rules": [
        { "address": "/a{n}", "number": "{n}+1" },
        { "address": "no slash" },
        { "address": "/b", "number": "{x}*2" },
        { "address": "/c", "low": 1, "high": 1 },
        { "address": "/d", "message": "pitchBend", "channel": 20, "argument": 99 },
        { "address": "/é" } ] } ] })";

    auto const read = OscConnectionFromJson(json::JsonObject::Parse(text));
    VERIFY_ARE_EQUAL(size_t{ 1 }, read.Rows.size());
    VERIFY_ARE_EQUAL(std::string{}, read.Rows[0].Root);
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.Rows[0].Rules.size());
    VERIFY_ARE_EQUAL(std::string{ "/a{n}" }, read.Rows[0].Rules[0].Address);
    VERIFY_ARE_EQUAL(std::string{ "/d" }, read.Rows[0].Rules[1].Address);
    VERIFY_ARE_EQUAL(int32_t{ 20 }, read.Rows[0].Rules[1].Channel.Offset);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, read.Rows[0].Rules[1].Argument);

    // A channel past 15 is kept in the file but takes nothing.
    UmpCollector umps{};
    VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Unreadable), static_cast<int32_t>(Receive(read.Rows, MessageBuilder{ "/d" }.Float(0.5f), umps).Result));
}

void OscTests::RulesAreSuggestedForTouchOsc()
{
    auto const suggest = [](MessageBuilder const& builder, std::vector<OscRow> const& rows)
        {
            auto const packet = MakePacket(builder);
            MessageView view{};
            ReadMessage(packet.data(), packet.size(), view);
            return SuggestRule(view, rows);
        };

    // From nothing: TouchOSC's first page in three clicks, the same numbers as the preset.
    std::vector<OscRow> rows{};

    auto rule = suggest(MessageBuilder{ "/1/fader5" }.Float(0.3f), rows);
    VERIFY_IS_TRUE(rule.has_value());
    VERIFY_ARE_EQUAL(std::string{ "/1/fader{n}" }, rule->Address);
    VERIFY_IS_TRUE(rule->Message == OscRuleMessage::ControlChange && rule->Number.Part == "n" && rule->Number.Offset == 70);
    VERIFY_ARE_EQUAL(0.0, rule->Low);
    VERIFY_ARE_EQUAL(1.0, rule->High);

    OscRow custom{};
    custom.Format = OscFormat::Custom;
    custom.Root.clear();
    custom.Rules.push_back(*rule);
    rows.push_back(custom);

    rule = suggest(MessageBuilder{ "/1/toggle1" }.Float(1.0f), rows);
    VERIFY_IS_TRUE(rule.has_value() && rule->Address == "/1/toggle{n}" && rule->Message == OscRuleMessage::ControlChange && rule->Number.Offset == 79);
    rows[0].Rules.push_back(*rule);

    rule = suggest(MessageBuilder{ "/2/push3" }.Float(1.0f), rows);
    VERIFY_IS_TRUE(rule.has_value() && rule->Address == "/2/push{n}" && rule->Message == OscRuleMessage::Note && rule->Number.Offset == 59);

    // A second page of faders doesn't land on the first page's controllers.
    rule = suggest(MessageBuilder{ "/2/fader1" }.Float(0.5f), rows);
    VERIFY_IS_TRUE(rule.has_value() && rule->Number.Offset != 70);
    VERIFY_IS_TRUE(rule->Number.Offset + 1 > 78 || rule->Number.Offset + 8 < 71);

    // An address with no number at the end is one controller. A whole number past 1 is a MIDI value.
    rule = suggest(MessageBuilder{ "/volume" }.Int(100), {});
    VERIFY_IS_TRUE(rule.has_value() && rule->Address == "/volume" && rule->Number.Part.empty() && rule->Number.Offset == 71);
    VERIFY_ARE_EQUAL(127.0, rule->High);

    // The value is the first number.
    rule = suggest(MessageBuilder{ "/label3" }.String("x").Float(0.5f), {});
    VERIFY_IS_TRUE(rule.has_value() && rule->Argument == 1);

    VERIFY_IS_FALSE(suggest(MessageBuilder{ "/text" }.String("x"), {}).has_value());
    VERIFY_IS_FALSE(suggest(MessageBuilder{ "/1/fader*" }.Float(0.5f), {}).has_value());
}

void OscTests::AdvertisingIsSavedAndNamed()
{
    // On unless the file says otherwise, and only written when off.
    auto settings = OscConnectionFromPreset(OscPreset::Paths);
    VERIFY_IS_TRUE(settings.Advertise);

    auto const onText = OscConnectionToJson(settings).Stringify();
    VERIFY_IS_TRUE(std::wstring_view{ onText }.find(L"advertise") == std::wstring_view::npos);

    settings.Advertise = false;
    auto const offText = OscConnectionToJson(settings).Stringify();
    VERIFY_IS_TRUE(std::wstring_view{ offText }.find(L"\"advertise\":false") != std::wstring_view::npos);

    auto const back = OscConnectionFromJson(json::JsonObject::Parse(offText));
    VERIFY_IS_FALSE(back.Advertise);
    VERIFY_ARE_NOT_EQUAL(OscConnectionSignature(back), OscConnectionSignature(OscConnectionFromJson(json::JsonObject::Parse(onText))));

    // Anything but a Boolean reads as the default.
    auto object = json::JsonObject::Parse(offText);
    object.GetNamedObject(L"receive").SetNamedValue(L"advertise", json::JsonValue::CreateStringValue(L"no"));
    VERIFY_IS_TRUE(OscConnectionFromJson(object).Advertise);

    // The name is the connection's, then the computer's.
    VERIFY_ARE_EQUAL(std::wstring{ L"Phone (STUDIO-PC)" }, OscServiceLabel(L"Phone", L"STUDIO-PC"));
    VERIFY_ARE_EQUAL(std::wstring{ L"Phone" }, OscServiceLabel(L"Phone", L""));
    VERIFY_ARE_EQUAL(std::wstring{ L"STUDIO-PC" }, OscServiceLabel(L"", L"STUDIO-PC"));
    VERIFY_ARE_EQUAL(std::wstring{ L"OSC" }, OscServiceLabel(L"", L""));
    VERIFY_ARE_EQUAL(std::wstring{ L"OSC" }, OscServiceLabel(L" . ", L"\t"));

    // Periods would split the label, and control characters become spaces too.
    VERIFY_ARE_EQUAL(std::wstring{ L"v1 2 Pad (pc)" }, OscServiceLabel(L"v1.2\x7FPad", L"pc."));
    VERIFY_ARE_EQUAL(std::wstring{ L"a b (c)" }, OscServiceLabel(L"a\nb", L"c"));

    // At most 63 bytes of UTF-8, cut where a whole character ends.
    auto const longLabel = OscServiceLabel(std::wstring(100, L'x'), L"pc");
    VERIFY_ARE_EQUAL(size_t{ 63 }, longLabel.size());

    auto const utf8Bytes = [](std::wstring const& text)
        {
            return winrt::to_string(text).size();
        };

    // Three bytes each: 21 fit exactly.
    auto const wide = OscServiceLabel(std::wstring(40, L'\x0FE9'), L"");
    VERIFY_ARE_EQUAL(size_t{ 21 }, wide.size());
    VERIFY_ARE_EQUAL(size_t{ 63 }, utf8Bytes(wide));

    // Four bytes for each pair, which is never split: 15 pairs make 60 bytes.
    std::wstring pairs{};

    for (int i = 0; i < 20; i++)
    {
        pairs += L"\xD83C\xDFB9";
    }

    auto const piano = OscServiceLabel(pairs, L"");
    VERIFY_ARE_EQUAL(size_t{ 30 }, piano.size());
    VERIFY_ARE_EQUAL(size_t{ 60 }, utf8Bytes(piano));
    VERIFY_IS_TRUE(piano.back() == L'\xDFB9');
}

void OscTests::PacketsAreFramedOnAStream()
{
    // A packet with every byte SLIP has to escape, and a second one.
    std::vector<uint8_t> const first{ 0x2F, 0xC0, 0x41, 0xDB, 0x00, 0xDC, 0xDD, 0xC0 };
    std::vector<uint8_t> const second{ 0x2F, 0x6D, 0x00, 0x00 };

    for (auto const framing : { osc::StreamFraming::Slip, osc::StreamFraming::SizeFirst })
    {
        std::vector<uint8_t> stream{};
        osc::AppendFramed(framing, first, stream);
        osc::AppendFramed(framing, second, stream);

        if (framing == osc::StreamFraming::Slip)
        {
            // END, the packet with its two ENDs and one ESC escaped, END. No END inside.
            VERIFY_ARE_EQUAL(size_t{ 2 + first.size() + 3 + 2 + second.size() }, stream.size());
            VERIFY_ARE_EQUAL(osc::slip::End, stream.front());
            VERIFY_ARE_EQUAL(size_t{ 4 }, static_cast<size_t>(std::count(stream.begin(), stream.end(), osc::slip::End)));
        }
        else
        {
            VERIFY_ARE_EQUAL(size_t{ 4 + first.size() + 4 + second.size() }, stream.size());
            VERIFY_ARE_EQUAL(uint8_t{ 8 }, stream[3]);
        }

        // Read whole, and a byte at a time: the same two packets either way.
        for (size_t step : { stream.size(), size_t{ 1 }, size_t{ 3 } })
        {
            osc::StreamReader reader{ framing };
            std::vector<std::vector<uint8_t>> packets{};

            for (size_t start = 0; start < stream.size(); start += step)
            {
                auto const length = (std::min)(step, stream.size() - start);
                auto const problem = reader.Read(std::span<uint8_t const>{ stream.data() + start, length },
                    [&](std::span<uint8_t const> packet) { packets.emplace_back(packet.begin(), packet.end()); });

                VERIFY_IS_TRUE(problem == osc::StreamProblem::None);
            }

            VERIFY_ARE_EQUAL(size_t{ 2 }, packets.size());
            VERIFY_IS_TRUE(packets[0] == first);
            VERIFY_IS_TRUE(packets[1] == second);
        }
    }

    // SLIP: noise and empty packets between ENDs are nothing, and a packet too long is dropped
    // without losing the next one.
    {
        std::vector<uint8_t> stream{ osc::slip::End, osc::slip::End, osc::slip::End };
        stream.insert(stream.end(), osc::MaximumPacketBytes + 10, uint8_t{ 0x41 });
        osc::AppendFramed(osc::StreamFraming::Slip, second, stream);

        osc::StreamReader reader{ osc::StreamFraming::Slip };
        std::vector<std::vector<uint8_t>> packets{};

        auto const problem = reader.Read(stream, [&](std::span<uint8_t const> packet) { packets.emplace_back(packet.begin(), packet.end()); });

        VERIFY_IS_TRUE(problem == osc::StreamProblem::TooLong);
        VERIFY_ARE_EQUAL(size_t{ 1 }, packets.size());
        VERIFY_IS_TRUE(packets[0] == second);
    }

    // Size first: a size of 0, or past the largest packet, ends the stream.
    for (std::vector<uint8_t> const bad : { std::vector<uint8_t>{ 0, 0, 0, 0 }, std::vector<uint8_t>{ 0x7F, 0xFF, 0xFF, 0xFF } })
    {
        osc::StreamReader reader{ osc::StreamFraming::SizeFirst };
        auto called = false;

        VERIFY_IS_TRUE(reader.Read(bad, [&](std::span<uint8_t const>) { called = true; }) == osc::StreamProblem::BadSize);
        VERIFY_IS_FALSE(called);
    }

    // The largest packet is fine.
    {
        std::vector<uint8_t> big(osc::MaximumPacketBytes, uint8_t{ 0xC0 });
        std::vector<uint8_t> stream{};
        osc::AppendFramed(osc::StreamFraming::Slip, big, stream);

        osc::StreamReader reader{ osc::StreamFraming::Slip };
        size_t size{ 0 };

        VERIFY_IS_TRUE(reader.Read(stream, [&](std::span<uint8_t const> packet) { size = packet.size(); }) == osc::StreamProblem::None);
        VERIFY_ARE_EQUAL(osc::MaximumPacketBytes, size);
    }
}

void OscTests::TcpSettingsSurviveTheFile()
{
    // UDP writes nothing new, so older patches read the same.
    auto settings = OscConnectionFromPreset(OscPreset::Paths);
    auto const udpText = std::wstring{ OscConnectionToJson(settings).Stringify() };
    VERIFY_IS_TRUE(udpText.find(L"transport") == std::wstring::npos);
    VERIFY_IS_TRUE(OscConnectionFromJson(json::JsonObject::Parse(udpText)).Transport == OscTransport::Udp);

    settings.Transport = OscTransport::TcpConnect;
    settings.TcpFraming = OscTcpFraming::SizeFirst;
    settings.SendHost = L"ipad.local";
    settings.SendPort = 9000;

    auto const text = OscConnectionToJson(settings).Stringify();
    auto back = OscConnectionFromJson(json::JsonObject::Parse(text));

    VERIFY_IS_TRUE(back.Transport == OscTransport::TcpConnect);
    VERIFY_IS_TRUE(back.TcpFraming == OscTcpFraming::SizeFirst);
    VERIFY_ARE_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));
    VERIFY_ARE_NOT_EQUAL(OscConnectionSignature(back), OscConnectionSignature(OscConnectionFromJson(json::JsonObject::Parse(udpText))));

    // Connecting receives and sends on the one connection, and listens for nobody.
    VERIFY_IS_TRUE(OscConnectionReceives(back));
    VERIFY_IS_TRUE(OscConnectionSends(back));
    VERIFY_IS_FALSE(OscConnectionListens(back));

    back.SendHost.clear();
    VERIFY_IS_FALSE(OscConnectionReceives(back));
    VERIFY_IS_FALSE(OscConnectionSends(back));

    // Waiting for apps listens, and sends to whoever connects.
    OscConnectionSettings listen{};
    listen.Transport = OscTransport::TcpListen;
    listen.ReceivePort = 8000;
    VERIFY_IS_TRUE(OscConnectionListens(listen));
    VERIFY_IS_TRUE(OscConnectionReceives(listen));
    VERIFY_IS_TRUE(OscConnectionSends(listen));

    // It has no computer it sends to, so taking messages from that computer means taking them
    // from the one named, or from anyone when there's none.
    listen.AcceptFrom = OscAcceptFrom::SendHost;
    listen.SendHost = L"192.168.1.199";
    SanitizeOscConnection(listen);
    VERIFY_IS_TRUE(listen.AcceptFrom == OscAcceptFrom::List);
    VERIFY_ARE_EQUAL(size_t{ 1 }, listen.AcceptAddresses.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"192.168.1.199" }, listen.AcceptAddresses[0]);

    listen.AcceptFrom = OscAcceptFrom::SendHost;
    listen.SendHost.clear();
    listen.AcceptAddresses.clear();
    SanitizeOscConnection(listen);
    VERIFY_IS_TRUE(listen.AcceptFrom == OscAcceptFrom::Anyone);

    // Unknown names read as the defaults.
    auto object = json::JsonObject::Parse(text);
    object.SetNamedValue(L"transport", json::JsonValue::CreateStringValue(L"carrierPigeon"));
    object.SetNamedValue(L"tcpFraming", json::JsonValue::CreateNumberValue(1));
    auto const odd = OscConnectionFromJson(object);
    VERIFY_IS_TRUE(odd.Transport == OscTransport::Udp);
    VERIFY_IS_TRUE(odd.TcpFraming == OscTcpFraming::Slip);
}

void OscTests::FloatsBecomeMidi2()
{
    // The 32-bit pitch bend's center is exact, and so are its ends.
    VERIFY_ARE_EQUAL(0x80000000u, PitchBend32FromUnit(0.5));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, PitchBend32FromUnit(1.0));
    VERIFY_ARE_EQUAL(0u, PitchBend32FromUnit(0.0));
    VERIFY_ARE_EQUAL(0x40000000u, PitchBend32FromUnit(0.25));
    VERIFY_ARE_EQUAL(0x80000000u, PitchBend32FromUnit(std::numeric_limits<double>::quiet_NaN()));

    auto row = OscRowFromPreset(OscPreset::Paths, 2);
    row.Values = OscValueKind::UnitFloats;
    row.Midi2 = true;

    struct Case
    {
        char const* Address;
        float Value;
        uint32_t Word0;
        uint32_t Word1;
    };

    // Floats from 0 to 1 fill the whole range: 32 bits, or 16 for a velocity.
    Case const floats[]
    {
        { "/midi/1/cc/7", 0.5f, 0x42B00700u, 0x80000000u },
        { "/midi/1/cc/7", 1.0f, 0x42B00700u, 0xFFFFFFFFu },
        { "/midi/1/cc/7", 0.25f, 0x42B00700u, 0x40000000u },
        { "/midi/2/note/60", 1.0f, 0x42913C00u, 0xFFFF0000u },
        { "/midi/2/note/60", 0.5f, 0x42913C00u, 0x80000000u },
        { "/midi/2/note/60", 0.0f, 0x42813C00u, 0x00000000u },
        { "/midi/1/pitch", 0.5f, 0x42E00000u, 0x80000000u },
        { "/midi/1/pitch", 1.0f, 0x42E00000u, 0xFFFFFFFFu },
        { "/midi/1/channelpressure", 1.0f, 0x42D00000u, 0xFFFFFFFFu },
        { "/midi/1/polyaftertouch/64", 0.5f, 0x42A04000u, 0x80000000u },
    };

    for (auto const& c : floats)
    {
        UmpCollector umps{};
        auto const outcome = Receive({ row }, MessageBuilder{ c.Address }.Float(c.Value), umps);

        VERIFY_ARE_EQUAL(static_cast<int32_t>(InboundResult::Translated), static_cast<int32_t>(outcome.Result));
        VERIFY_ARE_EQUAL(size_t{ 1 }, umps.Umps.size());
        VERIFY_ARE_EQUAL(size_t{ 2 }, umps.Umps[0].size());
        VERIFY_ARE_EQUAL(c.Word0, umps.Umps[0][0]);
        VERIFY_ARE_EQUAL(c.Word1, umps.Umps[0][1]);
    }

    // Whole numbers are scaled up the MIDI 2.0 way: the center stays the center and the top the top.
    auto whole = OscRowFromPreset(OscPreset::Paths, 0);
    whole.Midi2 = true;

    Case const wholes[]
    {
        { "/midi/1/cc/7", 127, 0x40B00700u, 0xFFFFFFFFu },
        { "/midi/1/cc/7", 64, 0x40B00700u, 0x80000000u },
        { "/midi/1/cc/7", 0, 0x40B00700u, 0x00000000u },
        { "/midi/1/note/60", 127, 0x40903C00u, 0xFFFF0000u },
        { "/midi/1/note/60", 0, 0x40803C00u, 0x00000000u },
        { "/midi/1/program", 5, 0x40C00000u, 0x05000000u },
        { "/midi/1/pitch", 8192, 0x40E00000u, 0x80000000u },
        { "/midi/1/pitch", 16383, 0x40E00000u, 0xFFFFFFFFu },
    };

    for (auto const& c : wholes)
    {
        UmpCollector umps{};
        Receive({ whole }, MessageBuilder{ c.Address }.Int(static_cast<int32_t>(c.Value)), umps);

        VERIFY_ARE_EQUAL(size_t{ 1 }, umps.Umps.size());
        VERIFY_ARE_EQUAL(c.Word0, umps.Umps[0][0]);
        VERIFY_ARE_EQUAL(c.Word1, umps.Umps[0][1]);
    }

    // System messages stay as they are.
    {
        UmpCollector umps{};
        Receive({ whole }, MessageBuilder{ "/midi/clock" }, umps);
        VERIFY_ARE_EQUAL(size_t{ 1 }, umps.Umps.size());
        VERIFY_ARE_EQUAL(0x10F80000u, umps.Umps[0][0]);
    }

    // TouchOSC's own layout: a fader's float sets its controller to 32 bits, and a push button
    // plays and lets go of its note.
    auto touch = OscRowFromPreset(OscPreset::TouchOsc, 0);
    touch.Midi2 = true;
    VERIFY_IS_TRUE(OscPresetOf(touch) == OscPreset::TouchOsc);

    {
        UmpCollector umps{};
        Receive({ touch }, MessageBuilder{ "/1/fader1" }.Float(0.5f), umps);
        VERIFY_ARE_EQUAL(size_t{ 1 }, umps.Umps.size());
        VERIFY_ARE_EQUAL(0x40B04700u, umps.Umps[0][0]);
        VERIFY_ARE_EQUAL(0x80000000u, umps.Umps[0][1]);

        umps.Umps.clear();
        Receive({ touch }, MessageBuilder{ "/1/push1" }.Float(1.0f), umps);
        Receive({ touch }, MessageBuilder{ "/1/push1" }.Float(0.0f), umps);
        VERIFY_ARE_EQUAL(size_t{ 2 }, umps.Umps.size());
        VERIFY_ARE_EQUAL(0x40903C00u, umps.Umps[0][0]);
        VERIFY_ARE_EQUAL(0xFFFF0000u, umps.Umps[0][1]);
        VERIFY_ARE_EQUAL(0x40803C00u, umps.Umps[1][0]);
    }

    // Off, the same message is MIDI 1.0 as before.
    {
        touch.Midi2 = false;
        UmpCollector umps{};
        Receive({ touch }, MessageBuilder{ "/1/fader1" }.Float(0.5f), umps);
        VERIFY_ARE_EQUAL(size_t{ 1 }, umps.Umps.size());
        VERIFY_ARE_EQUAL(size_t{ 1 }, umps.Umps[0].size());
        VERIFY_ARE_EQUAL(0x20B04740u, umps.Umps[0][0]);
    }

    // Saved only when on, and read back.
    OscConnectionSettings settings{};
    settings.ReceivePort = 8000;
    settings.Rows = { row };
    auto const text = std::wstring{ OscConnectionToJson(settings).Stringify() };
    VERIFY_IS_TRUE(text.find(L"\"midi2\":true") != std::wstring::npos);
    auto const back = OscConnectionFromJson(json::JsonObject::Parse(text));
    VERIFY_IS_TRUE(back.Rows[0].Midi2);
    VERIFY_ARE_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));

    settings.Rows[0].Midi2 = false;
    VERIFY_IS_TRUE(std::wstring{ OscConnectionToJson(settings).Stringify() }.find(L"midi2") == std::wstring::npos);
    VERIFY_ARE_NOT_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));
}

void OscTests::DevicesAreSavedAndNamed()
{
    // Off unless asked for, and only written when on.
    auto settings = OscConnectionFromPreset(OscPreset::TouchOsc);
    VERIFY_IS_FALSE(settings.ShowAsDevice);
    VERIFY_IS_TRUE(std::wstring{ OscConnectionToJson(settings).Stringify() }.find(L"showAsDevice") == std::wstring::npos);

    settings.ShowAsDevice = true;
    auto const text = std::wstring{ OscConnectionToJson(settings).Stringify() };
    VERIFY_IS_TRUE(text.find(L"\"showAsDevice\":true") != std::wstring::npos);

    auto const back = OscConnectionFromJson(json::JsonObject::Parse(text));
    VERIFY_IS_TRUE(back.ShowAsDevice);
    VERIFY_ARE_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));

    settings.ShowAsDevice = false;
    VERIFY_ARE_NOT_EQUAL(OscConnectionSignature(settings), OscConnectionSignature(back));

    // The same connection always gets the same product instance id, whatever the case of its
    // id, and different connections different ones. It's short and plain ASCII, as MIDI 2.0's
    // product instance id has to be.
    auto const one = OscDeviceProductInstanceId(L"osc:{1234}|node-1");
    VERIFY_ARE_EQUAL(one, OscDeviceProductInstanceId(L"OSC:{1234}|NODE-1"));
    VERIFY_ARE_NOT_EQUAL(one, OscDeviceProductInstanceId(L"osc:{1234}|node-2"));
    VERIFY_ARE_EQUAL(size_t{ 21 }, one.size());
    VERIFY_IS_TRUE(one.starts_with(L"PATCHBAY-OSC-"));
    VERIFY_IS_TRUE(std::all_of(one.begin(), one.end(), [](wchar_t c) { return c >= 0x20 && c < 0x7F; }));
}