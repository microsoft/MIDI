// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "VirtualMidiConfigTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace TransportConfigTest;

namespace
{
    // {8FEAAD91-70E1-4A19-997A-377720A719C1}
    constexpr GUID VirtualMidiTransportId
    {
        0x8FEAAD91, 0x70E1, 0x4A19, { 0x99, 0x7A, 0x37, 0x77, 0x20, 0xA7, 0x19, 0xC1 }
    };

    constexpr wchar_t VirtualMidiTransportIdString[]{ L"{8FEAAD91-70E1-4A19-997A-377720A719C1}" };


    ServiceConfigResult SendVirtualConfig(std::wstring const& json)
    {
        return SendTransportConfig(VirtualMidiTransportId, VirtualMidiTransportIdString, json);
    }


    bool VirtualAvailable()
    {
        // this transport takes an array rather than an object for create
        auto result = SendVirtualConfig(L"{\"create\":[]}");

        return result.CallSucceeded;
    }


    // create is an array of entries, unlike loopback which keys them by association id
    std::wstring BuildCreateJson(
        std::wstring const& associationId,
        std::wstring const& name,
        std::wstring const& uniqueId)
    {
        return
            L"{\"create\":[{"
            L"\"associationIdentifier\":\"" + EscapeJsonString(associationId) + L"\","
            L"\"name\":\"" + EscapeJsonString(name) + L"\","
            L"\"description\":\"Service test virtual device\","
            L"\"uniqueIdentifier\":\"" + EscapeJsonString(uniqueId) + L"\""
            L"}]}";
    }
}


void VirtualMidiConfigTests::TestEmptyCreateArrayIsAccepted()
{
    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    // a no-op payload the transport still has to answer cleanly
    auto result = SendVirtualConfig(L"{\"create\":[]}");

    VERIFY_IS_TRUE(result.CallSucceeded);
}


void VirtualMidiConfigTests::TestMalformedJsonIsRejected()
{
    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    // not valid json at all
    auto truncated = SendRawServiceConfig(VirtualMidiTransportId, L"{\"endpointTransportPluginSettings\":");
    VERIFY_IS_FALSE(truncated.IsSuccess());

    // create present but the wrong type
    auto wrongType = SendVirtualConfig(L"{\"create\":12345}");
    VERIFY_IS_FALSE(wrongType.IsSuccess());

    // an array holding a number where an object is expected
    auto wrongElement = SendVirtualConfig(L"{\"create\":[12345]}");
    VERIFY_IS_FALSE(wrongElement.IsSuccess());

    // the transport has to still be answering after all three
    VERIFY_IS_TRUE(VirtualAvailable());
}


void VirtualMidiConfigTests::TestUniqueIdWithInvalidCharactersIsRejected()
{
    // The behaviour under test is KIR-gated, so the test has to no-op when the KIR is off,
    // otherwise a rollback turns this suite red.
    if (!Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointUniqueIdValidation is disabled.");
        return;
    }

    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    auto result = SendVirtualConfig(
        BuildCreateJson(MakeGuidString(), L"Service Test Virtual", L"bad id with spaces"));

    VERIFY_IS_FALSE(result.IsSuccess());
}


void VirtualMidiConfigTests::TestUniqueIdWithPathCharactersIsRejected()
{
    if (!Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointUniqueIdValidation is disabled.");
        return;
    }

    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    // separators and wildcards are the characters which matter here, because this value ends up
    // inside a device instance id
    for (auto const& dirty : { L"..\\..\\escape", L"id/with/slashes", L"wild*card?id", L"semi;colon" })
    {
        auto result = SendVirtualConfig(
            BuildCreateJson(MakeGuidString(), L"Service Test Virtual", dirty));

        VERIFY_IS_FALSE(result.IsSuccess());
    }

    VERIFY_IS_TRUE(VirtualAvailable());
}


void VirtualMidiConfigTests::TestServiceSurvivesAWholeBatchOfBadEntries()
{
    if (!Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointUniqueIdValidation is disabled.");
        return;
    }

    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    // several bad entries in one payload, which is what a hand-edited config file looks like
    std::wstring json =
        L"{\"create\":["
        L"{\"associationIdentifier\":\"" + MakeGuidString() + L"\",\"name\":\"Bad One\",\"uniqueIdentifier\":\"has space\"},"
        L"{\"associationIdentifier\":\"" + MakeGuidString() + L"\",\"name\":\"Bad Two\",\"uniqueIdentifier\":\"has/slash\"},"
        L"{\"associationIdentifier\":\"" + MakeGuidString() + L"\",\"name\":\"Bad Three\",\"uniqueIdentifier\":\"has*star\"}"
        L"]}";

    auto result = SendVirtualConfig(json);

    VERIFY_IS_FALSE(result.IsSuccess());

    // the point of this one is that the service is still alive and answering afterwards
    VERIFY_IS_TRUE(VirtualAvailable());
}
