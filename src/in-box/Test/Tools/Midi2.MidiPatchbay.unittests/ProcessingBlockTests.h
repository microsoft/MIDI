// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ProcessingBlockTests : public WEX::TestClass<ProcessingBlockTests>
{
public:

    BEGIN_TEST_CLASS(ProcessingBlockTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(EveryKindHasItsOwnKey);
    TEST_METHOD(ANewBlockChangesNothing);
    TEST_METHOD(NoteFilterPicksNotesOnly);
    TEST_METHOD(ControlChangeFilterPicksControllersOnly);
    TEST_METHOD(VelocityFilterNeverKeepsOutANoteOff);
    TEST_METHOD(GroupFilterLetsGrouplessMessagesThrough);
    TEST_METHOD(GroupMapMovesOnlyTheGroupsItNames);
    TEST_METHOD(MessageMaskLooksOnlyAtItsOwnSize);
    TEST_METHOD(MessageMaskNeedsEveryPlaceToMatch);
    TEST_METHOD(ATransformBlockUsesOnlyItsOwnPart);
    TEST_METHOD(SettingsSurviveTheFile);
    TEST_METHOD(BadValuesInTheFileTakeTheDefault);
    TEST_METHOD(GeneratorSettingsReadBackExactly);
    TEST_METHOD(BadGeneratorValuesTakeTheDefault);
    TEST_METHOD(TheClockDividerLetsOneInSoManyThrough);
    TEST_METHOD(TheClockDividerFollowsSongPosition);
    TEST_METHOD(OnlyTheRightChangesRestartAGenerator);
    TEST_METHOD(AnAnnotationIsOnlyText);
    TEST_METHOD(AnnotationTextAndColorAreCleanedUp);
};
