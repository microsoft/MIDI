// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The .midiconfigpack format the MIDI Settings Configurations window writes and reads. A package
// may come from another PC or the internet, so most of these are about what a hostile or damaged
// one can't do. No service, no registry: everything happens in a temporary folder.
class ConfigPackageTests : public WEX::TestClass<ConfigPackageTests>
{
public:

    BEGIN_TEST_CLASS(ConfigPackageTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestRoundTripKeepsEverything);
    TEST_METHOD(TestConfigurationBytesAreExact);
    TEST_METHOD(TestMissingPictureIsLeftOut);
    TEST_METHOD(TestPlainConfigurationIsAccepted);
    TEST_METHOD(TestOtherJsonIsRefused);
    TEST_METHOD(TestNewerFormatIsRefused);
    TEST_METHOD(TestTooManyEntriesIsRefused);
    TEST_METHOD(TestOversizedConfigurationIsRefused);
    TEST_METHOD(TestDamagedChecksumIsRefused);
    TEST_METHOD(TestPathNamesAreNeverUsed);
    TEST_METHOD(TestUnknownEntriesAreIgnored);
    TEST_METHOD(TestPortNamingIsStoredByName);
    TEST_METHOD(TestPartsCanBeLeftOut);
};
