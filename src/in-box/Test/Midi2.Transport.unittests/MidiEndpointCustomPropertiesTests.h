// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Parsing tests for the shared "customProperties" section, which KS, KSAggregate and the
// network transport all feed from the configuration file.
//
// These are deliberately pure: they call the parser directly rather than going through a
// transport, so they need no MIDI driver and no service.
class MidiEndpointCustomPropertiesTests
    : public WEX::TestClass<MidiEndpointCustomPropertiesTests>
{
public:

    BEGIN_TEST_CLASS(MidiEndpointCustomPropertiesTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // The configuration file is writable by any standard user, so an image naming a path is
    // tampering and the whole set is refused rather than repaired.
    TEST_METHOD(TestBareImageFileNameIsAccepted);
    TEST_METHOD(TestMissingImageIsAccepted);
    TEST_METHOD(TestRelativePathImageIsRejected);
    TEST_METHOD(TestAbsolutePathImageIsRejected);
    TEST_METHOD(TestForwardSlashImageIsRejected);
    TEST_METHOD(TestAlternateDataStreamImageIsRejected);
    TEST_METHOD(TestWildcardImageIsRejected);

    // the ungated parser has to keep behaving exactly as it did, so a rollback is a rollback
    TEST_METHOD(TestUngatedParserStillAcceptsAPath);
};
