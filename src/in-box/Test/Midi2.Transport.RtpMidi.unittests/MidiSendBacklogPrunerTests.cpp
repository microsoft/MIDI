// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSendBacklogPrunerTests.h"

#include "midi_send_backlog_pruner.h"

#include <chrono>
#include <functional>
#include <random>

using namespace WindowsMidiServicesInternal;
using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    uint32_t Midi1(_In_ uint8_t const group, _In_ uint8_t const status, _In_ uint8_t const channel, _In_ uint8_t const data1, _In_ uint8_t const data2)
    {
        return 0x20000000u |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status & 0x0F) << 20) |
            (static_cast<uint32_t>(channel & 0x0F) << 16) |
            (static_cast<uint32_t>(data1) << 8) |
            data2;
    }

    uint32_t Cc(_In_ uint8_t const controller, _In_ uint8_t const value, _In_ uint8_t const channel = 0, _In_ uint8_t const group = 0)
    {
        return Midi1(group, 0xB, channel, controller, value);
    }

    uint32_t Bend(_In_ uint16_t const value, _In_ uint8_t const channel = 0)
    {
        return Midi1(0, 0xE, channel, static_cast<uint8_t>(value & 0x7F), static_cast<uint8_t>((value >> 7) & 0x7F));
    }

    uint32_t NoteOn(_In_ uint8_t const note, _In_ uint8_t const channel = 0, _In_ uint8_t const group = 0)
    {
        return Midi1(group, 0x9, channel, note, 0x64);
    }

    void AddMidi2(
        _Inout_ std::vector<uint32_t>& words,
        _In_ uint8_t const group,
        _In_ uint8_t const status,
        _In_ uint8_t const channel,
        _In_ uint8_t const index1,
        _In_ uint8_t const index2,
        _In_ uint32_t const value)
    {
        words.push_back(0x40000000u |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status & 0x0F) << 20) |
            (static_cast<uint32_t>(channel & 0x0F) << 16) |
            (static_cast<uint32_t>(index1) << 8) |
            index2);
        words.push_back(value);
    }

    std::vector<uint32_t> CcRamp(_In_ uint8_t const controller, _In_ std::vector<uint8_t> const& values, _In_ uint8_t const channel = 0, _In_ uint8_t const group = 0)
    {
        std::vector<uint32_t> words;

        for (auto const value : values)
        {
            words.push_back(Cc(controller, value, channel, group));
        }

        return words;
    }

    // No steps always go unless asked, so short ramps show each of the other rules on its own
    std::vector<uint32_t> Pruned(
        _In_ std::vector<uint32_t> words,
        _In_ uint32_t const keepOneIn,
        _Out_opt_ MidiSendBacklogPruneResult* result = nullptr,
        _In_ uint32_t const stepsKeptInEveryRun = 0)
    {
        MidiSendBacklogPruner pruner{ stepsKeptInEveryRun };
        auto const done = pruner.PruneUmp(words, 0, keepOneIn);

        if (result != nullptr)
        {
            *result = done;
        }

        return words;
    }

    // The values of the MIDI 1.0 control changes with this number, in order
    std::vector<uint8_t> CcValues(_In_ std::vector<uint32_t> const& words, _In_ uint8_t const controller)
    {
        std::vector<uint8_t> values;

        for (size_t i = 0; i < words.size(); i += MidiSendBacklogPruner::UmpWordCount(words[i]))
        {
            if ((words[i] & 0xF0F0FF00u) == (0x20B00000u | (static_cast<uint32_t>(controller) << 8)))
            {
                values.push_back(static_cast<uint8_t>(words[i] & 0x7F));
            }
        }

        return values;
    }

    std::wstring Text(_In_ std::vector<uint32_t> const& words)
    {
        std::wstring text;

        for (auto const word : words)
        {
            text += String().Format(L"%08X ", word);
        }

        return text;
    }

    std::wstring Text(_In_ std::vector<uint8_t> const& values)
    {
        std::wstring text;

        for (auto const value : values)
        {
            text += String().Format(L"%u ", value);
        }

        return text;
    }

    void VerifyWords(_In_ std::vector<uint32_t> const& actual, _In_ std::vector<uint32_t> const& expected, _In_ wchar_t const* what)
    {
        VERIFY_IS_TRUE(actual == expected, String().Format(L"%s. Got %s, expected %s", what, Text(actual).c_str(), Text(expected).c_str()));
    }

    void VerifyValues(_In_ std::vector<uint8_t> const& actual, _In_ std::vector<uint8_t> const& expected, _In_ wchar_t const* what)
    {
        VERIFY_IS_TRUE(actual == expected, String().Format(L"%s. Got %s, expected %s", what, Text(actual).c_str(), Text(expected).c_str()));
    }

    std::vector<uint8_t> const TenStepRamp{ 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
}


void MidiSendBacklogPrunerTests::TestThresholdAndHowHardToThin()
{
    VERIFY_ARE_EQUAL(MidiSendBacklogPruneThreshold(0), 0ull, L"no limit, nothing to thin");
    VERIFY_ARE_EQUAL(MidiSendBacklogPruneThreshold(1), 312ull, L"100 ms at wire speed");
    VERIFY_ARE_EQUAL(MidiSendBacklogPruneThreshold(32), 10000ull, L"100 ms at 32 times");
    VERIFY_ARE_EQUAL(MidiSendBacklogPruneThreshold(33), 0ull, L"past the fastest limit is no limit");

    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(100, 312), 1u, L"under the threshold");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(312, 312), 1u, L"at the threshold");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(313, 312), 2u, L"just over");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(624, 312), 2u, L"twice");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(625, 312), 4u, L"over twice");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(1248, 312), 4u, L"four times");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(1249, 312), 8u, L"over four times");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(2496, 312), 8u, L"eight times");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(2497, 312), 0u, L"over eight times keeps no steps");
    VERIFY_ARE_EQUAL(ChooseMidiSendBacklogKeepOneIn(1'000'000, 0), 1u, L"no threshold, nothing to thin");
}

