// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <string>

#include "MidiCiProgramList.h"

using namespace WindowsMidiServicesCapabilityInquiry;


namespace
{
    std::string BuildToString(const ProgramListEntry* entries, size_t count)
    {
        char buffer[4096]{};

        const auto length = BuildProgramListJson(entries, count, buffer, sizeof(buffer));

        return std::string(buffer, length);
    }
}


void MidiCiProgramListTests::TestEmptyListIsStillValidJson()
{
    VERIFY_ARE_EQUAL(BuildToString(nullptr, 0), std::string("[]"));
}

void MidiCiProgramListTests::TestSingleEntryBytes()
{
    ProgramListEntry entry{};

    entry.Title = "Acoustic Grand Piano";
    entry.BankMsb = 0;
    entry.BankLsb = 0;
    entry.Program = 0;
    entry.Category = "Piano";

    // Compared against text written out by hand, not against whatever the builder happens to emit.
    VERIFY_ARE_EQUAL(
        BuildToString(&entry, 1),
        std::string("[{\"title\":\"Acoustic Grand Piano\",\"bankPC\":[0,0,0],\"category\":[\"Piano\"]}]"));
}

void MidiCiProgramListTests::TestBankProgramAreZeroBased()
{
    // The specification's own example would put Acoustic Grand Piano at program 1. An
    // implementation that copied it is off by one for every program in the list.
    ProgramListEntry entries[2]{};

    entries[0].Title = "Acoustic Grand Piano";
    entries[0].Program = 0;

    entries[1].Title = "Bright Acoustic Piano";
    entries[1].Program = 1;

    const auto json = BuildToString(entries, 2);

    VERIFY_IS_TRUE(json.find("\"title\":\"Acoustic Grand Piano\",\"bankPC\":[0,0,0]") != std::string::npos);
    VERIFY_IS_TRUE(json.find("\"title\":\"Bright Acoustic Piano\",\"bankPC\":[0,0,1]") != std::string::npos);

    // A GS variation, which is how this sound set is actually addressed.
    ProgramListEntry variation{};

    variation.Title = "Piano 1";
    variation.BankMsb = 8;
    variation.Program = 0;

    VERIFY_IS_TRUE(BuildToString(&variation, 1).find("\"bankPC\":[8,0,0]") != std::string::npos);
}

void MidiCiProgramListTests::TestTitleIsEscaped()
{
    ProgramListEntry entry{};

    // A quote inside a name would end the JSON string early and corrupt everything after it.
    entry.Title = "He said \"hi\" \\ bye";

    const auto json = BuildToString(&entry, 1);

    VERIFY_ARE_EQUAL(
        json,
        std::string("[{\"title\":\"He said \\\"hi\\\" \\\\ bye\",\"bankPC\":[0,0,0]}]"));

    // Quotes must be balanced once escaping is accounted for.
    size_t unescapedQuotes = 0;

    for (size_t i = 0; i < json.size(); i++)
    {
        if (json[i] == '\\') { i++; continue; }
        if (json[i] == '"') { unescapedQuotes++; }
    }

    VERIFY_ARE_EQUAL(unescapedQuotes % 2, (size_t)0);
}

void MidiCiProgramListTests::TestOutputIsAlwaysSevenBit()
{
    ProgramListEntry entry{};

    // Names come out of a sound set file and are not guaranteed to be ASCII. A byte with the high
    // bit set inside a system exclusive message terminates it early.
    entry.Title = "Caf\xC3\xA9 \x01 Organ\x80";

    const auto json = BuildToString(&entry, 1);

    VERIFY_IS_GREATER_THAN(json.size(), (size_t)0);

    for (const auto character : json)
    {
        VERIFY_IS_LESS_THAN((uint8_t)character, (uint8_t)0x80);
        VERIFY_IS_GREATER_THAN_OR_EQUAL((uint8_t)character, (uint8_t)0x20);
    }

    VERIFY_IS_TRUE(json.find("\\u00C3") != std::string::npos);
    VERIFY_IS_TRUE(json.find("\\u0001") != std::string::npos);
}

