
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// ----------------------------------------------------------------------------
// This requires the version of MidiSrv with the hard-coded loopback endpoint
// ----------------------------------------------------------------------------


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <Windows.h>

//#include "..\api-core\ump_helpers.h"
#include <wil\resource.h>



//using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Data::Json;




#define JSON_KEY_EP_NAME L"name"
#define JSON_KEY_EP_PID L"productInstanceId"
#define JSON_KEY_EP_UMPVERMAJ L"umpVersionMajor"
#define JSON_KEY_EP_UMPVERMIN L"umpVersionMinor"
#define JSON_KEY_EP_STATICFB L"staticFunctionBlocks"
#define JSON_KEY_EP_NUMFB L"numberOfFunctionBlocks"
#define JSON_KEY_EP_MIDI2 L"midi2ProtocolCapability"
#define JSON_KEY_EP_MIDI1 L"midi1ProtocolCapability"
#define JSON_KEY_EP_RECJR L"receiveJRTimestampCapability"
#define JSON_KEY_EP_SENDJR L"sendJRTimestampCapability"



TEST_CASE("Offline.Metadata.EndpointInformation Get Json String")
{
    MidiEndpointInformation ep;

    auto json = ((IMidiCacheableMetadata)ep).GetJsonString();

    std::cout << winrt::to_string(json) << std::endl;

    REQUIRE(json != L"");

}

TEST_CASE("Offline.Metadata.EndpointInformation Set Json from String")
{

    const winrt::hstring epName = L"My Endpoint";
    const winrt::hstring epProductInstanceId = L"3263827";
    const uint8_t epUmpVersionMajor = 1;
    const uint8_t epUmpVersionMinor = 2;
    const bool epHasStaticFunctionBlocks = false;
    const uint8_t epNumberOfFunctionBlocks = 5;
    const bool epMidi2ProtocolCapability = true;
    const bool epMidi1ProtocolCapability = true;
    const bool epSupportsJRReceive = true;
    const bool epSupportsJRSend = true;

    JsonObject jsonObject;

    jsonObject.SetNamedValue(JSON_KEY_EP_NAME, JsonValue::CreateStringValue(epName));
    jsonObject.SetNamedValue(JSON_KEY_EP_PID, JsonValue::CreateStringValue(epProductInstanceId));
    jsonObject.SetNamedValue(JSON_KEY_EP_UMPVERMAJ, JsonValue::CreateNumberValue(epUmpVersionMajor));
    jsonObject.SetNamedValue(JSON_KEY_EP_UMPVERMIN, JsonValue::CreateNumberValue(epUmpVersionMinor));
    jsonObject.SetNamedValue(JSON_KEY_EP_STATICFB, JsonValue::CreateBooleanValue(epHasStaticFunctionBlocks));
    jsonObject.SetNamedValue(JSON_KEY_EP_NUMFB, JsonValue::CreateNumberValue(epNumberOfFunctionBlocks));
    jsonObject.SetNamedValue(JSON_KEY_EP_MIDI2, JsonValue::CreateBooleanValue(epMidi2ProtocolCapability));
    jsonObject.SetNamedValue(JSON_KEY_EP_MIDI1, JsonValue::CreateBooleanValue(epMidi1ProtocolCapability));
    jsonObject.SetNamedValue(JSON_KEY_EP_RECJR, JsonValue::CreateBooleanValue(epSupportsJRReceive));
    jsonObject.SetNamedValue(JSON_KEY_EP_SENDJR, JsonValue::CreateBooleanValue(epSupportsJRSend));

    winrt::hstring jsonString = jsonObject.Stringify();
    REQUIRE(jsonString != L"");

    std::cout << winrt::to_string(jsonString) << std::endl;

    // update from string

    MidiEndpointInformation ep1;
    ((IMidiCacheableMetadata)ep1).UpdateFromJsonString(jsonString);

    REQUIRE(ep1.Name() == epName);
    REQUIRE(ep1.ProductInstanceId() == epProductInstanceId);
    REQUIRE(ep1.UmpVersionMajor() == epUmpVersionMajor);
    REQUIRE(ep1.UmpVersionMinor() == epUmpVersionMinor);
    REQUIRE(ep1.HasStaticFunctionBlocks() == epHasStaticFunctionBlocks);
    REQUIRE(ep1.FunctionBlockCount() == epNumberOfFunctionBlocks);
    REQUIRE(ep1.SupportsMidi20Protocol() == epMidi2ProtocolCapability);
    REQUIRE(ep1.SupportsMidi10Protocol() == epMidi1ProtocolCapability);
    REQUIRE(ep1.SupportsReceivingJRTimestamps() == epSupportsJRReceive);
    REQUIRE(ep1.SupportsSendingJRTimestamps() == epSupportsJRSend);

    // update from json object

    MidiEndpointInformation ep2;
    ((IMidiCacheableMetadata)ep2).UpdateFromJson(jsonObject);

    REQUIRE(ep2.Name() == epName);
    REQUIRE(ep2.ProductInstanceId() == epProductInstanceId);
    REQUIRE(ep2.UmpVersionMajor() == epUmpVersionMajor);
    REQUIRE(ep2.UmpVersionMinor() == epUmpVersionMinor);
    REQUIRE(ep2.HasStaticFunctionBlocks() == epHasStaticFunctionBlocks);
    REQUIRE(ep2.FunctionBlockCount() == epNumberOfFunctionBlocks);
    REQUIRE(ep2.SupportsMidi20Protocol() == epMidi2ProtocolCapability);
    REQUIRE(ep2.SupportsMidi10Protocol() == epMidi1ProtocolCapability);
    REQUIRE(ep2.SupportsReceivingJRTimestamps() == epSupportsJRReceive);
    REQUIRE(ep2.SupportsSendingJRTimestamps() == epSupportsJRSend);


}