void MidiSendBacklogPrunerTests::TestRampKeepsBothEndsAndEveryOtherStep()
{
    MidiSendBacklogPruneResult result{};
    auto const words = Pruned(CcRamp(7, TenStepRamp), 2, &result);

    VerifyValues(CcValues(words, 7), { 0, 20, 40, 60, 80, 100 }, L"the ends, and every second step");
    VERIFY_ARE_EQUAL(result.MessagesRemoved, 5u);
}

void MidiSendBacklogPrunerTests::TestKeepOneInFourAndKeepNone()
{
    VerifyValues(CcValues(Pruned(CcRamp(7, TenStepRamp), 4), 7), { 0, 40, 80, 100 }, L"one step in four");
    VerifyValues(CcValues(Pruned(CcRamp(7, TenStepRamp), 8), 7), { 0, 80, 100 }, L"one step in eight");
    VerifyValues(CcValues(Pruned(CcRamp(7, TenStepRamp), 0), 7), { 0, 100 }, L"no steps");
    VerifyValues(CcValues(Pruned(CcRamp(7, TenStepRamp), 1), 7), TenStepRamp, L"every step");
}

void MidiSendBacklogPrunerTests::TestTurningPointsAreKept()
{
    VerifyValues(CcValues(Pruned(CcRamp(1, { 0, 10, 20, 30, 20, 10, 0, 10, 20 }), 0), 1), { 0, 30, 0, 20 },
        L"where it starts, turns and ends");

    VerifyValues(CcValues(Pruned(CcRamp(1, { 50, 51, 50, 51, 50 }), 0), 1), { 50, 51, 50, 51, 50 },
        L"a wobble is all turning points");
}

void MidiSendBacklogPrunerTests::TestRepeatedValuesAreKept()
{
    VerifyValues(CcValues(Pruned(CcRamp(20, { 127, 127, 127, 127 }), 0), 20), { 127, 127, 127, 127 },
        L"a button that sends the same value for each press");

    VerifyValues(CcValues(Pruned(CcRamp(16, { 65, 65, 66, 66, 67 }), 0), 16), { 65, 65, 66, 66, 67 },
        L"an encoder that steps by the same amount");

    VerifyValues(CcValues(Pruned(CcRamp(7, { 10, 20, 20, 30, 40 }), 0), 7), { 10, 20, 20, 40 },
        L"a value held for a moment, and the steps after it");
}

void MidiSendBacklogPrunerTests::TestFewerThanThreePointsChangeNothing()
{
    VerifyValues(CcValues(Pruned(CcRamp(7, { 10, 20 }), 0), 7), { 10, 20 }, L"two points");
    VerifyValues(CcValues(Pruned(CcRamp(7, { 10, 20, 30 }), 0), 7), { 10, 30 }, L"three points");
    VerifyWords(Pruned({}, 0), {}, L"nothing");

    MidiSendBacklogPruneResult result{};
    VerifyWords(Pruned({ Cc(7, 1), NoteOn(60), Cc(7, 2) }, 0, &result), { Cc(7, 1), NoteOn(60), Cc(7, 2) }, L"split by a note");
    VERIFY_ARE_EQUAL(result.MessagesRemoved, 0u);
}

void MidiSendBacklogPrunerTests::TestNoteKeepsTheValuesAroundIt()
{
    std::vector<uint32_t> words{ Cc(74, 0), Cc(74, 10), Cc(74, 20), Cc(74, 30), NoteOn(60), Cc(74, 40), Cc(74, 50), Cc(74, 60) };

    VerifyWords(Pruned(words, 0), { Cc(74, 0), Cc(74, 30), NoteOn(60), Cc(74, 40), Cc(74, 60) },
        L"the value in effect when the note starts is the one that was sent");

    words[4] = Midi1(0, 0xC, 0, 5, 0);
    VerifyWords(Pruned(words, 0), { Cc(74, 0), Cc(74, 30), words[4], Cc(74, 40), Cc(74, 60) }, L"and the same for a program change");
}

void MidiSendBacklogPrunerTests::TestOtherChannelsDoNotEndALine()
{
    std::vector<uint32_t> words{ Cc(74, 0), Cc(74, 10), Cc(74, 20), Cc(74, 30), NoteOn(60, 1), Cc(74, 40), Cc(74, 50), Cc(74, 60) };

    VerifyWords(Pruned(words, 0), { Cc(74, 0), NoteOn(60, 1), Cc(74, 60) }, L"a note on another channel");

    words[4] = NoteOn(60, 0, 3);
    VerifyWords(Pruned(words, 0), { Cc(74, 0), NoteOn(60, 0, 3), Cc(74, 60) }, L"a note in another group");
}

void MidiSendBacklogPrunerTests::TestEachLineIsThinnedOnItsOwn()
{
    std::vector<uint32_t> words;

    for (uint8_t i = 0; i < 5; i++)
    {
        words.push_back(Cc(1, static_cast<uint8_t>(i * 10)));
        words.push_back(Cc(7, static_cast<uint8_t>(100 - i * 10)));
        words.push_back(Bend(static_cast<uint16_t>(i * 1000)));
        words.push_back(Cc(1, static_cast<uint8_t>(i * 10), 1));
    }

    VerifyWords(Pruned(words, 0),
        { Cc(1, 0), Cc(7, 100), Bend(0), Cc(1, 0, 1), Cc(1, 40), Cc(7, 60), Bend(4000), Cc(1, 40, 1) },
        L"four lines, each down to its ends, still in order");
}

