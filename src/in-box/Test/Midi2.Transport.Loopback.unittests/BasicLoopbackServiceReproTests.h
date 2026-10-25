// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The Basic Loopback create sequences that used to crash midisrv, sent to the running service.
//
// Against a service with the old transport these take MIDI down for the whole PC, so every test
// skips unless the run passes /p:"ConfirmFixedTransport=true", which says the transport the
// service loads is known to have the fix. BasicLoopbackInProcessTests covers the same ground with
// no service at all.
class BasicLoopbackServiceReproTests
    : public WEX::TestClass<BasicLoopbackServiceReproTests>
{
public:

    BEGIN_TEST_CLASS(BasicLoopbackServiceReproTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // create {G, "x"}, create {G, "y"}, create {G2, "x"}: refused, and the service keeps answering
    TEST_METHOD(TestReusedAssociationIdIsRefusedByTheService);

    // several creates of one unique id at the same moment: one is created, the rest are refused
    TEST_METHOD(TestConcurrentCreatesOfOneUniqueIdThroughTheService);
};
