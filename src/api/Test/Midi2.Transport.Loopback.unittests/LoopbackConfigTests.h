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

    // basic behavior through hand-rolled json
    TEST_METHOD(TestCreateAndRemoveLoopbackPair);
    TEST_METHOD(TestCreateWithMissingNameIsRejected);
    TEST_METHOD(TestCreateWithMissingUniqueIdIsRejected);
    TEST_METHOD(TestCreateWithDuplicateUniqueIdIsRejected);

    // a loopback saved while muted has to come back muted
    TEST_METHOD(TestCreateMutedLoopbackIsMuted);
    TEST_METHOD(TestCreateWithoutMutedKeyIsNotMuted);

    // custom endpoint pictures
    TEST_METHOD(TestCreateWithImageIsReported);
    TEST_METHOD(TestCreateWithImagePathKeepsOnlyTheFileName);
    TEST_METHOD(TestTransportDeclaresImageCapability);

    // untrusted config entries
    TEST_METHOD(TestUniqueIdWithInvalidCharactersIsRejected);
    TEST_METHOD(TestOverlongUniqueIdIsRejected);
    TEST_METHOD(TestUniqueIdInvalidOnSecondEndpointIsRejected);
    TEST_METHOD(TestOverlongUnicodeNameIsAccepted);
    TEST_METHOD(TestMalformedJsonIsRejected);

    // an association id that is not a guid must be named as such, not silently turned into an
    // uninitialized value and looked up
    TEST_METHOD(TestMuteWithMalformedAssociationIdIsRejected);

    // Basic Loopback keys its endpoints by a real GUID parsed from the configuration file, so a
    // hand-edited non-guid there used to become the endpoint's identity
    TEST_METHOD(TestBasicLoopbackMalformedAssociationKeySkipsOnlyThatEntry);
};