void MidiSendBacklogPrunerTests::TestEveryMidi1ControllerNumber()
{
    for (uint32_t controller = 0; controller < 128; controller++)
    {
        auto const number = static_cast<uint8_t>(controller);

        bool const selectsOrCarries = number == 0 || number == 6 || number == 32 || number == 38 || number == 84 || number == 88 ||
            (number >= 96 && number <= 101) || number >= 120;
        bool const fineHalf = number >= 33 && number <= 63;

        std::vector<uint8_t> const expected = (selectsOrCarries || fineHalf) ? std::vector<uint8_t>{ 10, 20, 30, 40, 50 } : std::vector<uint8_t>{ 10, 50 };

        VerifyValues(CcValues(Pruned(CcRamp(number, { 10, 20, 30, 40, 50 }), 0), number), expected,
            String().Format(L"control change %u", controller));

        // And whether it ends the volume line around it
        std::vector<uint8_t> const around =
            number == 7 ? std::vector<uint8_t>{ 10, 64, 30, 40 } :
            (selectsOrCarries || number == 39) ? std::vector<uint8_t>{ 10, 20, 30, 40 } :
            std::vector<uint8_t>{ 10, 40 };

        VerifyValues(CcValues(Pruned({ Cc(7, 10), Cc(7, 20), Cc(number, 64), Cc(7, 30), Cc(7, 40) }, 0), 7), around,
            String().Format(L"volume around control change %u", controller));
    }
}

void MidiSendBacklogPrunerTests::TestEveryMidi2ControllerNumber()
{
    for (uint32_t controller = 0; controller < 128; controller++)
    {
        auto const number = static_cast<uint8_t>(controller);

        bool const selectsOrCarries = number == 0 || number == 6 || number == 32 || number == 38 || number == 84 || number == 88 ||
            (number >= 96 && number <= 101) || number >= 120;
        bool const fineHalf = number >= 33 && number <= 63;

        std::vector<uint32_t> words;

        for (uint32_t step = 1; step <= 5; step++)
        {
            AddMidi2(words, 0, 0xB, 0, number, 0, step * 0x10000000u);
        }

        MidiSendBacklogPruneResult result{};
        Pruned(words, 0, &result);

        VERIFY_ARE_EQUAL(result.MessagesRemoved, (selectsOrCarries || fineHalf) ? 0u : 3u, String().Format(L"MIDI 2.0 control change %u", controller));
    }
}

void MidiSendBacklogPrunerTests::TestEveryMidi1ChannelVoiceStatus()
{
    for (uint32_t status = 0; status < 16; status++)
    {
        std::vector<uint32_t> words;

        for (uint8_t step = 1; step <= 5; step++)
        {
            auto const value = static_cast<uint8_t>(step * 10);

            // channel pressure carries its value in the first data byte
            words.push_back(status == 0xD ?
                Midi1(0, static_cast<uint8_t>(status), 0, value, 0) :
                Midi1(0, static_cast<uint8_t>(status), 0, 7, value));
        }

        bool const isLine = status == 0xA || status == 0xB || status == 0xD || status == 0xE;

        MidiSendBacklogPruneResult result{};
        Pruned(words, 0, &result);

        VERIFY_ARE_EQUAL(result.MessagesRemoved, isLine ? 3u : 0u, String().Format(L"MIDI 1.0 status %X", status));

        // And whether it ends the pitch bend line around it
        Pruned({ Bend(1000), Bend(2000), words[4], Bend(3000), Bend(4000) }, 0, &result);

        VERIFY_ARE_EQUAL(result.MessagesRemoved, status == 0xE ? 1u : isLine ? 2u : 0u, String().Format(L"pitch bend around MIDI 1.0 status %X", status));
    }
}

void MidiSendBacklogPrunerTests::TestEveryMidi2ChannelVoiceStatus()
{
    for (uint32_t status = 0; status < 16; status++)
    {
        std::vector<uint32_t> words;

        for (uint32_t step = 1; step <= 5; step++)
        {
            AddMidi2(words, 0, static_cast<uint8_t>(status), 0, 7, 5, step * 0x10000000u);
        }

        // per-note controllers, assignable controllers, per-note pitch bend, poly pressure,
        // control change, channel pressure, pitch bend. Registered and relative controllers,
        // notes, program change and per-note management never.
        bool const isLine = status == 0x0 || status == 0x1 || status == 0x3 || status == 0x6 ||
            status == 0xA || status == 0xB || status == 0xD || status == 0xE;

        MidiSendBacklogPruneResult result{};
        Pruned(words, 0, &result);

        VERIFY_ARE_EQUAL(result.MessagesRemoved, isLine ? 3u : 0u, String().Format(L"MIDI 2.0 status %X", status));

        // And whether it ends the pitch bend line around it
        std::vector<uint32_t> around;
        AddMidi2(around, 0, 0xE, 0, 0, 0, 0x10000000u);
        AddMidi2(around, 0, 0xE, 0, 0, 0, 0x20000000u);
        around.push_back(words[8]);
        around.push_back(words[9]);
        AddMidi2(around, 0, 0xE, 0, 0, 0, 0x30000000u);
        AddMidi2(around, 0, 0xE, 0, 0, 0, 0x40000000u);

        Pruned(around, 0, &result);

        VERIFY_ARE_EQUAL(result.MessagesRemoved, status == 0xE ? 1u : isLine ? 2u : 0u, String().Format(L"pitch bend around MIDI 2.0 status %X", status));
    }
}

void MidiSendBacklogPrunerTests::TestFourteenBitControllerIsNotThinned()
{
    std::vector<uint32_t> words;

    for (uint8_t value = 10; value <= 50; value += 10)
    {
        words.push_back(Cc(1, value));
        words.push_back(Cc(33, 0x40));
    }

    VerifyWords(Pruned(words, 0), words, L"coarse and fine halves together are left alone");

    VerifyWords(Pruned({ Cc(1, 10), Cc(1, 20), Cc(1, 30), Cc(33, 5), Cc(1, 40), Cc(1, 50), Cc(1, 60) }, 0),
        { Cc(1, 10), Cc(1, 30), Cc(33, 5), Cc(1, 40), Cc(1, 60) },
        L"the coarse half on each side of a fine half is kept");

    VerifyValues(CcValues(Pruned({ Cc(1, 10), Cc(1, 20), Cc(1, 30), Cc(33, 5, 1), Cc(1, 40) }, 0), 1), { 10, 40 },
        L"a fine half on another channel changes nothing");
}

