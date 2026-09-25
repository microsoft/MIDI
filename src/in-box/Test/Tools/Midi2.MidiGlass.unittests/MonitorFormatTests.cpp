// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The monitor rail reads a message back rather than reporting what was asked for, so these check
// decoding against words that were captured on the wire in phase 4, not against the arithmetic
// that built them.

#include "MonitorFormatTests.h"

#include "MonitorFormat.h"

#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    glass::FormattedMessage Describe(_In_ std::initializer_list<uint32_t> words)
    {
        std::vector<uint32_t> buffer{ words };

        return glass::DescribeMessage(buffer.data(), static_cast<uint32_t>(buffer.size()));
    }
}

// ---- the bytes ----

void MonitorFormatTests::OneWordIsFourBytes()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"20 B0 07 7F" }, Describe({ 0x20B0077F }).Words);
}

void MonitorFormatTests::TwoWordsAreTwoGroupsOfFour()
{
    // Two spaces between the words, so a 64 bit message reads as two groups rather than eight
    // loose bytes.
    VERIFY_ARE_EQUAL(
        std::wstring{ L"40 90 3C 00  FF FF 00 00" },
        Describe({ 0x40903C00, 0xFFFF0000 }).Words);
}

void MonitorFormatTests::NothingIsNotACrash()
{
    auto const empty = glass::DescribeMessage(nullptr, 0);

    VERIFY_IS_TRUE(empty.Words.empty());
    VERIFY_IS_TRUE(empty.Meaning.empty());
}

// ---- MIDI 1.0 in a UMP ----

void MonitorFormatTests::AMidi1ControlChangeReadsBack()
{
    // Captured in phase 4: a fader limited to 0-127 pushed to the top.
    VERIFY_ARE_EQUAL(std::wstring{ L"CC 7 = 127" }, Describe({ 0x20B0077F }).Meaning);
}

void MonitorFormatTests::AMidi1NoteOnReadsBack()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"Note on 36 = 127" }, Describe({ 0x2090247F }).Meaning);
    VERIFY_ARE_EQUAL(std::wstring{ L"Note off 36 = 0" }, Describe({ 0x20802400 }).Meaning);
}

void MonitorFormatTests::AMidi1ProgramChangeHasNoValue()
{
    // A program number is an identity, not a position, so there is nothing after it to show.
    VERIFY_ARE_EQUAL(std::wstring{ L"Program 5" }, Describe({ 0x20C00500 }).Meaning);
}

void MonitorFormatTests::AMidi1PitchBendIsSignedAroundCenter()
{
    // 0x20E00040 is the captured center: low seven bits 0, high seven bits 64, so 8192.
    VERIFY_ARE_EQUAL(std::wstring{ L"Pitch bend = 0" }, Describe({ 0x20E00040 }).Meaning);

    // All the way down is -8192.
    VERIFY_ARE_EQUAL(std::wstring{ L"Pitch bend = -8192" }, Describe({ 0x20E00000 }).Meaning);
}

// ---- MIDI 2.0 ----

void MonitorFormatTests::AMidi2ControlChangeReadsAsAFraction()
{
    // Captured in phase 4: a fader at exactly half.
    VERIFY_ARE_EQUAL(std::wstring{ L"CC 8 = 0.500" }, Describe({ 0x40B00800, 0x80000000 }).Meaning);
}

void MonitorFormatTests::AMidi2NoteOnKeepsItsSixteenBitVelocity()
{
    // A velocity is not a percentage. 65535 has to read as 65535, the way the capture showed it.
    VERIFY_ARE_EQUAL(std::wstring{ L"Note on 60 = 65535" }, Describe({ 0x40903C00, 0xFFFF0000 }).Meaning);
    VERIFY_ARE_EQUAL(std::wstring{ L"Note on 60 = 30000" }, Describe({ 0x40903C00, 0x75300000 }).Meaning);
}

void MonitorFormatTests::AMidi2ControlChangeAtFullScaleIsOne()
{
    // 1.000 rather than 0.999: a fader pushed all the way up has to read as the top, because
    // one short is the kind of thing nobody reports and everybody notices.
    VERIFY_ARE_EQUAL(std::wstring{ L"CC 7 = 1.000" }, Describe({ 0x40B00700, 0xFFFFFFFF }).Meaning);
}

void MonitorFormatTests::ARegisteredControllerNamesBothHalves()
{
    // Captured in phase 4 as 0x40200509: bank 5, index 9.
    VERIFY_ARE_EQUAL(
        std::wstring{ L"RPN 5:9 = 0.071" },
        Describe({ 0x40200509, 0x12345678 }).Meaning);
}

// ---- group and channel ----

void MonitorFormatTests::GroupAndChannelAreCountedFromOne()
{
    // A customer counts from one. Group nibble 0 and channel nibble 0 are group 1, channel 1.
    auto const first = Describe({ 0x20B0077F });

    VERIFY_ARE_EQUAL(1, first.Group);
    VERIFY_ARE_EQUAL(1, first.Channel);

    // 0x20990015 was captured as channel 10.
    auto const tenth = Describe({ 0x20990015 });

    VERIFY_ARE_EQUAL(1, tenth.Group);
    VERIFY_ARE_EQUAL(10, tenth.Channel);
}

// ---- anything else ----

void MonitorFormatTests::SystemExclusiveIsNamedNotDecoded()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"System exclusive" }, Describe({ 0x30060001, 0x02030405 }).Meaning);
}

void MonitorFormatTests::SomethingUnknownStillShowsItsBytes()
{
    // No decoder for a stream message, but the hex is still worth showing.
    auto const unknown = Describe({ 0xF0000000 });

    VERIFY_IS_TRUE(unknown.Meaning.empty());
    VERIFY_ARE_EQUAL(std::wstring{ L"F0 00 00 00" }, unknown.Words);
}

// ---- the elapsed column ----

void MonitorFormatTests::ElapsedAlwaysHasThreeDecimals()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"+0.000" }, glass::FormatElapsed(0));
    VERIFY_ARE_EQUAL(std::wstring{ L"+0.012" }, glass::FormatElapsed(12));
    VERIFY_ARE_EQUAL(std::wstring{ L"+0.900" }, glass::FormatElapsed(900));
}

void MonitorFormatTests::ElapsedCountsPastASecond()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"+1.000" }, glass::FormatElapsed(1000));
    VERIFY_ARE_EQUAL(std::wstring{ L"+12.345" }, glass::FormatElapsed(12345));
}
