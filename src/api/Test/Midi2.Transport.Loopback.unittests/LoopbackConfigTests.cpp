// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "LoopbackConfigTests.h"

// the transport's own error codes, so the test asserts the exact rejection reason rather than
// just "it failed"
#include "..\..\Transport\LoopbackMidiTransport\loopback_transport_error_codes.h"

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

    // {10088473-9478-4E62-850B-3D2315E135B8}
    constexpr GUID BasicLoopbackTransportId
    {
        0x10088473, 0x9478, 0x4E62, { 0x85, 0x0B, 0x3D, 0x23, 0x15, 0xE1, 0x35, 0xB8 }
    };

    constexpr wchar_t BasicLoopbackTransportIdString[]{ L"{10088473-9478-4E62-850B-3D2315E135B8}" };

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


    // Same as above, with the muted flag on the association where the configuration file and
    // the SDK's creation config both put it.
    std::wstring BuildCreateJsonWithMuted(
        std::wstring const& associationId,
        std::wstring const& nameA,
        std::wstring const& uniqueIdA,
        std::wstring const& nameB,
        std::wstring const& uniqueIdB,
        bool const muted)
    {
        return
            L"{\"create\":{\"" + EscapeJsonString(associationId) + L"\":{"
            L"\"muted\":" + (muted ? L"true" : L"false") + L","
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


    // Same as above, with an image file name on each endpoint.
    std::wstring BuildCreateJsonWithImage(
        std::wstring const& associationId,
        std::wstring const& nameA,
        std::wstring const& uniqueIdA,
        std::wstring const& nameB,
        std::wstring const& uniqueIdB,
        std::wstring const& imageValue)
    {
        return
            L"{\"create\":{\"" + EscapeJsonString(associationId) + L"\":{"
            L"\"endpointA\":{"
            L"\"name\":\"" + EscapeJsonString(nameA) + L"\","
            L"\"description\":\"Service test endpoint A\","
            L"\"image\":\"" + EscapeJsonString(imageValue) + L"\","
            L"\"uniqueIdentifier\":\"" + EscapeJsonString(uniqueIdA) + L"\""
            L"},"
            L"\"endpointB\":{"
            L"\"name\":\"" + EscapeJsonString(nameB) + L"\","
            L"\"description\":\"Service test endpoint B\","
            L"\"image\":\"" + EscapeJsonString(imageValue) + L"\","
            L"\"uniqueIdentifier\":\"" + EscapeJsonString(uniqueIdB) + L"\""
            L"}}}}";
    }


    // The association id is a configuration file key, so the list can legitimately contain one
    // that is not a guid. Constructing a winrt::guid from it throws, which would abandon the
    // whole scan and hide the entry the test is actually looking for.
    bool ReportedAssociationMatches(winrt::hstring const& reported, winrt::guid const& wanted)
    {
        if (reported.empty())
        {
            return false;
        }

        try
        {
            return winrt::guid{ reported } == wanted;
        }
        catch (...)
        {
            return false;
        }
    }


    // Reads back what the transport reports for one association's A-side endpoint. Key names are
    // spelled out rather than taken from the transport's own headers, so this stays an
    // independent check of the wire shape.
    std::optional<std::wstring> GetReportedImage(std::wstring const& associationId)
    {
        auto result = SendLoopbackConfig(
            LR"({"transportCommand":{"commandName":"listEntries"}})");

        if (!result.IsSuccess())
        {
            return std::nullopt;
        }

        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, response) || response == nullptr)
        {
            return std::nullopt;
        }

        if (!response.HasKey(L"entries"))
        {
            return std::nullopt;
        }

        auto const wanted = winrt::guid{ associationId };

        for (auto const& value : response.GetNamedArray(L"entries"))
        {
            auto entry = value.GetObject();

            if (entry == nullptr)
            {
                continue;
            }

            auto const reported = entry.GetNamedString(L"associationIdentifier", L"");

            if (!ReportedAssociationMatches(reported, wanted))
            {
                continue;
            }

            auto endpointA = entry.GetNamedObject(L"endpointA", nullptr);

            if (endpointA == nullptr)
            {
                return std::nullopt;
            }

            return std::wstring{ endpointA.GetNamedString(L"image", L"") };
        }

        return std::nullopt;
    }

    // Reads the muted flag the transport reports for one association. Returns nothing when the
    // transport does not list the entry at all. Key names are spelled out rather than taken
    // from the transport's own headers, so this stays an independent check of the wire shape.
    std::optional<bool> GetReportedMutedState(std::wstring const& associationId)
    {
        auto result = SendLoopbackConfig(
            LR"({"transportCommand":{"commandName":"listEntries"}})");

        if (!result.IsSuccess())
        {
            return std::nullopt;
        }

        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, response) || response == nullptr)
        {
            return std::nullopt;
        }

        if (!response.HasKey(L"entries"))
        {
            return std::nullopt;
        }

        // the transport reports the association braced and lowercased, so compare on the guid
        auto const wanted = winrt::guid{ associationId };

        for (auto const& value : response.GetNamedArray(L"entries"))
        {
            auto entry = value.GetObject();

            if (entry == nullptr)
            {
                continue;
            }

            auto const reported = entry.GetNamedString(L"associationIdentifier", L"");

            if (ReportedAssociationMatches(reported, wanted))
            {
                return entry.GetNamedBoolean(L"muted", false);
            }
        }

        return std::nullopt;
    }


    ServiceConfigResult RemoveLoopback(std::wstring const& associationId)
    {
        return SendLoopbackConfig(L"{\"remove\":[\"" + EscapeJsonString(associationId) + L"\"]}");
    }


    struct ReportedSide
    {
        std::wstring EndpointDeviceId{};
        std::wstring Name{};
        std::wstring Description{};
    };

    struct ReportedPair
    {
        bool Found{ false };
        ReportedSide A{};
        ReportedSide B{};
    };

    // Reads back what the transport reports for one association. Note the list reports the id
    // under "endpointId" while the match criteria of an update wants "endpointDeviceId": two
    // different key names for the same value.
    ReportedPair GetReportedPair(std::wstring const& associationId)
    {
        ReportedPair result{};

        auto listResult = SendLoopbackConfig(LR"({"transportCommand":{"commandName":"listEntries"}})");

        if (!listResult.IsSuccess())
        {
            return result;
        }

        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(winrt::hstring{ listResult.ResponseJson }, response) ||
            response == nullptr ||
            !response.HasKey(L"entries"))
        {
            return result;
        }

        auto const wanted = winrt::guid{ associationId };

        for (auto const& value : response.GetNamedArray(L"entries"))
        {
            auto entry = value.GetObject();

            if (entry == nullptr) continue;

            if (!ReportedAssociationMatches(entry.GetNamedString(L"associationIdentifier", L""), wanted))
            {
                continue;
            }

            auto readSide = [](winrt::Windows::Data::Json::JsonObject const& side) -> ReportedSide
                {
                    ReportedSide s{};

                    if (side == nullptr) return s;

                    s.EndpointDeviceId = side.GetNamedString(L"endpointId", L"");
                    s.Name = side.GetNamedString(L"name", L"");
                    s.Description = side.GetNamedString(L"description", L"");

                    return s;
                };

            result.A = readSide(entry.GetNamedObject(L"endpointA", nullptr));
            result.B = readSide(entry.GetNamedObject(L"endpointB", nullptr));
            result.Found = true;

            break;
        }

        return result;
    }

    std::wstring BuildUpdateEntry(
        std::wstring const& endpointDeviceId,
        std::wstring const& name,
        std::wstring const& description)
    {
        return
            L"{\"match\":{\"endpointDeviceId\":\"" + EscapeJsonString(endpointDeviceId) + L"\"},"
            L"\"customProperties\":{"
            L"\"name\":\"" + EscapeJsonString(name) + L"\","
            L"\"description\":\"" + EscapeJsonString(description) + L"\"}}";
    }

    // Both sides travel in one payload, which is what lets the transport compare the two names
    // against each other before it writes either endpoint.
    ServiceConfigResult SendPairUpdate(
        ReportedPair const& pair,
        std::wstring const& nameA,
        std::wstring const& descriptionA,
        std::wstring const& nameB,
        std::wstring const& descriptionB)
    {
        return SendLoopbackConfig(
            L"{\"update\":[" +
            BuildUpdateEntry(pair.A.EndpointDeviceId, nameA, descriptionA) + L"," +
            BuildUpdateEntry(pair.B.EndpointDeviceId, nameB, descriptionB) +
            L"]}");
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


void LoopbackConfigTests::TestCreateMutedLoopbackIsMuted()
{
    // The behavior under test is KIR-gated, so the test has to no-op when the KIR is off,
    // otherwise a rollback turns this suite red.
    if (!Feature_Servicing_MIDI2LoopbackCreateMuted::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackCreateMuted is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto result = SendLoopbackConfig(
        BuildCreateJsonWithMuted(associationId, L"Service Test Muted A", uniqueId, L"Service Test Muted B", uniqueId, true));

    VERIFY_IS_TRUE(result.IsSuccess());

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    auto const muted = GetReportedMutedState(associationId);

    VERIFY_IS_TRUE(muted.has_value());
    VERIFY_IS_TRUE(muted.value());
}


void LoopbackConfigTests::TestCreateWithoutMutedKeyIsNotMuted()
{
    if (!Feature_Servicing_MIDI2LoopbackCreateMuted::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackCreateMuted is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    // an older configuration file has no muted key at all, and has to keep working
    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto result = SendLoopbackConfig(
        BuildCreateJson(associationId, L"Service Test Unmuted A", uniqueId, L"Service Test Unmuted B", uniqueId));

    VERIFY_IS_TRUE(result.IsSuccess());

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    auto const muted = GetReportedMutedState(associationId);

    VERIFY_IS_TRUE(muted.has_value());
    VERIFY_IS_FALSE(muted.value());
}


void LoopbackConfigTests::TestCreateWithImageIsReported()
{
    // The behavior under test is KIR-gated, so the test has to no-op when the KIR is off,
    // otherwise a rollback turns this suite red.
    if (!Feature_Servicing_MIDI2LoopbackCreateWithImage::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackCreateWithImage is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto result = SendLoopbackConfig(
        BuildCreateJsonWithImage(associationId, L"Service Test Image A", uniqueId, L"Service Test Image B", uniqueId, L"ep-test.png"));

    VERIFY_IS_TRUE(result.IsSuccess());

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    auto const image = GetReportedImage(associationId);

    VERIFY_IS_TRUE(image.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"ep-test.png" }, image.value());
}


void LoopbackConfigTests::TestCreateWithImagePathKeepsOnlyTheFileName()
{
    if (!Feature_Servicing_MIDI2LoopbackCreateWithImage::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackCreateWithImage is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    // The configuration file is writable by any standard user, so a relative path here must not
    // survive to whatever later joins it to the shared assets folder.
    auto associationId = MakeGuidString();
    auto uniqueId = MakeUniqueIdString();

    auto result = SendLoopbackConfig(
        BuildCreateJsonWithImage(
            associationId,
            L"Service Test Traversal A", uniqueId,
            L"Service Test Traversal B", uniqueId,
            L"..\\\\..\\\\..\\\\Windows\\\\System32\\\\evil.png"));

    VERIFY_IS_TRUE(result.IsSuccess());

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    auto const image = GetReportedImage(associationId);

    VERIFY_IS_TRUE(image.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"evil.png" }, image.value());
}


void LoopbackConfigTests::TestTransportDeclaresImageCapability()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    auto result = SendLoopbackConfig(
        LR"({"transportCommand":{"commandName":"queryCapabilities"}})");

    VERIFY_IS_TRUE(result.IsSuccess());

    // A client decides whether to offer the customer a picture based on this, so the declaration
    // has to track the KIR rather than being assumed.
    auto const expected = Feature_Servicing_MIDI2LoopbackCreateWithImage::IsEnabled() ?
        std::wstring{ L"\"createWithImage\":true" } : std::wstring{ L"\"createWithImage\":false" };

    VERIFY_IS_TRUE(result.ResponseJson.find(expected) != std::wstring::npos);
}


void LoopbackConfigTests::TestUniqueIdWithInvalidCharactersIsRejected(){
    // The behavior under test is KIR-gated, so the test has to no-op when the KIR is off,
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


void LoopbackConfigTests::TestMuteWithMalformedAssociationIdIsRejected()
{
    if (!Feature_Servicing_MIDI2TransportAssociationIdGuidValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2TransportAssociationIdGuidValidation is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    // right shape and length, but 'M' is not a hexadecimal digit
    auto result = SendLoopbackConfig(
        LR"({"transportCommand":{"commandName":"mute","commandArguments":{"associationId":"{1E5A0001-0000-4000-8000-00000000BMC1}"}}})");

    VERIFY_IS_TRUE(result.CallSucceeded);
    VERIFY_IS_FALSE(result.IsSuccess());

    // Asserting the specific code matters: an unparsed id used to become an uninitialized GUID
    // which then simply missed the lookup, so the caller saw ENDPOINT_NOT_FOUND and the failure
    // looked identical to a stale id.
    winrt::Windows::Data::Json::JsonObject response{ nullptr };

    VERIFY_IS_TRUE(winrt::Windows::Data::Json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, response));
    VERIFY_IS_NOT_NULL(response);

    auto const reportedErrorCode = static_cast<uint32_t>(response.GetNamedNumber(L"errorCode", 0));

    VERIFY_ARE_EQUAL(static_cast<uint32_t>(LOOPBACK_ERROR_CODE_INVALID_ASSOCIATION_ID), reportedErrorCode);

    // and the service is still answering
    VERIFY_IS_TRUE(LoopbackAvailable());
}


// Basic Loopback stores AssociationId as a real GUID parsed from the configuration file key, so
// this is the site where a hand-edited non-guid used to become the endpoint's own identity. A
// good entry is sent in the same batch, because rejecting the bad one must not cost the rest.
void LoopbackConfigTests::TestBasicLoopbackMalformedAssociationKeySkipsOnlyThatEntry()
{
    if (!IsTransportAvailable(BasicLoopbackTransportId, BasicLoopbackTransportIdString))
    {
        Log::Result(TestResults::Skipped, L"Basic loopback transport is not available.");
        return;
    }

    auto const goodAssociation = MakeGuidString();
    auto const goodUniqueId = MakeUniqueIdString();

    // right shape and length, but 'M' is not a hexadecimal digit
    std::wstring const badAssociation{ L"{1E5A0001-0000-4000-8000-00000000BMC1}" };

    std::wstring json =
        L"{\"create\":{"
        L"\"" + badAssociation + L"\":{\"endpoint\":{"
            L"\"name\":\"Malformed Key Test Bad\","
            L"\"description\":\"Should never be created\","
            L"\"uniqueIdentifier\":\"" + MakeUniqueIdString() + L"\"}},"
        L"\"" + goodAssociation + L"\":{\"endpoint\":{"
            L"\"name\":\"Malformed Key Test Good\","
            L"\"description\":\"Should survive the bad sibling\","
            L"\"uniqueIdentifier\":\"" + goodUniqueId + L"\"}}"
        L"}}";

    auto result = SendTransportConfig(BasicLoopbackTransportId, BasicLoopbackTransportIdString, json);

    VERIFY_IS_TRUE(result.CallSucceeded);

    // the good sibling has to exist, and the bad one must not
    auto listResult = SendTransportConfig(
        BasicLoopbackTransportId,
        BasicLoopbackTransportIdString,
        LR"({"transportCommand":{"commandName":"listEntries"}})");

    VERIFY_IS_TRUE(listResult.CallSucceeded);

    winrt::Windows::Data::Json::JsonObject response{ nullptr };

    VERIFY_IS_TRUE(winrt::Windows::Data::Json::JsonObject::TryParse(winrt::hstring{ listResult.ResponseJson }, response));
    VERIFY_IS_NOT_NULL(response);

    bool foundGood{ false };
    bool foundBad{ false };

    if (response.HasKey(L"entries"))
    {
        for (auto const& value : response.GetNamedArray(L"entries"))
        {
            auto entry = value.GetObject();

            if (entry == nullptr) continue;

            auto const name = entry.GetNamedString(L"name", L"");

            if (name == L"Malformed Key Test Good") foundGood = true;
            if (name == L"Malformed Key Test Bad") foundBad = true;
        }
    }

    // clean up before asserting, so a failure does not leave the endpoint behind
    if (foundGood)
    {
        SendTransportConfig(
            BasicLoopbackTransportId,
            BasicLoopbackTransportIdString,
            L"{\"remove\":[\"" + EscapeJsonString(goodAssociation) + L"\"]}");
    }

    VERIFY_IS_TRUE(foundGood, L"the entry with a valid association key should still have been created");
    VERIFY_IS_FALSE(foundBad, L"the entry with a malformed association key should have been skipped");
}


namespace
{
    // Creates a pair and hands back what the transport reports for it, so the update tests all
    // start from a known endpoint rather than whatever else is on the machine.
    bool SetUpPairForUpdate(std::wstring& associationId, ReportedPair& pair)
    {
        associationId = MakeGuidString();

        auto const uniqueId = MakeUniqueIdString();

        auto const created = SendLoopbackConfig(
            BuildCreateJson(associationId, L"Update Test A", uniqueId, L"Update Test B", uniqueId));

        if (!created.IsSuccess())
        {
            return false;
        }

        pair = GetReportedPair(associationId);

        return pair.Found &&
            !pair.A.EndpointDeviceId.empty() &&
            !pair.B.EndpointDeviceId.empty();
    }
}


void LoopbackConfigTests::TestUpdateRenamesBothSidesOfAPair()
{
    if (!Feature_Servicing_MIDI2LoopbackEndpointCustomization::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackEndpointCustomization is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    std::wstring associationId{};
    ReportedPair pair{};

    VERIFY_IS_TRUE(SetUpPairForUpdate(associationId, pair));

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    auto const result = SendPairUpdate(
        pair,
        L"Renamed A", L"Description for A",
        L"Renamed B", L"Description for B");

    VERIFY_IS_TRUE(result.IsSuccess());

    auto const after = GetReportedPair(associationId);

    VERIFY_IS_TRUE(after.Found);

    VERIFY_ARE_EQUAL(std::wstring{ L"Renamed A" }, after.A.Name);
    VERIFY_ARE_EQUAL(std::wstring{ L"Renamed B" }, after.B.Name);

    // the two sides keep their own descriptions rather than sharing one
    VERIFY_ARE_EQUAL(std::wstring{ L"Description for A" }, after.A.Description);
    VERIFY_ARE_EQUAL(std::wstring{ L"Description for B" }, after.B.Description);

    // renaming must not disturb identity, or apps lose their connections
    VERIFY_ARE_EQUAL(pair.A.EndpointDeviceId, after.A.EndpointDeviceId);
    VERIFY_ARE_EQUAL(pair.B.EndpointDeviceId, after.B.EndpointDeviceId);
}


// The point of validating the whole batch first is that a rejected update leaves BOTH endpoints
// untouched. Checking only the return value would pass even if one side had already been written.
void LoopbackConfigTests::TestUpdateWithDuplicateNamesChangesNothing()
{
    if (!Feature_Servicing_MIDI2LoopbackEndpointCustomization::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackEndpointCustomization is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    std::wstring associationId{};
    ReportedPair pair{};

    VERIFY_IS_TRUE(SetUpPairForUpdate(associationId, pair));

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    // differing only by case, because the comparison has to be case-insensitive
    auto const result = SendPairUpdate(
        pair,
        L"Same Name", L"first",
        L"SAME NAME", L"second");

    VERIFY_IS_FALSE(result.IsSuccess());

    auto const after = GetReportedPair(associationId);

    VERIFY_IS_TRUE(after.Found);

    VERIFY_ARE_EQUAL(pair.A.Name, after.A.Name);
    VERIFY_ARE_EQUAL(pair.B.Name, after.B.Name);
    VERIFY_ARE_EQUAL(pair.A.Description, after.A.Description);
    VERIFY_ARE_EQUAL(pair.B.Description, after.B.Description);
}


void LoopbackConfigTests::TestUpdateWithBlankNameIsRejected()
{
    if (!Feature_Servicing_MIDI2LoopbackEndpointCustomization::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackEndpointCustomization is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    std::wstring associationId{};
    ReportedPair pair{};

    VERIFY_IS_TRUE(SetUpPairForUpdate(associationId, pair));

    auto cleanup = wil::scope_exit([&] { RemoveLoopback(associationId); });

    auto const result = SendPairUpdate(
        pair,
        L"", L"blank name on the A side",
        L"Still Fine B", L"second");

    VERIFY_IS_FALSE(result.IsSuccess());

    // the good sibling in the same batch must be untouched too
    auto const after = GetReportedPair(associationId);

    VERIFY_IS_TRUE(after.Found);
    VERIFY_ARE_EQUAL(pair.A.Name, after.A.Name);
    VERIFY_ARE_EQUAL(pair.B.Name, after.B.Name);
}
