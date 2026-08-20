// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ComponentSignatureCacheTests
    : public WEX::TestClass<ComponentSignatureCacheTests>
{
public:

    BEGIN_TEST_CLASS(ComponentSignatureCacheTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestSignedSystemFileIsPermitted);
    TEST_METHOD(TestRepeatedCallsAgreeWithFirstResult);
    TEST_METHOD(TestCachedResultMatchesUncachedResult);
    TEST_METHOD(TestVerifiedFileIsPinned);
    TEST_METHOD(TestMissingFileIsRejectedAndNotCached);
    TEST_METHOD(TestUnregisteredClsidIsRejected);
    TEST_METHOD(TestConcurrentCallersAllSucceed);
};
