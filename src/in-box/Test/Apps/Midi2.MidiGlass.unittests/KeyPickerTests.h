// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class KeyPickerTests : public WEX::TestClass<KeyPickerTests>
{
public:

    BEGIN_TEST_CLASS(KeyPickerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- what a pick does to the pads ----

    TEST_METHOD(APickedKeyReplacesTheGridsKey);
    TEST_METHOD(InKeyPadsPlayOnlyTheScaleThatIsPicked);
    TEST_METHOD(PickersShowTheFirstGridsKeyUntilOneIsTouched);

    // ---- the picker itself ----

    TEST_METHOD(EachPositionOfAPickerIsANoteOrAScale);
    TEST_METHOD(APickerSendsNothing);

    // ---- the file ----

    TEST_METHOD(PickersSurviveARoundTrip);
    TEST_METHOD(PickersFromAFileAreBounded);

    // ---- the editor ----

    TEST_METHOD(ASwitchThatStartsPickingGrowsToFitItsNames);
    TEST_METHOD(AScalePickerOffersTwoOrMoreScalesInOrder);
    TEST_METHOD(AddingPickersPutsOneOfEachBesideTheGrid);
};
