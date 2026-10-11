// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

#include "MidiEndpointCustomPropertiesTests.h"

#include "Feature_Servicing_MIDI2EndpointCustomizationEnhancements.h"
#include "Feature_Servicing_MIDI2RecommendedCCIntervalProp.h"
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
        if (!Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
        {
            Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointCustomizationEnhancements is disabled.");
            return;
        }

        auto const props = config::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
            CustomPropertiesWithImage(imageValue));

        VERIFY_IS_NULL(props);
    }
}


void MidiEndpointCustomPropertiesTests::TestBareImageFileNameIsAccepted()
{
    if (!Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointCustomizationEnhancements is disabled.");
        return;
    }

    auto const props = config::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
        CustomPropertiesWithImage(L"ep-my picture.png"));

    VERIFY_IS_NOT_NULL(props);
    VERIFY_ARE_EQUAL(winrt::hstring{ L"ep-my picture.png" }, props->Image);
}


void MidiEndpointCustomPropertiesTests::TestMissingImageIsAccepted()
{
    if (!Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointCustomizationEnhancements is disabled.");
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


void MidiEndpointCustomPropertiesTests::TestControlChangeIntervalAndMpeUseTheirOwnProperties()
{
    if (!Feature_Servicing_MIDI2RecommendedCCIntervalProp::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2RecommendedCCIntervalProp is disabled.");
        return;
    }

    config::MidiEndpointCustomProperties props{};
    props.RequiresNoteOffTranslation = false;
    props.SupportsMidiPolyphonicExpression = true;
    props.RecommendedControlChangeIntervalMilliseconds = 20;

    // the property buffers point into props, so it has to outlive this list
    std::vector<DEVPROPERTY> written{};
    props.WriteNonCommonProperties(written);

    auto findAll = [&written](DEVPROPKEY const& key)
        {
            std::vector<DEVPROPERTY> found{};

            for (auto const& property : written)
            {
                if (IsEqualDevPropKey(property.CompKey.Key, key))
                {
                    found.push_back(property);
                }
            }

            return found;
        };

    // the gate-off path writes both values to this key as well, so it appears three times
    auto const noteOff = findAll(PKEY_MIDI_RequiresNoteOffTranslation);
    VERIFY_ARE_EQUAL(noteOff.size(), (size_t)1);
    VERIFY_ARE_EQUAL(noteOff[0].Type, (DEVPROPTYPE)DEVPROP_TYPE_BOOLEAN);
    VERIFY_ARE_EQUAL((int)*static_cast<DEVPROP_BOOLEAN*>(noteOff[0].Buffer), (int)DEVPROP_FALSE);

    auto const interval = findAll(PKEY_MIDI_RecommendedCCAutomationIntervalMS);
    VERIFY_ARE_EQUAL(interval.size(), (size_t)1);
    VERIFY_ARE_EQUAL(interval[0].Type, (DEVPROPTYPE)DEVPROP_TYPE_UINT16);
    VERIFY_ARE_EQUAL((uint32_t)*static_cast<uint16_t*>(interval[0].Buffer), (uint32_t)20);

    auto const mpe = findAll(PKEY_MIDI_SupportsMidiPolyphonicExpression);
    VERIFY_ARE_EQUAL(mpe.size(), (size_t)1);
    VERIFY_ARE_EQUAL(mpe[0].Type, (DEVPROPTYPE)DEVPROP_TYPE_BOOLEAN);
    VERIFY_ARE_EQUAL((int)*static_cast<DEVPROP_BOOLEAN*>(mpe[0].Buffer), (int)DEVPROP_TRUE);
}
