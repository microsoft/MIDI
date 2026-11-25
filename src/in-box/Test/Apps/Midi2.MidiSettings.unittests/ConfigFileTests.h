// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The MIDI Settings Configurations window's file work: the locked write, backups, restoring them,
// deleting old ones, and creating, renaming, duplicating, switching and deleting configurations.
// No service, no registry: everything happens in a temporary folder.
class ConfigFileTests : public WEX::TestClass<ConfigFileTests>
{
public:

    BEGIN_TEST_CLASS(ConfigFileTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // the write
    TEST_METHOD(TestWriteInPlaceRefusesAChangedFile);
    TEST_METHOD(TestWriteInPlaceRefusesALink);
    TEST_METHOD(TestWriteInPlaceIdenticalBytesAreUnchanged);
    TEST_METHOD(TestWriteInPlaceWaitsOutASharingViolation);

    // backups
    TEST_METHOD(TestBackupsListFullAndConfigurationOnly);
    TEST_METHOD(TestBackupNamesNeverOverwrite);
    TEST_METHOD(TestRestoreFullBackupBringsPicturesAndPortNames);
    TEST_METHOD(TestRestoreConfigurationOnlyBackup);
    TEST_METHOD(TestDeleteOldBackupsKeepsNewestOfEach);
    TEST_METHOD(TestBackupBeforeLastChange);

    // configurations
    TEST_METHOD(TestCreateRenameDuplicateSwitchDelete);
    TEST_METHOD(TestPortNamingNames);

    // sharing
    TEST_METHOD(TestShareTakesOnlyTheChosenParts);
    TEST_METHOD(TestShareBackup);
};
