// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Hostile and malformed configuration input for the General MIDI synthesizer transport.
//
// The configuration file is writable by a standard user, so everything the configuration manager
// reads is untrusted. The WinRT JSON two-argument accessors throw when a key is present with the
// wrong type, and an exception escaping a transport takes midisrv down for the whole machine, so
// a wrong-typed value is the case that matters most here.
//
// The pass condition throughout is the same: the call comes back, the bad value does not take
// effect, and the transport is still answering afterwards.
class MidiSynthConfigTests : public WEX::TestClass<MidiSynthConfigTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthConfigTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Stress", L"Stress")
    END_TEST_CLASS()

    TEST_METHOD(TestStatusCommandAnswers);
    TEST_METHOD(TestMalformedJsonIsRejected);
    TEST_METHOD(TestWrongTypesDoNotCrashTheTransport);
    TEST_METHOD(TestWrongTypeDoesNotDiscardLaterKeys);
    TEST_METHOD(TestUnknownModeStringsAreRejected);
    TEST_METHOD(TestVolumeIsClampedAndNonFiniteRejected);
    TEST_METHOD(TestUnknownKeysAreIgnored);
    TEST_METHOD(TestServiceSurvivesConfigFuzzing);
};
