// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side tests for the Virtual MIDI transport's configuration manager.
//
// The SDK tests are canonical for normal workloads. These exist for the payloads the SDK can
// never produce, because it cleans and shortens everything before sending: a hand-authored
// config file is untrusted input and the transport has to defend itself.
//
// Note on scope: this transport's configuration manager implements "create" only, with no
// removal command, so anything successfully created here would live until the service restarts.
// These tests therefore stay on the paths which create nothing.
class VirtualMidiConfigTests
    : public WEX::TestClass<VirtualMidiConfigTests>
{
public:

    BEGIN_TEST_CLASS(VirtualMidiConfigTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // basic behaviour through hand-rolled json
    TEST_METHOD(TestEmptyCreateArrayIsAccepted);
    TEST_METHOD(TestMalformedJsonIsRejected);

    // untrusted config entries
    TEST_METHOD(TestUniqueIdWithInvalidCharactersIsRejected);
    TEST_METHOD(TestUniqueIdWithPathCharactersIsRejected);
    TEST_METHOD(TestServiceSurvivesAWholeBatchOfBadEntries);
};