void MidiSendBacklogPrunerTests::TestSystemExclusiveEndsLinesInItsGroupOnly()
{
    std::vector<uint32_t> const sysExGroup0{ 0x30030102u, 0x03000000u };

    std::vector<uint32_t> words{ Cc(7, 10), Cc(7, 20) };
    words.insert(words.end(), sysExGroup0.begin(), sysExGroup0.end());
    words.push_back(Cc(7, 30));
    words.push_back(Cc(7, 40));

    VerifyWords(Pruned(words, 0), words, L"system exclusive in the same group");

    std::vector<uint32_t> other{ Cc(7, 10, 0, 1), Cc(7, 20, 0, 1) };
    other.insert(other.end(), sysExGroup0.begin(), sysExGroup0.end());
    other.push_back(Cc(7, 30, 0, 1));
    other.push_back(Cc(7, 40, 0, 1));

    std::vector<uint32_t> expected{ Cc(7, 10, 0, 1) };
    expected.insert(expected.end(), sysExGroup0.begin(), sysExGroup0.end());
    expected.push_back(Cc(7, 40, 0, 1));

    VerifyWords(Pruned(other, 0), expected, L"system exclusive in another group");

    std::vector<uint32_t> const reset{ Cc(7, 10), Cc(7, 20), 0x10FF0000u, Cc(7, 30), Cc(7, 40) };
    VerifyWords(Pruned(reset, 0), reset, L"a system reset");

    std::vector<uint32_t> sysEx8{ Cc(7, 10), Cc(7, 20), 0x50020000u, 0x01020000u, 0, 0, Cc(7, 30), Cc(7, 40) };
    VerifyWords(Pruned(sysEx8, 0), sysEx8, L"8-bit system exclusive");
}

void MidiSendBacklogPrunerTests::TestClockAndTextDoNotEndALine()
{
    std::vector<uint32_t> const words{
        Cc(7, 10),
        0x10F80000u,                                    // timing clock
        Cc(7, 20),
        0x00000000u,                                    // NOOP
        Cc(7, 30),
        0x00100100u,                                    // JR clock
        Cc(7, 40),
        0xD0100001u, 0x00000000u, 0x00000000u, 0x00000000u,   // flex data
        Cc(7, 50),
        0x10FE0000u,                                    // active sensing
        0x10F20102u,                                    // song position
    };

    VerifyWords(Pruned(words, 0),
        { Cc(7, 10), 0x10F80000u, 0x00000000u, 0x00100100u, 0xD0100001u, 0, 0, 0, Cc(7, 50), 0x10FE0000u, 0x10F20102u },
        L"only the steps go");
}

void MidiSendBacklogPrunerTests::TestTimestampsAndStreamMessagesEndEveryLine()
{
    struct Case { std::vector<uint32_t> Words; wchar_t const* What; };

    std::vector<Case> const cases{
        { { 0x00201234u }, L"a JR timestamp, which belongs to the message after it" },
        { { 0x00300060u }, L"delta clockstamp ticks per quarter note" },
        { { 0x00400010u }, L"a delta clockstamp" },
        { { 0xF0000101u, 0x1F000000u, 0, 0 }, L"a stream message" },
        { { 0x60000000u }, L"a reserved 32-bit type" },
        { { 0x80000000u, 0 }, L"a reserved 64-bit type" },
    };

    for (auto const& testCase : cases)
    {
        std::vector<uint32_t> words{ Cc(7, 10, 0, 5), Cc(7, 20, 0, 5) };
        words.insert(words.end(), testCase.Words.begin(), testCase.Words.end());
        words.push_back(Cc(7, 30, 0, 5));
        words.push_back(Cc(7, 40, 0, 5));

        VerifyWords(Pruned(words, 0), words, testCase.What);
    }
}

void MidiSendBacklogPrunerTests::TestPerNoteLinesAreSeparate()
{
    std::vector<uint32_t> words;
    std::vector<uint32_t> expected;

    for (uint32_t step = 0; step < 5; step++)
    {
        auto const first = words.size();

        AddMidi2(words, 0, 0xA, 0, 60, 0, step * 0x1000000u);                  // poly pressure, note 60
        AddMidi2(words, 0, 0xA, 0, 61, 0, 0xF0000000u - step * 0x1000000u);    // poly pressure, note 61
        AddMidi2(words, 0, 0x6, 0, 60, 0, 0x80000000u + step * 0x100000u);    // per-note pitch bend, note 60
        AddMidi2(words, 0, 0x0, 0, 60, 3, step * 0x2000000u);                  // registered per-note controller 3, note 60
        AddMidi2(words, 0, 0x0, 0, 60, 7, step * 0x3000000u);                  // registered per-note controller 7, note 60
        AddMidi2(words, 0, 0x1, 0, 60, 3, step * 0x4000000u);                  // assignable per-note controller 3, note 60
        AddMidi2(words, 0, 0x3, 0, 1, 2, step * 0x5000000u);                   // assignable controller, bank 1, index 2
        AddMidi2(words, 0, 0x3, 0, 2, 1, step * 0x6000000u);                   // assignable controller, bank 2, index 1

        if (step == 0 || step == 4)
        {
            expected.insert(expected.end(), words.begin() + first, words.end());
        }
    }

    VerifyWords(Pruned(words, 0), expected, L"eight lines, each down to its ends");
}

void MidiSendBacklogPrunerTests::TestOnlyMessagesFromTheStartAreTouched()
{
    std::vector<uint32_t> words{ Cc(7, 0), Cc(7, 10), Cc(7, 20), Cc(7, 30), Cc(7, 40), Cc(7, 50), Cc(7, 60) };

    MidiSendBacklogPruner pruner{ 0 };
    auto const result = pruner.PruneUmp(words, 3, 0);

    VerifyWords(words, { Cc(7, 0), Cc(7, 10), Cc(7, 20), Cc(7, 30), Cc(7, 60) }, L"what was before the start stays, and is not counted");
    VERIFY_ARE_EQUAL(result.MessagesRemoved, 2u);

    auto const again = pruner.PruneUmp(words, words.size(), 0);
    VERIFY_ARE_EQUAL(again.MessagesRemoved, 0u, L"a start at the end has nothing to look at");
}

