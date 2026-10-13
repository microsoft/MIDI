// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The header-only part of the KS aggregate transport's handling of pins added to or removed from a
// device while its endpoint is open: reading the pin map, working out what to open and close, and
// handing new maps to open endpoints. No driver, service or device is needed.
class KsaPinMapUpdateTests
    : public WEX::TestClass<KsaPinMapUpdateTests>
{
public:

    BEGIN_TEST_CLASS(KsaPinMapUpdateTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // reading the property
    TEST_METHOD(TestParseReadsTheLayoutTheEndpointManagerWrites);
    TEST_METHOD(TestParseReadsAnEmptyMap);
    TEST_METHOD(TestParseRejectsMalformedMaps);

    // what an open endpoint does with a new map
    TEST_METHOD(TestPlanOpensOnlyAddedPins);
    TEST_METHOD(TestPlanClosesOnlyRemovedPins);
    TEST_METHOD(TestPlanReplacesAGroupGivenToAnotherPin);
    TEST_METHOD(TestPlanIgnoresFilterIdCase);
    TEST_METHOD(TestPlanLastEntryForAGroupWins);

    // handing maps to open endpoints
    TEST_METHOD(TestDeliveryBeforeStartIsDroppedAndInitialWorkRuns);
    TEST_METHOD(TestMapPublishedDuringInitialWorkIsAppliedAfterIt);
    TEST_METHOD(TestPublishReachesOnlyItsEndpoint);
    TEST_METHOD(TestOlderDeliveryIsIgnored);
    TEST_METHOD(TestFailedInitialWorkLeavesNothingRegistered);
    TEST_METHOD(TestEndWaitsForADeliveryInProgress);
};
