// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class BindingEngineTests : public WEX::TestClass<BindingEngineTests>
{
public:

    BEGIN_TEST_CLASS(BindingEngineTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- scaling, which is where a layout is quietly wrong if it is wrong ----
    TEST_METHOD(ScalesToTheTopOfTheRangeNotOneShort);
    TEST_METHOD(ScalesTheMidpointWhereItBelongs);
    TEST_METHOD(ClampsAndSurvivesNonsense);

    // ---- the wire ----
    TEST_METHOD(AlwaysSendsMidi2ProtocolWhateverTheDeviceIs);
    TEST_METHOD(BuildsAMidi2ControlChangeAtFullResolution);
    TEST_METHOD(BuildsANoteAtSixteenBitVelocity);
    TEST_METHOD(SendsRegisteredControllersForTheServiceToExpand);
    TEST_METHOD(NeverScalesAProgramNumber);

    // ---- exact values, for data that is a code rather than a position ----
    TEST_METHOD(SendsMidi1ProtocolWhenTheMessageAsksForIt);
    TEST_METHOD(APadColorVelocityLandsExactlyAsTyped);
    TEST_METHOD(EverySevenBitValueSurvivesTheFractionExactly);
    TEST_METHOD(AnAbsoluteValueIsWrittenNotScaled);
    TEST_METHOD(DrivesAnApc40ClipLedFromItsOwnDocumentation);
    TEST_METHOD(AnAbsoluteValueWorksOnAWideMidi2Field);
    TEST_METHOD(ClampsAnAbsoluteValueToItsField);

    // ---- ranges ----
    TEST_METHOD(AFaderLimitedToSevenBitsQuantizesOntoWholeNumbers);
    TEST_METHOD(AButtonIsJustTheTwoEndsOfARange);
    TEST_METHOD(TheTwoEndsCanUseDifferentUnits);
    TEST_METHOD(AMinimumAboveAMaximumInvertsTheControl);

    // ---- the editor's preview of what a MIDI 1.0 device receives ----
    TEST_METHOD(PreviewsPitchBendWithTheLowByteFirst);
    TEST_METHOD(DoesNotPretendToPreviewAnRpnExpansion);

    // ---- routing ----
    TEST_METHOD(ResolvesDeviceNamesToIndexesOnce);
    TEST_METHOD(SkipsAMessageWhoseDeviceIsMissing);
    TEST_METHOD(StopsSendingWhenADeviceGoesAwayAndResumesWhenItReturns);
    TEST_METHOD(SendsOnlyTheMatchingTrigger);
    TEST_METHOD(SendsToSeveralDevicesFromOneControl);

    // ---- startup values ----
    TEST_METHOD(SendsStartupValuesInKeyboardOrder);
    TEST_METHOD(TheGlobalOverrideSuppressesEveryStartupValue);

    // ---- feedback ----
    TEST_METHOD(FeedbackMovesAControlFromADevice);
    TEST_METHOD(FeedbackIgnoresAMessageNobodyWants);

    // ---- the hot path ----
    TEST_METHOD(EvaluateWritesNoMoreThanTheCallerAllowed);
};