void MidiSendBacklogPrunerTests::TestPartialMessageAtTheEndStays()
{
    VerifyWords(Pruned({ Cc(7, 10), Cc(7, 20), Cc(7, 30), Cc(7, 40), 0x40B00700u }, 0),
        { Cc(7, 10), Cc(7, 40), 0x40B00700u },
        L"the first half of a MIDI 2.0 message stays last");
}

void MidiSendBacklogPrunerTests::TestRemovedWireBytes()
{
    MidiSendBacklogPruneResult result{};
    Pruned(CcRamp(7, TenStepRamp), 0, &result);

    VERIFY_ARE_EQUAL(result.MessagesRemoved, 9u);
    VERIFY_ARE_EQUAL(result.WireBytesRemoved, 27ull, L"three bytes each on a MIDI 1.0 cable");

    std::vector<uint32_t> words;
    for (uint32_t step = 1; step <= 5; step++)
    {
        AddMidi2(words, 0, 0xD, 0, 0, 0, step * 0x10000000u);
    }

    Pruned(words, 0, &result);

    VERIFY_ARE_EQUAL(result.MessagesRemoved, 3u);
    VERIFY_ARE_EQUAL(result.WireBytesRemoved, 9ull, L"a MIDI 2.0 message counts as the MIDI 1.0 message it becomes");
}

void MidiSendBacklogPrunerTests::TestRepeatedPassesStopAtTheTurningPoints()
{
    std::vector<uint8_t> values;
    for (int value = 0; value <= 120; value++) values.push_back(static_cast<uint8_t>(value));
    for (int value = 119; value >= 0; value--) values.push_back(static_cast<uint8_t>(value));

    auto words = CcRamp(74, values);

    MidiSendBacklogPruner pruner{ 0 };
    uint32_t passes{ 0 };

    while (pruner.PruneUmp(words, 0, 2).MessagesRemoved > 0)
    {
        passes++;
        VERIFY_IS_LESS_THAN(passes, 20u, L"each pass takes away about half of what is left");
    }

    Log::Comment(String().Format(L"%u passes", passes));

    VerifyValues(CcValues(words, 74), { 0, 120, 0 }, L"in the end only the turning points are left");
}

void MidiSendBacklogPrunerTests::TestTheFirstStepsOfEachRunAlwaysGo()
{
    auto const kept = static_cast<int>(MidiSendBacklogPruneStepsKeptInEveryRun);

    std::vector<uint8_t> shortRun;
    for (int value = 0; value <= kept + 1; value++) shortRun.push_back(static_cast<uint8_t>(value));

    VerifyValues(CcValues(Pruned(CcRamp(7, shortRun), 0, nullptr, MidiSendBacklogPruneStepsKeptInEveryRun), 7), shortRun,
        L"a run with no more steps than always go");

    std::vector<uint8_t> values;
    for (int value = 0; value <= 40; value++) values.push_back(static_cast<uint8_t>(value));
    for (int value = 39; value >= 0; value--) values.push_back(static_cast<uint8_t>(value));

    std::vector<uint8_t> expected;
    for (int value = 0; value <= kept; value++) expected.push_back(static_cast<uint8_t>(value));
    expected.push_back(40);
    for (int value = 39; value > 39 - kept; value--) expected.push_back(static_cast<uint8_t>(value));
    expected.push_back(0);

    VerifyValues(CcValues(Pruned(CcRamp(7, values), 0, nullptr, MidiSendBacklogPruneStepsKeptInEveryRun), 7), expected,
        L"each way, the first steps after the start or the turn, then only the turn and the end");

    std::deque<uint8_t> bytes;
    std::deque<uint32_t> sizes;

    for (auto const value : values)
    {
        bytes.insert(bytes.end(), { uint8_t{ 0xB0 }, uint8_t{ 7 }, value });
        sizes.push_back(3);
    }

    MidiSendBacklogPruner pruner;
    pruner.PruneMidi1(bytes, sizes, 0);

    std::vector<uint8_t> left;
    for (size_t i = 2; i < bytes.size(); i += 3) left.push_back(bytes[i]);

    VerifyValues(left, expected, L"the same from the MIDI 1.0 bytes RTP-MIDI queues");
}

void MidiSendBacklogPrunerTests::TestRepeatedPassesKeepTheFirstStepsOfEachRun()
{
    std::vector<uint8_t> values;
    for (int value = 0; value <= 120; value++) values.push_back(static_cast<uint8_t>(value));
    for (int value = 119; value >= 0; value--) values.push_back(static_cast<uint8_t>(value));

    auto words = CcRamp(74, values);

    MidiSendBacklogPruner pruner;
    uint32_t passes{ 0 };

    while (pruner.PruneUmp(words, 0, 2).MessagesRemoved > 0)
    {
        passes++;
        VERIFY_IS_LESS_THAN(passes, 20u, L"each pass takes away about half of what can go");
    }

    auto const kept = static_cast<int>(MidiSendBacklogPruneStepsKeptInEveryRun);

    std::vector<uint8_t> expected;
    for (int value = 0; value <= kept; value++) expected.push_back(static_cast<uint8_t>(value));
    expected.push_back(120);
    for (int value = 119; value > 119 - kept; value--) expected.push_back(static_cast<uint8_t>(value));
    expected.push_back(0);

    VerifyValues(CcValues(words, 74), expected, L"in the end, the turning points and the first steps each way");
}

