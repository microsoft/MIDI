// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// What Studio setup lists, and what it keeps (design section 25).
class StudioSetupTests : public WEX::TestClass<StudioSetupTests>
{
public:

    BEGIN_TEST_CLASS(StudioSetupTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ARowForEachDeviceAndGroupInTrackOrder);
    TEST_METHOD(SourcesGetRowsToo);
    TEST_METHOD(SavedChoicesLandOnTheirRows);
    TEST_METHOD(DevicesOnlyTheSettingsNameComeAfterTheMetronome);
    TEST_METHOD(TheMetronomeAndTimeCodeShareTheirDevicesRow);
    TEST_METHOD(TheFirstSavedChoiceForARowWins);
    TEST_METHOD(OnlyRowsThatGetSomethingAreKept);
    TEST_METHOD(KeptChoicesComeBackOnTheSameRows);
    TEST_METHOD(TurnedOffGroupsComeBackOnTheirRows);
    TEST_METHOD(GroupsOffSaysWhichGroupsAndWhichWay);
    TEST_METHOD(MessageGroupIsReadOnlyWhereThereIsOne);
};
