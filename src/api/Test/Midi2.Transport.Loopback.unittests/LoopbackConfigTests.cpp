// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "LoopbackConfigTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace TransportConfigTest;

namespace
{
    // {942BF02D-93C0-4EA8-B03E-D51156CA75E1}
    constexpr GUID LoopbackTransportId
    {
        0x942BF02D, 0x93C0, 0x4EA8, { 0xB0, 0x3E, 0xD5, 0x11, 0x56, 0xCA, 0x75, 0xE1 }
    };

    constexpr wchar_t LoopbackTransportIdString[]{ L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}" };

    // 42 is the limit, so this is one over
    constexpr size_t OverlongUniqueIdCharacterCount = 43;


    ServiceConfigResult SendLoopbackConfig(std::wstring const& json)
    {
        return SendTransportConfig(LoopbackTransportId, LoopbackTransportIdString, json);
    }


    bool LoopbackAvailable()
    {
        return IsTransportAvailable(LoopbackTransportId, LoopbackTransportIdString);
    }


    std::wstring BuildCreateJson(
        std::wstring const& associationId,
        std::wstring const& nameA,
        std::wstring const& uniqueIdA,
        std::wstring const& nameB,
        std::wstring const& uniqueIdB)
    {
        return
            L"{\"create\":{\"" + EscapeJsonString(associationId) + L"\":{"
            L"\"endpointA\":{"
            L"\"name\":\"" + EscapeJsonString(nameA) + L"\","
            L"\"description\":\"Service test endpoint A\","
            L"\"uniqueIdentifier\":\"" + EscapeJsonString(uniqueIdA) + L"\""
            L"},"
            L"\"endpointB\":{"
            L"\"name\":\"" + EscapeJsonString(nameB) + L"\","
            L"\"description\":\"Service test endpoint B\","
            L"\"uniqueIdentifier\":\"" + EscapeJsonString(uniqueIdB) + L"\""
            L"}}}}";
    }


    ServiceConfigResult RemoveLoopback(std::wstring const& associationId)
    {
        return SendLoopbackConfig(L"{\"remove\":[\"" + EscapeJsonString(associationId) + L"\"]}");
    }


    std::wstring RepeatedString(std::wstring const& unit, uint32_t const count)
    {
        std::wstring result{ };

        for (uint32_t i = 0; i < count; i++)
        {
            result += unit;
        }

        return result;
    }
}


void LoopbackConfigTests::TestCreateAndRemoveLoopbackPair()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"Service Test A", uniqueId, L"Service Test B", uniqueId));

    VERIFY_IS_TRUE(result.CallSucceeded);
    VERIFY_IS_TRUE(result.IsSuccess());

    auto removeResult = RemoveLoopback(associationId);

    VERIFY_IS_TRUE(removeResult.CallSucceeded);
}


void LoopbackConfigTests::TestCreateWithMissingNameIsRejected()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"", uniqueId, L"Service Test B", uniqueId));

    VERIFY_IS_FALSE(result.IsSuccess());

    // nothing should have been created, but remove anyway so a failure here cannot leak
    RemoveLoopback(associationId);
}


void LoopbackConfigTests::TestCreateWithMissingUniqueIdIsRejected()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"Service Test A", L"", L"Service Test B", L""));

    VERIFY_IS_FALSE(result.IsSuccess());

    RemoveLoopback(associationId);
}


void LoopbackConfigTests::TestCreateWithDuplicateUniqueIdIsRejected()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto firstAssociationId = MakeGuidString();
    auto secondAssociationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto first = SendLoopbackConfig(
        BuildCreateJson(firstAssociationId, L"Service Test Dup A", uniqueId, L"Service Test Dup B", uniqueId));

    VERIFY_IS_TRUE(first.IsSuccess());

    auto cleanup = wil::scope_exit([&]
        {
            RemoveLoopback(firstAssociationId);
            RemoveLoopback(secondAssociationId);
        });

    // same unique id, different association. The transport comment says failing to catch this
    // crashes the service on device creation.
    auto second = SendLoopbackConfig(
        BuildCreateJson(secondAssociationId, L"Service Test Dup C", uniqueId, L"Service Test Dup D", uniqueId));

    VERIFY_IS_FALSE(second.IsSuccess());
}


void LoopbackConfigTests::TestUniqueIdWithInvalidCharactersIsRejected()
{
    // The behaviour under test is KIR-gated, so the test has to no-op when the KIR is off,
    // otherwise a rollback turns this suite red.
    if (!Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointUniqueIdValidation is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();

    // characters which are legal in json but not in a device instance id
    auto dirtyUniqueId = std::wstring{ L"bad id/with\\slashes*and?wildcards" };

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"Service Test Invalid Id A", dirtyUniqueId, L"Service Test Invalid Id B", dirtyUniqueId));

    VERIFY_IS_FALSE(result.IsSuccess());

    RemoveLoopback(associationId);
}


void LoopbackConfigTests::TestOverlongUniqueIdIsRejected()
{
    if (!Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointUniqueIdValidation is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();

    // all valid characters, just too many of them
    auto longUniqueId = RepeatedString(L"A", OverlongUniqueIdCharacterCount);

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"Service Test Long Id A", longUniqueId, L"Service Test Long Id B", longUniqueId));

    VERIFY_IS_FALSE(result.IsSuccess());

    RemoveLoopback(associationId);
}


void LoopbackConfigTests::TestUniqueIdInvalidOnSecondEndpointIsRejected()
{
    if (!Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointUniqueIdValidation is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();

    // only the B side is dirty, which is the case a check written for the A side alone misses
    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"Service Test B Only A", MakeUniqueIdString(), L"Service Test B Only B", L"dirty|id"));

    VERIFY_IS_FALSE(result.IsSuccess());

    RemoveLoopback(associationId);
}


void LoopbackConfigTests::TestOverlongUnicodeNameIsAccepted()
{
    if (!Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    // 40 three-byte characters is 120 UTF-8 bytes, over the 98 byte limit. Unlike the unique id,
    // an over-length name is truncated rather than refused, because a name is not an identity.
    auto longName = RepeatedString(L"\u8A2D", 40);

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, longName, uniqueId, longName, uniqueId));

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    VERIFY_IS_TRUE(result.IsSuccess());
}


void LoopbackConfigTests::TestMalformedJsonIsRejected()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    // not valid json at all, so it never reaches the transport's own parsing
    auto result = SendRawServiceConfig(LoopbackTransportId, L"{\"endpointTransportPluginSettings\":");

    VERIFY_IS_FALSE(result.IsSuccess());

    // a create section holding the wrong type where an object is expected
    auto wrongShape = SendLoopbackConfig(L"{\"create\":{\"" + MakeGuidString() + L"\":12345}}");

    VERIFY_IS_FALSE(wrongShape.IsSuccess());

    // the service has to still be answering after both
    VERIFY_IS_TRUE(LoopbackAvailable());
}
