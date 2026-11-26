// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "TimeTextTests.h"

#include "TimeText.h"

#include <windows.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using midiplayer::ParseTimeText;

namespace
{
    void VerifyTime(_In_ wchar_t const* text, _In_ uint64_t expectedSeconds)
    {
        auto const parsed = ParseTimeText(text);

        Log::Comment(String().Format(L"\"%s\"", text));

        VERIFY_IS_TRUE(parsed.has_value());
        VERIFY_ARE_EQUAL(expectedSeconds, parsed.value());
    }

    void VerifyNotATime(_In_ wchar_t const* text)
    {
        Log::Comment(String().Format(L"\"%s\"", text));

        VERIFY_IS_FALSE(ParseTimeText(text).has_value());
    }
}

void TimeTextTests::ReadsMinutesAndSeconds()
{
    VerifyTime(L"1:25", 85);
    VerifyTime(L"01:25", 85);
    VerifyTime(L"0:00", 0);
    VerifyTime(L"10:00", 600);
    VerifyTime(L"1:5", 65);

    // More than an hour of minutes is still minutes.
    VerifyTime(L"75:30", 4530);
}

void TimeTextTests::ReadsHoursMinutesAndSeconds()
{
    VerifyTime(L"1:02:03", 3723);
    VerifyTime(L"0:00:59", 59);
    VerifyTime(L"2:00:00", 7200);
}

void TimeTextTests::ReadsPlainSeconds()
{
    VerifyTime(L"85", 85);
    VerifyTime(L"0", 0);
    VerifyTime(L"600", 600);
}

void TimeTextTests::IgnoresSpacesAround()
{
    VerifyTime(L"  2:30 ", 150);
    VerifyTime(L"\t45\t", 45);
}

void TimeTextTests::RefusesWhatIsNotATime()
{
    VerifyNotATime(L"");
    VerifyNotATime(L"   ");
    VerifyNotATime(L"abc");
    VerifyNotATime(L"1:60");
    VerifyNotATime(L"1:60:00");
    VerifyNotATime(L"1:02:60");
    VerifyNotATime(L":30");
    VerifyNotATime(L"1:");
    VerifyNotATime(L"1::2");
    VerifyNotATime(L"1:2:3:4");
    VerifyNotATime(L"-5");
    VerifyNotATime(L"1.5");
    VerifyNotATime(L"1,5");
    VerifyNotATime(L"1 25");

    // Ten digits would let the sums overflow, and nobody means it.
    VerifyNotATime(L"1234567890");

    // Digits from other scripts are not read as numbers.
    VerifyNotATime(L"\u0663");
}
