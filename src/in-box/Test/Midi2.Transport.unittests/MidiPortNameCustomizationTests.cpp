// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

#include "MidiPortNameCustomizationTests.h"

#include "Feature_Servicing_MIDI2EndpointCustomizationEnhancements.h"
#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointCustomizationProcessor.h"

using namespace WEX::Common;
using namespace WEX::Logging;

namespace json = ::winrt::Windows::Data::Json;
namespace config = ::WindowsMidiServicesPluginConfigurationLib;
namespace naming = ::WindowsMidiServicesNamingLib;

namespace
{
    // Clearing a port on the running endpoint is KIR-gated, so these pass trivially when the KIR is
    // off rather than turn the suite red on a rollback.
    bool SkipUnlessEnhancementsEnabled()
    {
        if (Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
        {
            return false;
        }

        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointCustomizationEnhancements is disabled.");
        return true;
    }

    json::JsonObject Parse(_In_ std::wstring const& text)
    {
        json::JsonObject parsed{ nullptr };

        VERIFY_IS_TRUE(json::JsonObject::TryParse(winrt::hstring{ text }, parsed));

        return parsed;
    }

    // empties inputs 1 and 3 and output 4, and names input 2 and output 5
    std::wstring const ClearedAndNamedPorts{ LR"({
        "midi1Ports": {
            "namingApproach": "default",
            "sources": [
                { "groupIndex": 0, "name": "" },
                { "groupIndex": 1, "name": "Keys" },
                { "groupIndex": 2, "name": "   " } ],
            "destinations": [
                { "groupIndex": 3, "name": "" },
                { "groupIndex": 4, "name": "Pads" } ] } })" };

    // Three inputs and one output that already have custom names, as the endpoint has them before
    // the update is applied.
    std::shared_ptr<naming::MidiEndpointNameTable> TableWithCustomNames()
    {
        auto table = std::make_shared<naming::MidiEndpointNameTable>();

        VERIFY_SUCCEEDED(table->PopulateEntryForMidi1DeviceUsingMidi1Driver(0, MidiFlow::MidiFlowIn, L"Old A", L"Device", L"Filter", L"Pin", 0));
        VERIFY_SUCCEEDED(table->PopulateEntryForMidi1DeviceUsingMidi1Driver(1, MidiFlow::MidiFlowIn, L"Old B", L"Device", L"Filter", L"Pin", 1));
        VERIFY_SUCCEEDED(table->PopulateEntryForMidi1DeviceUsingMidi1Driver(2, MidiFlow::MidiFlowIn, L"Keep", L"Device", L"Filter", L"Pin", 2));
        VERIFY_SUCCEEDED(table->PopulateEntryForMidi1DeviceUsingMidi1Driver(0, MidiFlow::MidiFlowOut, L"Out A", L"Device", L"Filter", L"Pin", 0));

        return table;
    }

    config::MidiEndpointCustomizationApplyResult ResultWithNoProperties()
    {
        config::MidiEndpointCustomizationApplyResult result{};
        result.Properties = std::make_shared<config::MidiEndpointCustomProperties>();

        return result;
    }
}


void MidiPortNameCustomizationTests::TestEmptyPortNameIsReadAsCleared()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    std::vector<uint8_t> sources{};
    std::vector<uint8_t> destinations{};

    config::MidiEndpointCustomProperties::ReadClearedMidi1Ports(Parse(ClearedAndNamedPorts), sources, destinations);

    // a name of only spaces is trimmed to nothing by the parser, so it clears the port too
    VERIFY_ARE_EQUAL(sources.size(), (size_t)2);
    VERIFY_ARE_EQUAL((uint32_t)sources[0], (uint32_t)0);
    VERIFY_ARE_EQUAL((uint32_t)sources[1], (uint32_t)2);

    VERIFY_ARE_EQUAL(destinations.size(), (size_t)1);
    VERIFY_ARE_EQUAL((uint32_t)destinations[0], (uint32_t)3);
}


void MidiPortNameCustomizationTests::TestPortNamedElsewhereInTheListIsNotCleared()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    auto const section = Parse(LR"({ "midi1Ports": { "sources": [
        { "groupIndex": 0, "name": "" },
        { "groupIndex": 0, "name": "Keys" } ] } })");

    std::vector<uint8_t> sources{};
    std::vector<uint8_t> destinations{};

    config::MidiEndpointCustomProperties::ReadClearedMidi1Ports(section, sources, destinations);

    VERIFY_ARE_EQUAL(sources.size(), (size_t)0);

    // the parser keeps the name, which is why the port must not also be cleared
    auto const props = config::MidiEndpointCustomProperties::FromJson(section);

    VERIFY_IS_NOT_NULL(props);
    VERIFY_IS_TRUE(props->Midi1Sources.find(0) != props->Midi1Sources.end());
    VERIFY_ARE_EQUAL(props->Midi1Sources.at(0).Name, winrt::hstring{ L"Keys" });
}


