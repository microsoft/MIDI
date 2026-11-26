// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The import rules of the MIDI Settings Configurations window: what a package from another PC
// does to the configuration in use, given the customer's choices. The customer's own version wins
// by default. No service, no registry: everything happens in a temporary folder.
class ConfigMergeTests : public WEX::TestClass<ConfigMergeTests>
{
public:

    BEGIN_TEST_CLASS(ConfigMergeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // device names and pictures
    TEST_METHOD(TestDefaultsKeepYourNames);
    TEST_METHOD(TestUseThePackagesNamesReplacesShared);
    TEST_METHOD(TestReplaceMineListsAndRemovesYours);
    TEST_METHOD(TestDontImportLeavesDeviceNamesAlone);
    TEST_METHOD(TestPicturesAreStoredAndRenamed);

    // loopbacks
    TEST_METHOD(TestTakenLoopbackNameIsLeftOutUnlessTicked);
    TEST_METHOD(TestSameLoopbackStaysAsItIs);
    TEST_METHOD(TestTakenAssociationAndUniqueIdsGetNewOnes);

    // network
    TEST_METHOD(TestHostNamedAfterOtherPcTakesThisPcsName);
    TEST_METHOD(TestHostKeepsOtherPcNameWhenUnticked);
    TEST_METHOD(TestTakenHostNameIsLeftOutUnlessTicked);
    TEST_METHOD(TestSameClientIsNotAddedTwice);
    TEST_METHOD(TestTakenEntryIdGetsNewIdWithItsChanges);
    TEST_METHOD(TestThisPcTransportSettingsAreKept);
    TEST_METHOD(TestRtpIsSavedWhenNotInstalled);

    // the rest
    TEST_METHOD(TestBluetoothMergesByDeviceId);
    TEST_METHOD(TestSynthModeKeepsMineByDefault);
    TEST_METHOD(TestUnknownSectionCopiedOnlyWhenAbsent);
    TEST_METHOD(TestPortNamingAutomaticNotUsableIsKept);

    // applying it
    TEST_METHOD(TestApplyImportBacksUpWritesAndUndoes);
    TEST_METHOD(TestApplyImportRefusesAChangedFile);
    TEST_METHOD(TestImportAsNewConfiguration);
};
