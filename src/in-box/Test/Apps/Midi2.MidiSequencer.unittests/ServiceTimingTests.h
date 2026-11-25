// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Phase 0 measurements against the running MIDI service, through the diagnostics loopbacks.
// Ignored by default because they need the service and take a few seconds; run them with
//   TE.exe Midi2.MidiSequencer.unittests.dll /name:*Timing* /runIgnoredTests
class ServiceTimingTests : public WEX::TestClass<ServiceTimingTests>
{
public:

    BEGIN_TEST_CLASS(ServiceTimingTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"Ignore", L"true")
    END_TEST_CLASS()

    // How late a scheduled message arrives, for each look-ahead.
    TEST_METHOD(HowLateScheduledMessagesArrive);

    // How late a thread wakes when it asks for 5 ms, with and without a high resolution timer.
    TEST_METHOD(HowLateAnEngineThreadWakes);

    // The engine, on its own thread, playing through the service to a loopback.
    TEST_METHOD(TheEnginePlaysThroughTheService);
};