void MidiSendBacklogPrunerTests::TestMackieControlMetersAreNotThinned()
{
    // A DAW sends a Mackie Control surface its meters as channel pressure on the first channel:
    // the strip in the high bits, the level in the low four, one strip after another.
    std::vector<uint32_t> words;

    for (uint32_t frame = 0; frame < 50; frame++)
    {
        for (uint32_t strip = 0; strip < 8; strip++)
        {
            auto const level = (frame * 3 + strip * 5) % 13;
            words.push_back(Midi1(0, 0xD, 0, static_cast<uint8_t>((strip << 4) | level), 0));
        }
    }

    for (uint32_t const keepOneIn : { 0u, 2u, 8u })
    {
        MidiSendBacklogPruneResult result{};
        Pruned(words, keepOneIn, &result, MidiSendBacklogPruneStepsKeptInEveryRun);

        VERIFY_ARE_EQUAL(result.MessagesRemoved, 0u, String().Format(L"keep one in %u", keepOneIn));
    }

    MidiSendBacklogPruneResult result{};
    Pruned(words, 0, &result);

    VERIFY_IS_GREATER_THAN(result.MessagesRemoved, 0u, L"without the steps that always go, the strips in the middle would miss levels");
}

void MidiSendBacklogPrunerTests::TestAcceleratingRelativeEncodersAreNotThinned()
{
    // Each message is how far the knob turned, so every one counts. Speeding up, then slowing to
    // a stop, one way and then the other.
    std::vector<int> turns;
    for (int speed = 1; speed <= 8; speed++) turns.push_back(speed);
    for (int speed = 8; speed >= 1; speed--) turns.push_back(speed);
    for (int speed = 1; speed <= 8; speed++) turns.push_back(-speed);
    for (int speed = 8; speed >= 1; speed--) turns.push_back(-speed);

    struct Encoding
    {
        wchar_t const* Name;
        std::function<uint8_t(int)> Value;
    };

    std::vector<Encoding> const encodings{
        { L"sign and magnitude, as Mackie Control V-Pots send", [](int turn) { return static_cast<uint8_t>(turn > 0 ? turn : (0x40 | -turn)); } },
        { L"offset from 64", [](int turn) { return static_cast<uint8_t>(64 + turn); } },
        { L"two's complement", [](int turn) { return static_cast<uint8_t>(turn & 0x7F); } },
    };

    for (auto const& encoding : encodings)
    {
        std::vector<uint8_t> values;

        for (int repeat = 0; repeat < 20; repeat++)
        {
            for (auto const turn : turns)
            {
                values.push_back(encoding.Value(turn));
            }
        }

        auto const words = CcRamp(16, values);

        for (uint32_t const keepOneIn : { 0u, 2u })
        {
            MidiSendBacklogPruneResult result{};
            Pruned(words, keepOneIn, &result, MidiSendBacklogPruneStepsKeptInEveryRun);

            VERIFY_ARE_EQUAL(result.MessagesRemoved, 0u, String().Format(L"%s, keep one in %u", encoding.Name, keepOneIn));
        }

        MidiSendBacklogPruneResult result{};
        Pruned(words, 0, &result);

        VERIFY_IS_GREATER_THAN(result.MessagesRemoved, 0u, String().Format(L"%s: without the steps that always go, turns would be lost", encoding.Name));
    }
}

void MidiSendBacklogPrunerTests::TestMidi1BytesAsRtpMidiQueuesThem()
{
    std::vector<std::vector<uint8_t>> const messages{
        { 0xE0, 0x00, 0x00 },
        { 0xE0, 0x00, 0x10 },
        { 0xE0, 0x00, 0x20 },
        { 0x90, 0x3C, 0x64 },
        { 0xE0, 0x00, 0x30 },
        { 0xE0, 0x00, 0x40 },
        { 0xE0, 0x00, 0x50 },
        { 0xF0, 0x01, 0x02 },
        { 0x03, 0x04 },
        { 0x05, 0xF7 },
        { 0xE0, 0x00, 0x60 },
        { 0xE0, 0x00, 0x70 },
        { 0xF8 },
        { 0xE0, 0x00, 0x7F },
        { 0xD1, 0x10 },
        { 0xD1, 0x20 },
        { 0xD1, 0x30 },
        { 0xB2, 0x65, 0x00, 0xB2, 0x64, 0x00, 0xB2, 0x06, 0x02, 0xB2, 0x26, 0x00 },
        { 0xD1, 0x40 },
    };

    std::deque<uint8_t> bytes;
    std::deque<uint32_t> sizes;

    for (auto const& message : messages)
    {
        bytes.insert(bytes.end(), message.begin(), message.end());
        sizes.push_back(static_cast<uint32_t>(message.size()));
    }

    MidiSendBacklogPruner pruner{ 0 };
    auto const result = pruner.PruneMidi1(bytes, sizes, 0);

    // A note, then system exclusive, end the pitch bend lines. The registered parameter on
    // channel 3 does not end channel 2's pressure.
    std::vector<size_t> const removed{ 1, 5, 11, 15, 16 };

    std::vector<uint8_t> expectedBytes;
    std::vector<uint32_t> expectedSizes;

    for (size_t i = 0; i < messages.size(); i++)
    {
        if (std::find(removed.begin(), removed.end(), i) == removed.end())
        {
            expectedBytes.insert(expectedBytes.end(), messages[i].begin(), messages[i].end());
            expectedSizes.push_back(static_cast<uint32_t>(messages[i].size()));
        }
    }

    VERIFY_ARE_EQUAL(result.MessagesRemoved, static_cast<uint32_t>(removed.size()));
    VERIFY_ARE_EQUAL(result.WireBytesRemoved, 3ull + 3 + 3 + 2 + 2);
    VERIFY_IS_TRUE(std::vector<uint8_t>(bytes.begin(), bytes.end()) == expectedBytes, L"the bytes left are the messages left");
    VERIFY_IS_TRUE(std::vector<uint32_t>(sizes.begin(), sizes.end()) == expectedSizes, L"and so are the sizes");

    // The same parameter on the pressure's own channel ends it
    std::deque<uint8_t> sameChannel{ 0xD1, 0x10, 0xD1, 0x20, 0xB1, 0x65, 0x00, 0xB1, 0x64, 0x00, 0xB1, 0x06, 0x02, 0xD1, 0x30, 0xD1, 0x40 };
    std::deque<uint32_t> sameChannelSizes{ 2, 2, 9, 2, 2 };

    VERIFY_ARE_EQUAL(pruner.PruneMidi1(sameChannel, sameChannelSizes, 0).MessagesRemoved, 0u, L"several messages made from one end the lines on their channel");
}

