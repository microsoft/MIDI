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

#include <iomanip>


//using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Data::Json;


#define JSON_KEY_FB_NUMBER L"functionBlockNumber"
#define JSON_KEY_FB_NAME L"name"
#define JSON_KEY_FB_ACTIVE L"active"
#define JSON_KEY_FB_UIHINT L"uiHint"
#define JSON_KEY_FB_MIDI10 L"midi10"
#define JSON_KEY_FB_DIRECTION L"direction"
#define JSON_KEY_FB_FIRSTGROUP L"firstGroupIndex"
#define JSON_KEY_FB_NUMGROUPSSPANNED L"numberOfGroupsSpanned"
#define JSON_KEY_FB_MIDICIFORMAT L"midiCIMessageFormat"

TEST_CASE("Offline.Metadata.FunctionBlock Get Json String")
{
    MidiFunctionBlock fb;

    auto json = ((IMidiCacheableMetadata)fb).GetJsonString();

    std::cout << winrt::to_string(json) << std::endl;

    REQUIRE(json != L"");

}

TEST_CASE("Offline.Metadata.FunctionBlock Set Json from String")
{
    const uint8_t fbNumber = 1;
    const winrt::hstring fbName = L"My Test Function Block";
    const bool fbIsActive = true;
    const MidiFunctionBlockUIHint fbUIHint = MidiFunctionBlockUIHint::Sender;
    const MidiFunctionBlockMidi10 fbMidi10 = MidiFunctionBlockMidi10::Not10;
    const MidiFunctionBlockDirection fbDirection = MidiFunctionBlockDirection::Bidirectional;
    const uint8_t fbFirstGroup = 12;
    const uint8_t fbNumGroupsSpanned = 2;
    const uint8_t fbMidiCIMessageVersionFormat = 1;

    JsonObject jsonObject;

    jsonObject.SetNamedValue(JSON_KEY_FB_NUMBER, JsonValue::CreateNumberValue(fbNumber));
    jsonObject.SetNamedValue(JSON_KEY_FB_NAME, JsonValue::CreateStringValue(fbName));
    jsonObject.SetNamedValue(JSON_KEY_FB_ACTIVE, JsonValue::CreateBooleanValue(fbIsActive));
    jsonObject.SetNamedValue(JSON_KEY_FB_UIHINT, JsonValue::CreateNumberValue((int)fbUIHint));
    jsonObject.SetNamedValue(JSON_KEY_FB_MIDI10, JsonValue::CreateNumberValue((int)fbMidi10));
    jsonObject.SetNamedValue(JSON_KEY_FB_DIRECTION, JsonValue::CreateNumberValue((int)fbDirection));
    jsonObject.SetNamedValue(JSON_KEY_FB_FIRSTGROUP, JsonValue::CreateNumberValue(fbFirstGroup));
    jsonObject.SetNamedValue(JSON_KEY_FB_NUMGROUPSSPANNED, JsonValue::CreateNumberValue(fbNumGroupsSpanned));
    jsonObject.SetNamedValue(JSON_KEY_FB_MIDICIFORMAT, JsonValue::CreateNumberValue(fbMidiCIMessageVersionFormat));

    winrt::hstring jsonString = jsonObject.Stringify();
    REQUIRE(jsonString != L"");

    std::cout << winrt::to_string(jsonString) << std::endl;

    // update from string

    MidiFunctionBlock fb;
    ((IMidiCacheableMetadata)fb).UpdateFromJsonString(jsonString);

    REQUIRE(fb.Number() == fbNumber);
    REQUIRE(fb.Name() == fbName);
    REQUIRE(fb.IsActive() == fbIsActive);
    REQUIRE(fb.UIHint() == fbUIHint);
    REQUIRE(fb.Midi10Connection() == fbMidi10);
    REQUIRE(fb.Direction() == fbDirection);
    REQUIRE(fb.FirstGroupIndex() == fbFirstGroup);
    REQUIRE(fb.NumberOfGroupsSpanned() == fbNumGroupsSpanned);
    REQUIRE(fb.MidiCIMessageVersionFormat() == fbMidiCIMessageVersionFormat);


    // update from json object

    MidiFunctionBlock fb2;
    ((IMidiCacheableMetadata)fb2).UpdateFromJson(jsonObject);

    REQUIRE(fb2.Number() == fbNumber);
    REQUIRE(fb2.Name() == fbName);
    REQUIRE(fb2.IsActive() == fbIsActive);
    REQUIRE(fb2.UIHint() == fbUIHint);
    REQUIRE(fb2.Midi10Connection() == fbMidi10);
    REQUIRE(fb2.Direction() == fbDirection);
    REQUIRE(fb2.FirstGroupIndex() == fbFirstGroup);
    REQUIRE(fb2.NumberOfGroupsSpanned() == fbNumGroupsSpanned);
    REQUIRE(fb2.MidiCIMessageVersionFormat() == fbMidiCIMessageVersionFormat);

}