void MidiCiProgramListTests::TestMeasureThenBuild()
{
    ProgramListEntry entries[3]{};

    entries[0].Title = "One";
    entries[1].Title = "Two";
    entries[1].Program = 1;
    entries[2].Title = "Three";
    entries[2].Program = 2;

    // A null buffer measures, so a caller can allocate once and exactly.
    const auto measured = BuildProgramListJson(entries, 3, nullptr, 0);

    VERIFY_IS_GREATER_THAN(measured, (size_t)0);

    char buffer[256]{};
    const auto written = BuildProgramListJson(entries, 3, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(written, measured);

    // And a buffer of exactly the measured size must succeed.
    const auto exact = BuildProgramListJson(entries, 3, buffer, measured);

    VERIFY_ARE_EQUAL(exact, measured);
}

void MidiCiProgramListTests::TestShortBufferProducesNothing()
{
    ProgramListEntry entry{};
    entry.Title = "Acoustic Grand Piano";

    const auto measured = BuildProgramListJson(&entry, 1, nullptr, 0);

    char buffer[256]{};

    // One byte short must report failure rather than hand back a truncated array that parses as
    // valid JSON right up until it does not.
    VERIFY_ARE_EQUAL(BuildProgramListJson(&entry, 1, buffer, measured - 1), (size_t)0);
    VERIFY_ARE_EQUAL(BuildProgramListJson(&entry, 1, buffer, 0), (size_t)0);
    VERIFY_ARE_EQUAL(BuildProgramListJson(&entry, 1, buffer, measured), measured);
}

namespace
{
    DeviceInfoFields MakeSynthDeviceInfo()
    {
        DeviceInfoFields fields{};

        fields.ManufacturerId[0] = 0x00;
        fields.ManufacturerId[1] = 0x00;
        fields.ManufacturerId[2] = 0x41;
        fields.Manufacturer = "Microsoft";

        fields.FamilyId[0] = 11;
        fields.FamilyId[1] = 0;
        fields.Family = "Windows";

        fields.ModelId[0] = 1;
        fields.ModelId[1] = 0;
        fields.Model = "General MIDI Synth";

        return fields;
    }
}

void MidiCiProgramListTests::TestDeviceInfoBytes()
{
    char buffer[512]{};

    const auto length = BuildDeviceInfoJson(MakeSynthDeviceInfo(), buffer, sizeof(buffer));

    VERIFY_IS_GREATER_THAN(length, (size_t)0);

    // Written out by hand from the worked example in the specification.
    VERIFY_ARE_EQUAL(
        std::string(buffer, length),
        std::string("{\"manufacturerId\":[0,0,65],\"manufacturer\":\"Microsoft\","
                    "\"familyId\":[11,0],\"family\":\"Windows\","
                    "\"modelId\":[1,0],\"model\":\"General MIDI Synth\"}"));
}

void MidiCiProgramListTests::TestDeviceInfoAgreesWithTheOtherIdentityCarriers()
{
    // This is the fifth place the same identity appears. If DeviceInfo disagrees with the discovery
    // reply, a client sees one device claiming to be two.
    const auto info = MakeSynthDeviceInfo();

    char buffer[512]{};
    const auto length = BuildDeviceInfoJson(info, buffer, sizeof(buffer));
    const std::string json(buffer, length);

    // 00 00 41 is Microsoft. Roland is the single byte 41 and must never appear here.
    VERIFY_IS_TRUE(json.find("\"manufacturerId\":[0,0,65]") != std::string::npos);
    VERIFY_IS_TRUE(json.find("\"familyId\":[11,0]") != std::string::npos);
    VERIFY_IS_TRUE(json.find("\"modelId\":[1,0]") != std::string::npos);

    const auto measured = BuildDeviceInfoJson(info, nullptr, 0);

    VERIFY_ARE_EQUAL(measured, length);
    VERIFY_ARE_EQUAL(BuildDeviceInfoJson(info, buffer, measured - 1), (size_t)0);
}

void MidiCiProgramListTests::TestResourceListBytes()
{
    ResourceListEntry resources[3]{};

    resources[0].Resource = "ResourceList";
    resources[1].Resource = "ChannelList";
    resources[1].CanSubscribe = true;
    resources[2].Resource = "ProgramList";
    resources[2].RequireResourceId = true;

    char buffer[256]{};

    const auto length = BuildResourceListJson(resources, 3, buffer, sizeof(buffer));

    // Both flags default to false in the specification, so a bare name is a complete entry and
    // only the true ones are written.
    VERIFY_ARE_EQUAL(
        std::string(buffer, length),
        std::string("[{\"resource\":\"ResourceList\"},"
                    "{\"resource\":\"ChannelList\",\"canSubscribe\":true},"
                    "{\"resource\":\"ProgramList\",\"requireResId\":true}]"));

    // An empty list is still a valid array.
    VERIFY_ARE_EQUAL(BuildResourceListJson(nullptr, 0, buffer, sizeof(buffer)), (size_t)2);
    VERIFY_ARE_EQUAL(std::string(buffer, 2), std::string("[]"));
}

void MidiCiProgramListTests::TestChannelListWithoutLinksBytes()
{
    ChannelListEntry entry{};

    entry.Title = "Channel 1";
    entry.Channel = 1;
    entry.ProgramTitle = "Piano 1";
    entry.BankMsb = 0;
    entry.BankLsb = 0;
    entry.Program = 0;

    char buffer[256]{};

    const auto length = BuildChannelListJson(&entry, 1, buffer, sizeof(buffer));

    // No links array at all rather than an empty one, which is what a device with a single
    // program list sends.
    VERIFY_ARE_EQUAL(
        std::string(buffer, length),
        std::string("[{\"title\":\"Channel 1\",\"channel\":1,\"programTitle\":\"Piano 1\","
                    "\"bankPC\":[0,0,0]}]"));
}

void MidiCiProgramListTests::TestChannelListLinksBytes()
{
    // Worked out by hand from the drum channel entry in M2-105-UM section 4.5.2, which is what
    // makes a device with more than one program list usable.
    ResourceLink link{};

    link.Resource = "ProgramList";
    link.ResourceId = "gm2drums";
    link.Title = "GM2 Drum Sets";

    ChannelListEntry entry{};

    entry.Title = "Drums";
    entry.Channel = 10;
    entry.ProgramTitle = "GM2 Jazz Drum Set";
    entry.BankMsb = 0;
    entry.BankLsb = 0;
    entry.Program = 33;
    entry.Links = &link;
    entry.LinkCount = 1;

    char buffer[512]{};

    const auto length = BuildChannelListJson(&entry, 1, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(
        std::string(buffer, length),
        std::string("[{\"title\":\"Drums\",\"channel\":10,\"programTitle\":\"GM2 Jazz Drum Set\","
                    "\"bankPC\":[0,0,33],"
                    "\"links\":[{\"resource\":\"ProgramList\",\"resId\":\"gm2drums\","
                    "\"title\":\"GM2 Drum Sets\"}]}]"));

    const auto measured = BuildChannelListJson(&entry, 1, nullptr, 0);

    VERIFY_ARE_EQUAL(measured, length);
    VERIFY_ARE_EQUAL(BuildChannelListJson(&entry, 1, buffer, measured - 1), (size_t)0);
}

void MidiCiProgramListTests::TestChannelListLinkOmitsMissingFields()
{
    // A link with no resource id is what a device with exactly one collection sends, and it must
    // not produce "resId":null or an empty string a client would send back.
    ResourceLink links[2]{};

    links[0].Resource = "ProgramList";
    links[1].Resource = "X-MidiEffects";
    links[1].ResourceId = "singch1";

    ChannelListEntry entry{};

    entry.Title = "Lead";
    entry.Channel = 1;
    entry.Links = links;
    entry.LinkCount = 2;

    char buffer[512]{};

    const auto length = BuildChannelListJson(&entry, 1, buffer, sizeof(buffer));

    VERIFY_ARE_EQUAL(
        std::string(buffer, length),
        std::string("[{\"title\":\"Lead\",\"channel\":1,\"bankPC\":[0,0,0],"
                    "\"links\":[{\"resource\":\"ProgramList\"},"
                    "{\"resource\":\"X-MidiEffects\",\"resId\":\"singch1\"}]}]"));
}