void MidiSendBacklogPrunerTests::TestMidi1BytesThatDoNotAddUpAreLeftAlone()
{
    std::deque<uint8_t> const original{ 0xE0, 0x00, 0x00, 0xE0, 0x00, 0x10, 0xE0, 0x00, 0x20, 0xE0, 0x00, 0x30 };

    MidiSendBacklogPruner pruner{ 0 };

    {
        auto bytes = original;
        std::deque<uint32_t> sizes{ 3, 3, 3, 4 };

        VERIFY_ARE_EQUAL(pruner.PruneMidi1(bytes, sizes, 0).MessagesRemoved, 0u, L"sizes past the end of the bytes");
        VERIFY_IS_TRUE(bytes == original);
    }

    {
        auto bytes = original;
        std::deque<uint32_t> sizes{ 3, 0, 3, 3, 3 };

        VERIFY_ARE_EQUAL(pruner.PruneMidi1(bytes, sizes, 0).MessagesRemoved, 0u, L"a message of no bytes");
        VERIFY_IS_TRUE(bytes == original);
    }

    {
        auto bytes = original;
        std::deque<uint32_t> sizes{ 3, 3, 3, 3 };

        VERIFY_ARE_EQUAL(pruner.PruneMidi1(bytes, sizes, 0).MessagesRemoved, 2u, L"and once they do add up, it thins as usual");
    }
}


namespace
{
    // Random traffic where every message says what it is, checked against the plainest possible
    // reading of the rules: list each line's points between whatever ends it, then take out the
    // ones strictly between their neighbors, after the first steps each way.
    enum class Role { Point, EndChannel, EndGroup, EndAll, EndLine, Other };

    struct RandomMessage
    {
        std::vector<uint32_t> Words;
        Role What{ Role::Other };
        size_t Line{ 0 };
        uint32_t Value{ 0 };
        uint8_t Group{ 0 };
        uint8_t Channel{ 0 };
    };

    struct RandomLine
    {
        uint8_t Group;
        uint8_t Channel;
        uint32_t Maximum;
        std::function<std::vector<uint32_t>(uint32_t)> Make;
    };

    std::vector<RandomLine> RandomLines()
    {
        return {
            { 0, 0, 127, [](uint32_t v) { return std::vector<uint32_t>{ Cc(1, static_cast<uint8_t>(v)) }; } },
            { 0, 0, 127, [](uint32_t v) { return std::vector<uint32_t>{ Cc(7, static_cast<uint8_t>(v)) }; } },
            { 0, 0, 16383, [](uint32_t v) { return std::vector<uint32_t>{ Bend(static_cast<uint16_t>(v)) }; } },
            { 0, 1, 127, [](uint32_t v) { return std::vector<uint32_t>{ Midi1(0, 0xD, 1, static_cast<uint8_t>(v), 0) }; } },
            { 0, 1, 127, [](uint32_t v) { return std::vector<uint32_t>{ Midi1(0, 0xA, 1, 60, static_cast<uint8_t>(v)) }; } },
            { 1, 0, 65535, [](uint32_t v) { std::vector<uint32_t> w; AddMidi2(w, 1, 0xE, 0, 0, 0, v << 16); return w; } },
            { 1, 0, 65535, [](uint32_t v) { std::vector<uint32_t> w; AddMidi2(w, 1, 0x3, 0, 1, 2, v << 16); return w; } },
            { 2, 3, 127, [](uint32_t v) { return std::vector<uint32_t>{ Cc(74, static_cast<uint8_t>(v), 3, 2) }; } },
        };
    }

    std::vector<RandomMessage> RandomTraffic(_In_ uint32_t const seed, _In_ size_t const count)
    {
        auto const lines = RandomLines();

        std::mt19937 random{ seed };
        std::vector<uint32_t> values(lines.size());
        std::vector<int> directions(lines.size(), 1);

        for (size_t i = 0; i < lines.size(); i++)
        {
            values[i] = lines[i].Maximum / 2;
        }

        std::vector<RandomMessage> messages;

        while (messages.size() < count)
        {
            RandomMessage message{};

            if (random() % 100 < 85)
            {
                auto const line = random() % lines.size();
                auto const maximum = static_cast<int64_t>(lines[line].Maximum);
                auto const roll = random() % 100;

                if (roll >= 70 && roll < 85)
                {
                    directions[line] = -directions[line];
                }

                if (roll < 85)
                {
                    auto const step = 1 + static_cast<int64_t>(random() % (std::max)(1u, lines[line].Maximum / 40));
                    auto next = static_cast<int64_t>(values[line]) + directions[line] * step;

                    if (next < 0 || next > maximum)
                    {
                        directions[line] = -directions[line];
                        next = static_cast<int64_t>(values[line]) + directions[line] * step;
                    }

                    values[line] = static_cast<uint32_t>((std::clamp)(next, int64_t{ 0 }, maximum));
                }

                message.Words = lines[line].Make(values[line]);
                message.What = Role::Point;
                message.Line = line;
                message.Value = values[line];
            }
            else
            {
                switch (random() % 9)
                {
                case 0: message = { { NoteOn(60) }, Role::EndChannel, 0, 0, 0, 0 }; break;
                case 1: message = { { Midi1(0, 0x8, 1, 60, 0) }, Role::EndChannel, 0, 0, 0, 1 }; break;
                case 2: message = { { 0x41C00000u, 0x05000000u }, Role::EndChannel, 0, 0, 1, 0 }; break;
                case 3: message = { { 0x30030102u, 0x03000000u }, Role::EndGroup, 0, 0, 0, 0 }; break;
                case 4: message = { { 0x32030102u, 0x03000000u }, Role::EndGroup, 0, 0, 2, 0 }; break;
                case 5: message = { { 0x00201234u }, Role::EndAll }; break;
                case 6: message = { { Cc(33, 0x40) }, Role::EndLine, 0 }; break;
                case 7: message = { { 0x10F80000u }, Role::Other }; break;
                default: message = { { NoteOn(60, 5, 2) }, Role::EndChannel, 0, 0, 2, 5 }; break;
                }
            }

            messages.push_back(std::move(message));
        }

        return messages;
    }

