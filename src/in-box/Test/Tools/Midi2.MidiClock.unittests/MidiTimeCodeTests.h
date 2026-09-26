// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class MidiTimeCodeTests : public WEX::TestClass<MidiTimeCodeTests>
{
public:

    BEGIN_TEST_CLASS(MidiTimeCodeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- frame rates ----
    TEST_METHOD(ReportsTheCountingRateForEachFrameRate);
    TEST_METHOD(OnlyDropFrameIsSlowerThanItsCountingRate);
    TEST_METHOD(QuarterFrameRateIsFourTimesTheFrameRate);

    // ---- counting ----
    TEST_METHOD(AdvancesFramesSecondsMinutesAndHours);
    TEST_METHOD(WrapsAtTwentyFourHours);
    TEST_METHOD(CountsAWholeSecondAtEveryRate);

    // ---- drop frame, which is the part that is easy to get wrong ----
    TEST_METHOD(DropFrameSkipsTwoNumbersAtTheTopOfAMinute);
    TEST_METHOD(DropFrameKeepsThemOnEveryTenthMinute);
    TEST_METHOD(DropFrameStaysWithATenthOfASecondOfTheWallClockOverAnHour);
    TEST_METHOD(DropFrameRejectsPositionsThatCannotExist);
    TEST_METHOD(OtherRatesNeverSkipANumber);

    // ---- what goes on the wire ----
    TEST_METHOD(QuarterFrameCarriesThePieceNumberInTheHighNibble);
    TEST_METHOD(EightQuarterFramesRebuildThePosition);
    TEST_METHOD(TheLastQuarterFrameCarriesTheFrameRate);
    TEST_METHOD(FullFrameCarriesTheUniversalRealTimeHeader);
    TEST_METHOD(FullFramePacksTheRateIntoTheHoursByte);

    // ---- reading and writing a position ----
    TEST_METHOD(FormatsWithASemicolonOnlyForDropFrame);
    TEST_METHOD(ParsesAFullPosition);
    TEST_METHOD(ParsesAShortPositionFromTheRight);
    TEST_METHOD(RejectsAPositionOutOfRange);
    TEST_METHOD(RoundTripsEveryFormattedPosition);
    TEST_METHOD(ParsesFrameRateNames);
};
