// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The theme engine (user-tools\theme-engine) and the Sequencer's themes (midi-sequencer\Themes):
// colors worked out the way CSS works them out, theme files checked before they're used, and the
// look of each built-in theme.
class ThemeEngineTests : public WEX::TestClass<ThemeEngineTests>
{
public:

    BEGIN_TEST_CLASS(ThemeEngineTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ColorsConvertAsCssDoes);
    TEST_METHOD(ColorMixFollowsCss);
    TEST_METHOD(ColorsAreReadAsCssWritesThem);
    TEST_METHOD(RelativeColorsTakeTheOriginsChannels);
    TEST_METHOD(BadColorsAreRefusedWithAReason);
    TEST_METHOD(StyleValuesAreReadAsCssWritesThem);
    TEST_METHOD(AThemeFileIsCheckedBeforeItIsUsed);
    TEST_METHOD(TheDarkLookStartsFromTheLightOne);
    TEST_METHOD(APartInAStateTakesWhatTheStateChanges);
    TEST_METHOD(TheWindowColorOnlyChangesTintedColors);
    TEST_METHOD(EveryBuiltInThemeLoadsAndResolves);
    TEST_METHOD(NightDriveDrawsWhatTheBuiltInPaletteDid);
    TEST_METHOD(AluminumTextIsReadableInEveryColor);
    TEST_METHOD(VellumTextIsReadableInEveryColor);
    TEST_METHOD(CanvasSizesAreWholePixels);
};