TEST_CASE("Offline.Metadata.FunctionBlock Get from Message")
{   
    std::cout << std::endl;
    std::cout << "(Get from Message) Building Function Block Info Notification" << std::endl;

    bool active{ true };
    uint8_t functionBlockNumber{ 31 };
    MidiFunctionBlockUIHint uiHint{ MidiFunctionBlockUIHint::Sender };                
    MidiFunctionBlockMidi10 midi10{ MidiFunctionBlockMidi10::Not10 };  
    MidiFunctionBlockDirection direction{ MidiFunctionBlockDirection::Bidirectional }; 

    uint8_t firstGroup{ 1 };
    uint8_t numberOfGroups{ 8 };
    uint8_t midiCIVersionFormat{ 1 };
    uint8_t maxNumberOfSysEx8Streams{ 1 };

    winrt::hstring name = L"A Function Block";

    auto ump = MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
        MidiClock::GetMidiTimestamp(),
        active,
        functionBlockNumber,
        uiHint,
        midi10,
        direction,
        firstGroup,
        numberOfGroups,
        midiCIVersionFormat,
        maxNumberOfSysEx8Streams
    );

    auto messages = MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        MidiClock::GetMidiTimestamp(),
        functionBlockNumber,
        name
    );

    // add the info notification to the name notification
    messages.Append(ump);


    for (auto message : messages)
    {
        std::cout << "   Function Block Message:"  
            << " 0x" << std::hex << std::setw(8) << std::setfill('0') << message.Word0()
            << " 0x" << std::hex << std::setw(8) << std::setfill('0') << message.Word1() 
            << std::endl;

        std::cout << "   - Status: 0x" << std::hex << (int)MidiMessageUtility::GetStatusFromStreamMessageFirstWord(message.Word0()) << std::endl;
        std::cout << "   - Form:   0x" << std::hex << (int)MidiMessageUtility::GetFormFromStreamMessageFirstWord(message.Word0()) << std::endl;
        std::cout << std::endl;
    }

    MidiFunctionBlock block{};

    bool success = block.UpdateFromMessages(messages);

    REQUIRE(success);

    REQUIRE(block.Name() == name);

    REQUIRE(block.Number() == functionBlockNumber);
    REQUIRE(block.Midi10Connection() == midi10);
    REQUIRE(block.UIHint() == uiHint);
    REQUIRE(block.Direction() == direction);
    REQUIRE(block.FirstGroupIndex() == firstGroup);
    REQUIRE(block.NumberOfGroupsSpanned() == numberOfGroups);
    REQUIRE(block.MidiCIMessageVersionFormat() == midiCIVersionFormat);
    REQUIRE(block.MaxSystemExclusive8Streams() == maxNumberOfSysEx8Streams);
    REQUIRE(block.IsActive() == active);

}