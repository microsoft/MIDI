// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

class MidiNetworkAuthenticationDigestTests
    : public WEX::TestClass<MidiNetworkAuthenticationDigestTests>
{
public:

    BEGIN_TEST_CLASS(MidiNetworkAuthenticationDigestTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    // The two worked examples from the specification. These are the only evidence we have that
    // our reading of the construction is the same as everyone else's, so they matter more than
    // anything else in this file.
    TEST_METHOD(SharedSecretDigestMatchesSpecExample);
    TEST_METHOD(UserAuthenticationDigestMatchesSpecExample);

    // Each pins down one way the construction could be misread and still look plausible.
    TEST_METHOD(DigestIsOrderDependent);
    TEST_METHOD(DigestHasNoSeparatorBetweenParts);
    TEST_METHOD(DigestExcludesAnyStringTerminator);
    TEST_METHOD(UserDigestIsNotAmbiguousAcrossTheUserNameBoundary);
    TEST_METHOD(NonAsciiUserNameIsHashedAsUtf8);

    TEST_METHOD(RejectsBadArguments);

private:

};
