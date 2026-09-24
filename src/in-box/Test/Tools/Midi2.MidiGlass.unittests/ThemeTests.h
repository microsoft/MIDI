// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ThemeTests : public WEX::TestClass<ThemeTests>
{
public:

    BEGIN_TEST_CLASS(ThemeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ShipsTheNineThemesTheDesignNames);
    TEST_METHOD(EveryBuiltInThemeFillsAllSixSlots);
    TEST_METHOD(ContrastMatchesTheWcagReferenceValues);
    TEST_METHOD(MeasuresEverySlotAgainstTheDeck);
    TEST_METHOD(EverySlotOfEveryShippedThemeIsLegible);
    TEST_METHOD(NoThemeLeavesTheTrackColorAgainstItsOwnDeck);
    TEST_METHOD(BigwigUsesANeutralRimAndOneHue);
    TEST_METHOD(TheTonalThemesTurnOffTheGlass);
    TEST_METHOD(HighContrastTurnsOffEveryEffect);
};
