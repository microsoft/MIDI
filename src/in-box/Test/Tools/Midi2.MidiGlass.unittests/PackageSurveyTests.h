// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// What a backup or a package is about to cost, which is the number the customer is warned with.
// A layout that names a video can be larger than everything else this app has ever written put
// together, so getting this wrong either nags about nothing or writes two gigabytes in silence.
class PackageSurveyTests
{
    BEGIN_TEST_CLASS(PackageSurveyTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(Setup);
    TEST_METHOD_CLEANUP(Cleanup);

    TEST_METHOD(ALayoutOnItsOwnIsOneSmallFile);
    TEST_METHOD(APictureBesideTheLayoutIsCounted);
    TEST_METHOD(TheBiggestFileIsNamed);
    TEST_METHOD(APictureThatIsNotThereIsNotCounted);
    TEST_METHOD(APictureNamedAsAPathIsNotCounted);
    TEST_METHOD(AMissingLayoutSurveysAsNothing);
    TEST_METHOD(VideoIsCountedSeparately);
    TEST_METHOD(AStillPictureIsNotCountedAsVideo);
    TEST_METHOD(ABackupWithoutVideoLeavesTheClipOut);
    TEST_METHOD(ABackupWithoutVideoStillCarriesTheStills);
};