void MidiPortNameCustomizationTests::TestMalformedPortEntriesAreIgnored()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    // The configuration file is writable by any standard user, so nothing in it may throw out of
    // the reader. Only the last source entry is well formed.
    auto const section = Parse(LR"({ "midi1Ports": {
        "sources": [
            "not an object",
            5,
            { "groupIndex": "1", "name": "" },
            { "groupIndex": 16, "name": "" },
            { "groupIndex": -1, "name": "" },
            { "groupIndex": 1e300, "name": "" },
            { "groupIndex": 2, "name": 7 },
            { "groupIndex": 3 },
            { "name": "" },
            { "groupIndex": 4, "name": "" } ],
        "destinations": { "groupIndex": 0, "name": "" } } })");

    std::vector<uint8_t> sources{};
    std::vector<uint8_t> destinations{};

    config::MidiEndpointCustomProperties::ReadClearedMidi1Ports(section, sources, destinations);

    VERIFY_ARE_EQUAL(sources.size(), (size_t)1);
    VERIFY_ARE_EQUAL((uint32_t)sources[0], (uint32_t)4);
    VERIFY_ARE_EQUAL(destinations.size(), (size_t)0);

    // a port list that is not an object is ignored as a whole
    config::MidiEndpointCustomProperties::ReadClearedMidi1Ports(
        Parse(LR"({ "midi1Ports": [ { "groupIndex": 0, "name": "" } ] })"), sources, destinations);

    VERIFY_ARE_EQUAL(sources.size(), (size_t)0);
    VERIFY_ARE_EQUAL(destinations.size(), (size_t)0);
}


void MidiPortNameCustomizationTests::TestNoPortListMeansNothingIsCleared()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    // filled beforehand, to show the reader empties its outputs rather than adding to them
    std::vector<uint8_t> sources{ 9 };
    std::vector<uint8_t> destinations{ 9 };

    config::MidiEndpointCustomProperties::ReadClearedMidi1Ports(Parse(LR"({ "name": "Synth" })"), sources, destinations);

    VERIFY_ARE_EQUAL(sources.size(), (size_t)0);
    VERIFY_ARE_EQUAL(destinations.size(), (size_t)0);
}


void MidiPortNameCustomizationTests::TestParserStillLeavesEmptyPortNamesOut()
{
    // FromJson is what a rolled back build relies on, so it has to keep its original behavior
    // exactly. This is the "before" half of the KIR and must pass either way.
    auto const props = config::MidiEndpointCustomProperties::FromJson(Parse(ClearedAndNamedPorts));

    VERIFY_IS_NOT_NULL(props);

    VERIFY_ARE_EQUAL(props->Midi1Sources.size(), (size_t)1);
    VERIFY_IS_TRUE(props->Midi1Sources.find(1) != props->Midi1Sources.end());

    VERIFY_ARE_EQUAL(props->Midi1Destinations.size(), (size_t)1);
    VERIFY_IS_TRUE(props->Midi1Destinations.find(4) != props->Midi1Destinations.end());
}


void MidiPortNameCustomizationTests::TestNamedPortsAreSetAndOthersAreLeftAlone()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    auto table = TableWithCustomNames();
    auto result = ResultWithNoProperties();

    result.Properties->Midi1Sources[1] = { 1, winrt::hstring{ L"New B" } };

    config::MidiEndpointCustomizationProcessor::ApplyPortNames(result, *table);

    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(0), std::wstring{ L"Old A" });
    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(1), std::wstring{ L"New B" });
    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(2), std::wstring{ L"Keep" });
    VERIFY_ARE_EQUAL(table->GetDestinationEntryCustomName(0), std::wstring{ L"Out A" });
}


void MidiPortNameCustomizationTests::TestClearedPortLosesItsCustomName()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    auto table = TableWithCustomNames();
    auto result = ResultWithNoProperties();

    // one update both renames a port and clears another, as a save of the whole list does
    result.Properties->Midi1Sources[1] = { 1, winrt::hstring{ L"New B" } };
    result.ClearedSourceGroups = { 0 };

    config::MidiEndpointCustomizationProcessor::ApplyPortNames(result, *table);

    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(0), std::wstring{});
    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(1), std::wstring{ L"New B" });
    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(2), std::wstring{ L"Keep" });
    VERIFY_ARE_EQUAL(table->GetDestinationEntryCustomName(0), std::wstring{ L"Out A" });
}


void MidiPortNameCustomizationTests::TestClearedDestinationLeavesTheSourceAlone()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    auto table = TableWithCustomNames();
    auto result = ResultWithNoProperties();

    result.ClearedDestinationGroups = { 0 };

    config::MidiEndpointCustomizationProcessor::ApplyPortNames(result, *table);

    VERIFY_ARE_EQUAL(table->GetDestinationEntryCustomName(0), std::wstring{});
    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(0), std::wstring{ L"Old A" });
}


void MidiPortNameCustomizationTests::TestRemovalClearsEveryPortName()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    auto table = TableWithCustomNames();
    auto result = ResultWithNoProperties();

    result.ClearAllPortNames = true;

    config::MidiEndpointCustomizationProcessor::ApplyPortNames(result, *table);

    for (uint8_t groupIndex = 0; groupIndex < 3; groupIndex++)
    {
        VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(groupIndex), std::wstring{});
    }

    VERIFY_ARE_EQUAL(table->GetDestinationEntryCustomName(0), std::wstring{});
}


void MidiPortNameCustomizationTests::TestClearingAGroupTheEndpointLacksIsHarmless()
{
    if (SkipUnlessEnhancementsEnabled()) { return; }

    auto table = TableWithCustomNames();

    // no property set at all, as when the update's properties were refused
    config::MidiEndpointCustomizationApplyResult result{};
    result.ClearedSourceGroups = { 9 };
    result.ClearedDestinationGroups = { 15 };

    config::MidiEndpointCustomizationProcessor::ApplyPortNames(result, *table);

    VERIFY_IS_TRUE(table->GetSourceEntry(9) == nullptr);
    VERIFY_IS_TRUE(table->GetDestinationEntry(15) == nullptr);

    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(0), std::wstring{ L"Old A" });
    VERIFY_ARE_EQUAL(table->GetSourceEntryCustomName(1), std::wstring{ L"Old B" });
    VERIFY_ARE_EQUAL(table->GetDestinationEntryCustomName(0), std::wstring{ L"Out A" });
}
