// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Service-side tests for the Loopback transport's configuration manager.
//
// The SDK tests are canonical for normal workloads. These exist for the payloads the SDK can
// never produce, because it cleans and shortens everything before sending: a hand-authored
// config file is untrusted input and the transport has to defend itself.
class LoopbackConfigTests
    : public WEX::TestClass<LoopbackConfigTests>
{
public:

    BEGIN_TEST_CLASS(LoopbackConfigTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // basic behaviour through hand-rolled json
    TEST_METHOD(TestCreateAndRemoveLoopbackPair);
    TEST_METHOD(TestCreateWithMissingNameIsRejected);
    TEST_METHOD(TestCreateWithMissingUniqueIdIsRejected);
    TEST_METHOD(TestCreateWithDuplicateUniqueIdIsRejected);

    // untrusted config entries
    TEST_METHOD(TestUniqueIdWithInvalidCharactersIsRejected);
    TEST_METHOD(TestOverlongUniqueIdIsRejected);
    TEST_METHOD(TestUniqueIdInvalidOnSecondEndpointIsRejected);
    TEST_METHOD(TestOverlongUnicodeNameIsAccepted);
    TEST_METHOD(TestMalformedJsonIsRejected);
};