    std::vector<bool> ReferencePruned(
        _In_ std::vector<RandomMessage> const& messages,
        _In_ uint32_t const keepOneIn,
        _In_ uint32_t const stepsKeptInEveryRun)
    {
        auto const lines = RandomLines();

        std::vector<bool> pruned(messages.size(), false);
        std::vector<std::vector<std::pair<size_t, uint32_t>>> segments(lines.size());

        auto const close = [&](size_t const line)
        {
            auto& points = segments[line];
            uint32_t step{ 0 };
            uint32_t sinceTurn{ 0 };

            for (size_t i = 1; i + 1 < points.size(); i++)
            {
                auto const before = points[i - 1].second;
                auto const value = points[i].second;
                auto const after = points[i + 1].second;

                if (!((before < value && value < after) || (before > value && value > after)))
                {
                    sinceTurn = 0;
                    continue;
                }

                if (sinceTurn++ < stepsKeptInEveryRun)
                {
                    continue;
                }

                if (keepOneIn == 0 || (keepOneIn != 1 && step % keepOneIn != keepOneIn - 1))
                {
                    pruned[points[i].first] = true;
                }

                step++;
            }

            points.clear();
        };

        for (size_t i = 0; i < messages.size(); i++)
        {
            auto const& message = messages[i];

            for (size_t line = 0; line < lines.size(); line++)
            {
                bool const ends =
                    (message.What == Role::EndAll) ||
                    (message.What == Role::EndGroup && lines[line].Group == message.Group) ||
                    (message.What == Role::EndChannel && lines[line].Group == message.Group && lines[line].Channel == message.Channel) ||
                    (message.What == Role::EndLine && line == message.Line);

                if (ends)
                {
                    close(line);
                }
            }

            if (message.What == Role::Point)
            {
                segments[message.Line].push_back({ i, message.Value });
            }
        }

        for (size_t line = 0; line < lines.size(); line++)
        {
            close(line);
        }

        return pruned;
    }
}

void MidiSendBacklogPrunerTests::TestMatchesASimpleReferenceOnRandomTraffic()
{
    // With no steps always sent, and with a few, which random traffic often runs past
    for (uint32_t const stepsKept : { 0u, 3u })
    {
        MidiSendBacklogPruner pruner{ stepsKept };

        for (uint32_t seed = 1; seed <= 6; seed++)
        {
            auto const messages = RandomTraffic(seed, 4000);

            std::vector<uint32_t> words;
            for (auto const& message : messages)
            {
                words.insert(words.end(), message.Words.begin(), message.Words.end());
            }

            for (uint32_t const keepOneIn : { 0u, 2u, 4u, 8u, 1u })
            {
                auto const expected = ReferencePruned(messages, keepOneIn, stepsKept);

                VERIFY_IS_TRUE(pruner.Begin(keepOneIn, messages.size()));

                for (auto const& message : messages)
                {
                    pruner.AddUmp(message.Words.data(), static_cast<uint32_t>(message.Words.size()));
                }

                size_t mismatches{ 0 };
                size_t expectedCount{ 0 };
                std::vector<uint32_t> expectedWords;

                for (size_t i = 0; i < messages.size(); i++)
                {
                    if (pruner.IsPruned(i) != expected[i])
                    {
                        if (mismatches++ < 5)
                        {
                            Log::Error(String().Format(L"%u steps kept, seed %u, keep one in %u: message %zu (%s) pruned %d, expected %d",
                                stepsKept, seed, keepOneIn, i, Text(messages[i].Words).c_str(), pruner.IsPruned(i), static_cast<int>(expected[i])));
                        }
                    }

                    if (expected[i])
                    {
                        expectedCount++;
                    }
                    else
                    {
                        expectedWords.insert(expectedWords.end(), messages[i].Words.begin(), messages[i].Words.end());
                    }
                }

                auto const what = String().Format(L"%u steps kept, seed %u, keep one in %u: %zu of %zu removed",
                    stepsKept, seed, keepOneIn, expectedCount, messages.size());

                VERIFY_ARE_EQUAL(mismatches, size_t{ 0 }, what);
                VERIFY_ARE_EQUAL(static_cast<size_t>(pruner.PrunedCount()), expectedCount);

                auto thinned = words;
                auto const result = pruner.PruneUmp(thinned, 0, keepOneIn);

                VERIFY_ARE_EQUAL(static_cast<size_t>(result.MessagesRemoved), expectedCount);
                VERIFY_IS_TRUE(thinned == expectedWords, L"the queue holds exactly the messages kept, in order");

                if (keepOneIn == 0)
                {
                    Log::Comment(what);
                }
            }
        }
    }
}

void MidiSendBacklogPrunerTests::TestLargeBacklogIsQuick()
{
    auto const messages = RandomTraffic(42, 250'000);

    std::vector<uint32_t> words;
    for (auto const& message : messages)
    {
        words.insert(words.end(), message.Words.begin(), message.Words.end());
    }

    MidiSendBacklogPruner pruner{ 0 };

    auto const start = std::chrono::steady_clock::now();
    auto const result = pruner.PruneUmp(words, 0, 2);
    auto const milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

    Log::Comment(String().Format(L"%zu messages thinned by %u in %.1f ms", messages.size(), result.MessagesRemoved, milliseconds));

    VERIFY_IS_GREATER_THAN(result.MessagesRemoved, 0u);
    VERIFY_IS_LESS_THAN(milliseconds, 2000.0, L"a quarter of a million messages, well under a second or two");
}
