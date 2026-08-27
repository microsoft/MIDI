// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

class BleMidiValidationTests
    : public WEX::TestClass<BleMidiValidationTests>
{
public:
    BEGIN_TEST_CLASS(BleMidiValidationTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    // reading untrusted json. A wrong type here used to be able to take the service down.
    TEST_METHOD(TestSafeGetObjectRejectsEveryWrongType);
    TEST_METHOD(TestSafeGetObjectAcceptsAnObject);
    TEST_METHOD(TestSafeGetArrayRejectsEveryWrongType);
    TEST_METHOD(TestSafeGetArrayAcceptsAnArray);
    TEST_METHOD(TestSafeGetStringRejectsEveryWrongType);
    TEST_METHOD(TestSafeGetStringAcceptsAString);
    TEST_METHOD(TestSafeGetBooleanRejectsEveryWrongType);
    TEST_METHOD(TestSafeGetBooleanAcceptsABoolean);
    TEST_METHOD(TestSafeAccessorsToleratePresentButNullValues);
    TEST_METHOD(TestSafeAccessorsTolerateAMissingKey);

    // the device id is the key for every command, and comes out of a user-writable file
    TEST_METHOD(TestWellFormedDeviceIdAcceptsATwelveDigitAddress);
    TEST_METHOD(TestWellFormedDeviceIdAcceptsSeparatedForms);
    TEST_METHOD(TestWellFormedDeviceIdRejectsWrongLengths);
    TEST_METHOD(TestWellFormedDeviceIdRejectsNonHex);
    TEST_METHOD(TestWellFormedDeviceIdRejectsEmptyAndSeparatorsOnly);
    TEST_METHOD(TestWellFormedDeviceIdRejectsAVeryLongValue);

    TEST_METHOD(TestParseAddressAcceptsAFullAddress);
    TEST_METHOD(TestParseAddressIsDeliberatelyLenientAboutLength);
    TEST_METHOD(TestParseAddressRejectsNonHexAndOverlongValues);

    TEST_METHOD(TestFormatAddressAlwaysProducesTwelveUpperCaseDigits);
    TEST_METHOD(TestFormatAddressDiscardsBitsAboveFortyEight);
    TEST_METHOD(TestAddressRoundTripsThroughFormatAndParse);

    // strings which cross the wire between the service and the SDK
    TEST_METHOD(TestProtocolStringsRoundTrip);
    TEST_METHOD(TestNativeDataFormatStrings);
    TEST_METHOD(TestConnectionParameterPreferenceStringsRoundTrip);
    TEST_METHOD(TestConnectionParameterPreferenceFallsBackOnUnknownValue);

    TEST_METHOD(TestGenericDeviceNameIsMatchedWholeAndCaseInsensitively);

    // the whole point of giving the transport its own codes
    TEST_METHOD(TestEveryTransportErrorCodeIsDistinct);
};
