// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

#include "MidiEndpointCustomPropertiesTests.h"

#include "Feature_Servicing_MIDI2EndpointImageFileNameValidation.h"
#include "MidiEndpointCustomProperties.h"

using namespace WEX::Common;
using namespace WEX::Logging;

namespace json = ::winrt::Windows::Data::Json;
namespace config = ::WindowsMidiServicesPluginConfigurationLib;

namespace
{
    json::JsonObject CustomPropertiesWithImage(std::wstring const& imageValue)
    {
        // built as text rather than with the transport's own helpers, so this stays an
        // independent check of what a hand-authored configuration file would supply
        std::wstring text =
            L"{\"name\":\"Test Endpoint\",\"description\":\"Test\",\"image\":\"" + imageValue + L"\"}";

        json::JsonObject parsed{ nullptr };

        VERIFY_IS_TRUE(json::JsonObject::TryParse(winrt::hstring{ text }, parsed));

        return parsed;
    }

    void VerifyRejected(std::wstring const& imageValue)
    {
        // The behavior under test is KIR-gated, so the test has to no-op when the KIR is off,
        // otherwise a rollback turns this suite red.
        if (!Feature_Servicing_MIDI2EndpointImageFileNameValidation::IsEnabled())
        {
            Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointImageFileNameValidation is disabled.");
            return;
        }

        auto const props = config::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
            CustomPropertiesWithImage(imageValue));

        VERIFY_IS_NULL(props);
    }
}


void MidiEndpointCustomPropertiesTests::TestBareImageFileNameIsAccepted()
{
    if (!Feature_Servicing_MIDI2EndpointImageFileNameValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointImageFileNameValidation is disabled.");
        return;
    }

    auto const props = config::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
        CustomPropertiesWithImage(L"ep-my picture.png"));

    VERIFY_IS_NOT_NULL(props);
    VERIFY_ARE_EQUAL(winrt::hstring{ L"ep-my picture.png" }, props->Image);
}


void MidiEndpointCustomPropertiesTests::TestMissingImageIsAccepted()
{
    if (!Feature_Servicing_MIDI2EndpointImageFileNameValidation::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointImageFileNameValidation is disabled.");
        return;
    }

    // an entry with no picture at all is normal, and an empty one is how a picture gets cleared
    json::JsonObject parsed{ nullptr };

    VERIFY_IS_TRUE(json::JsonObject::TryParse(winrt::hstring{ L"{\"name\":\"Test Endpoint\"}" }, parsed));

    auto const props = config::MidiEndpointCustomProperties::FromJsonRejectingImagePath(parsed);

    VERIFY_IS_NOT_NULL(props);

    auto const cleared = config::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
        CustomPropertiesWithImage(L""));

    VERIFY_IS_NOT_NULL(cleared);
}


void MidiEndpointCustomPropertiesTests::TestRelativePathImageIsRejected()
{
    VerifyRejected(L"..\\\\..\\\\..\\\\Windows\\\\System32\\\\evil.png");
}


void MidiEndpointCustomPropertiesTests::TestAbsolutePathImageIsRejected()
{
    VerifyRejected(L"C:\\\\Windows\\\\System32\\\\evil.png");
}


void MidiEndpointCustomPropertiesTests::TestForwardSlashImageIsRejected()
{
    VerifyRejected(L"../../evil.png");
}


void MidiEndpointCustomPropertiesTests::TestAlternateDataStreamImageIsRejected()
{
    VerifyRejected(L"picture.png:hidden");
}


void MidiEndpointCustomPropertiesTests::TestWildcardImageIsRejected()
{
    VerifyRejected(L"*.png");
}


void MidiEndpointCustomPropertiesTests::TestUngatedParserStillAcceptsAPath()
{
    // FromJson is what a rolled back build calls, so it has to keep its original behavior
    // exactly. This is the "before" half of the KIR and must pass either way.
    auto const props = config::MidiEndpointCustomProperties::FromJson(
        CustomPropertiesWithImage(L"C:\\\\Windows\\\\System32\\\\evil.png"));

    VERIFY_IS_NOT_NULL(props);
    VERIFY_ARE_EQUAL(winrt::hstring{ L"C:\\Windows\\System32\\evil.png" }, props->Image);
}
