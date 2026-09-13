// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


#include "MidiEndpointNameTable.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"

using namespace WindowsMidiServicesNamingLib;

namespace
{
    // The new naming only takes effect when the servicing gate is on, so these have to pass
    // trivially when it is off rather than turn the suite red on a rollback.
    bool SkipUnlessPortNamingReworkEnabled()
    {
        if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled()) { return false; }

        WEX::Logging::Log::Comment(L"Feature_Servicing_MIDI2PortNamingRework is disabled. Skipping.");
        return true;
    }
}



//void NamingTests::TestGetPreferredName()
//{
//}
//
//void NamingTests::TestGetSourceEntry()
//{
//}
//
//void NamingTests::TestGetDestinationEntry()
//{
//}
//
//void NamingTests::TestUpdateSourceEntryCustomName()
//{
//}
//
//void NamingTests::TestUpdateDestinationEntryCustomName()
//{
//}
//
//
//
//void NamingTests::TestGetSourceEntryCustomName()
//{
//}
//
//void NamingTests::TestGetDestinationEntryCustomName()
//{
//}
//
//
//void NamingTests::TestPopulateAllEntriesForNativeUmpDevice()
//{
//}
//
//void NamingTests::TestPopulateAllEntriesForMidi1DeviceUsingUmpDriver()
//{
//}

void NamingTests::TestStringEndsWithSpecifiedNumber()
{
    // we don't test for numeric-only strings because that is not an expected scenario
    // and the function under test doesn't handle that.

    VERIFY_IS_TRUE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"FOO 01", 1));
    VERIFY_IS_TRUE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"FOO 25 01", 1));
    VERIFY_IS_TRUE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"G 0005", 5));
    VERIFY_IS_TRUE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"A 6", 6));

    VERIFY_IS_FALSE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"A [6]", 6));

    VERIFY_IS_TRUE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"YAMAHA DTX-MULTI 12", 12));
    VERIFY_IS_FALSE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"YAMAHA DTX-MULTI 12", 2));
    VERIFY_IS_FALSE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"YAMAHA DTX-MULTI 12", 1));
    VERIFY_IS_FALSE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"YAMAHA DTX-MULTI 120", 12));
    VERIFY_IS_TRUE(WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(L"YAMAHA DTX-MULTI 120", 120));
}

void NamingTests::TestPopulateEntryForNativeUmpDevice()
{
    MidiEndpointNameTable table;

    uint8_t portIndexSource{ 0 };
    uint8_t portIndexDestination{ 0 };

    // name from the registry isn't always correct, so keeping it different from filterName is important for this thest
    std::wstring parentDeviceName{ L"TEST DEVICE PARENT" };
    std::wstring filterName{ L"TEST DEVICE" };

    std::vector<std::wstring> sourceBlockNames{};
    sourceBlockNames.push_back(L"MIDI");
    sourceBlockNames.push_back(L"Synth [0]");
    sourceBlockNames.push_back(L"Synth [1]");
    sourceBlockNames.push_back(L"Synth [2]");
    sourceBlockNames.push_back(L"Ext In [0]");
    sourceBlockNames.push_back(L"Ext In [1]");

    std::vector<std::wstring> expectedSourceNewStyleNames{};
    expectedSourceNewStyleNames.push_back(filterName);
    expectedSourceNewStyleNames.push_back(filterName + L" Synth");
    expectedSourceNewStyleNames.push_back(filterName + L" Synth");
    expectedSourceNewStyleNames.push_back(filterName + L" Synth");
    expectedSourceNewStyleNames.push_back(filterName + L" Ext In");
    expectedSourceNewStyleNames.push_back(filterName + L" Ext In");

    std::vector<std::wstring> expectedSourceClassicNames{};
    expectedSourceClassicNames.push_back(parentDeviceName);
    expectedSourceClassicNames.push_back(L"MIDIIN2 (" + parentDeviceName + L")");
    expectedSourceClassicNames.push_back(L"MIDIIN3 (" + parentDeviceName + L")");
    expectedSourceClassicNames.push_back(L"MIDIIN4 (" + parentDeviceName + L")");
    expectedSourceClassicNames.push_back(L"MIDIIN5 (" + parentDeviceName + L")");
    expectedSourceClassicNames.push_back(L"MIDIIN6 (" + parentDeviceName + L")");


    std::vector<std::wstring> destinationBlockNames{};
    destinationBlockNames.push_back(L"MIDI");
    destinationBlockNames.push_back(L"Control");
    destinationBlockNames.push_back(L"Ext Out [0]");
    destinationBlockNames.push_back(L"Ext Out [1]");

    std::vector<std::wstring> expectedDestinationNewStyleNames{};
    expectedDestinationNewStyleNames.push_back(filterName);
    expectedDestinationNewStyleNames.push_back(filterName + L" Control");
    expectedDestinationNewStyleNames.push_back(filterName + L" Ext Out");
    expectedDestinationNewStyleNames.push_back(filterName + L" Ext Out");

    // for UMP devices, there's no filter name that's separate from the parent device name. They are the same.
    std::vector<std::wstring> expectedDestinationClassicNames{};
    expectedDestinationClassicNames.push_back(parentDeviceName);
    expectedDestinationClassicNames.push_back(L"MIDIOUT2 (" + parentDeviceName + L")");
    expectedDestinationClassicNames.push_back(L"MIDIOUT3 (" + parentDeviceName + L")");
    expectedDestinationClassicNames.push_back(L"MIDIOUT4 (" + parentDeviceName + L")");


    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceClassicNames.size());
    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceNewStyleNames.size());

    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationClassicNames.size());
    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationNewStyleNames.size());


    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForNativeUmpDevice(
            groupIndex,
            MidiFlow::MidiFlowIn,
            L"",                   // custom name
            parentDeviceName,
            filterName,
            sourceBlockNames[groupIndex],
            portIndexSource
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForNativeUmpDevice(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            parentDeviceName,
            filterName,
            destinationBlockNames[groupIndex],
            portIndexDestination
        ));

        portIndexDestination++;
    }

    // validate all the names

    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        auto entry = table.GetSourceEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        auto entry = table.GetDestinationEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
    }
}

void NamingTests::TestPopulateEntryForMidi1DeviceUsingUmpDriver()
{
    MidiEndpointNameTable table;
    uint8_t portIndexSource{ 0 };
    uint8_t portIndexDestination{ 0 };

    std::wstring parentDeviceName { L"SomeSynth"};

    std::vector<std::wstring> sourceBlockNames{};
    sourceBlockNames.push_back(L"SomeSynth MIDI");
    sourceBlockNames.push_back(L"SomeSynth Synth");
    sourceBlockNames.push_back(L"SomeSynth Synth");
    sourceBlockNames.push_back(L"SomeSynth Synth");
    sourceBlockNames.push_back(L"SomeSynth Ext In");
    sourceBlockNames.push_back(L"SomeSynth Ext In");

    // names should flow through unchanged from the block name. This may need to change in the future for classic names only
    std::vector<std::wstring> expectedSourceNewStyleNames{};
    expectedSourceNewStyleNames.push_back(L"SomeSynth MIDI");
    expectedSourceNewStyleNames.push_back(L"SomeSynth Synth");
    expectedSourceNewStyleNames.push_back(L"SomeSynth Synth");
    expectedSourceNewStyleNames.push_back(L"SomeSynth Synth");
    expectedSourceNewStyleNames.push_back(L"SomeSynth Ext In");
    expectedSourceNewStyleNames.push_back(L"SomeSynth Ext In");

    // classic names are same as new-style for a MIDI 1 device using the UMP driver
    std::vector<std::wstring> expectedSourceClassicNames{};
    expectedSourceClassicNames.push_back(L"SomeSynth");
    expectedSourceClassicNames.push_back(L"MIDIIN2 (SomeSynth)");
    expectedSourceClassicNames.push_back(L"MIDIIN3 (SomeSynth)");
    expectedSourceClassicNames.push_back(L"MIDIIN4 (SomeSynth)");
    expectedSourceClassicNames.push_back(L"MIDIIN5 (SomeSynth)");
    expectedSourceClassicNames.push_back(L"MIDIIN6 (SomeSynth)");


    std::vector<std::wstring> destinationBlockNames{};
    destinationBlockNames.push_back(L"SomeSynth MIDI");
    destinationBlockNames.push_back(L"SomeSynth Control");
    destinationBlockNames.push_back(L"SomeSynth Ext Out");
    destinationBlockNames.push_back(L"SomeSynth Ext Out");

    std::vector<std::wstring> expectedDestinationNewStyleNames{};
    expectedDestinationNewStyleNames.push_back(L"SomeSynth MIDI");
    expectedDestinationNewStyleNames.push_back(L"SomeSynth Control");
    expectedDestinationNewStyleNames.push_back(L"SomeSynth Ext Out");
    expectedDestinationNewStyleNames.push_back(L"SomeSynth Ext Out");

    std::vector<std::wstring> expectedDestinationClassicNames{};
    expectedDestinationClassicNames.push_back(L"SomeSynth");
    expectedDestinationClassicNames.push_back(L"MIDIOUT2 (SomeSynth)");
    expectedDestinationClassicNames.push_back(L"MIDIOUT3 (SomeSynth)");
    expectedDestinationClassicNames.push_back(L"MIDIOUT4 (SomeSynth)");


    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceClassicNames.size());
    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceNewStyleNames.size());

    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationClassicNames.size());
    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationNewStyleNames.size());


    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowIn,
            L"",                   // custom name
            parentDeviceName,
            sourceBlockNames[groupIndex],
            groupIndex
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            parentDeviceName,
            destinationBlockNames[groupIndex],
            groupIndex
        ));

        portIndexDestination++;
    }

    // validate all the names

    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        auto entry = table.GetSourceEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        auto entry = table.GetDestinationEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
    }



}


// A name with a character outside ASCII used to be formatted through a narrow string and widened
// one byte at a time, so "Pete\u2019s MacBook Pro" reached WinMM as its UTF-8 bytes. Only the
// ports after the first were affected, because only those get the MIDIIN/MIDIOUT prefix.
void NamingTests::TestLegacyNameKeepsNonAsciiCharacters()
{
    MidiEndpointNameTable table;

    std::wstring const deviceName{ L"Pete\u2019s Mac" };

    for (uint8_t groupIndex = 0; groupIndex < 3; groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForNativeUmpDevice(
            groupIndex, MidiFlow::MidiFlowIn, L"", deviceName, deviceName, L"MIDI", groupIndex));

        VERIFY_SUCCEEDED(table.PopulateEntryForNativeUmpDevice(
            groupIndex, MidiFlow::MidiFlowOut, L"", deviceName, deviceName, L"MIDI", groupIndex));
    }

    // the first port carries the name alone, and never went through the conversion
    VERIFY_ARE_EQUAL(table.GetSourceEntry(0)->LegacyWinMMName, deviceName);
    VERIFY_ARE_EQUAL(table.GetDestinationEntry(0)->LegacyWinMMName, deviceName);

    VERIFY_ARE_EQUAL(table.GetSourceEntry(1)->LegacyWinMMName, L"MIDIIN2 (" + deviceName + L")");
    VERIFY_ARE_EQUAL(table.GetSourceEntry(2)->LegacyWinMMName, L"MIDIIN3 (" + deviceName + L")");

    VERIFY_ARE_EQUAL(table.GetDestinationEntry(1)->LegacyWinMMName, L"MIDIOUT2 (" + deviceName + L")");
    VERIFY_ARE_EQUAL(table.GetDestinationEntry(2)->LegacyWinMMName, L"MIDIOUT3 (" + deviceName + L")");

    // the byte-at-a-time widening turned one character into three, so the length catches the
    // regression even somewhere the difference cannot be seen
    size_t const expectedLength = deviceName.length() + wcslen(L"MIDIIN2 ()");

    VERIFY_ARE_EQUAL(wcslen(table.GetSourceEntry(1)->LegacyWinMMName), expectedLength);
}

void NamingTests::TestPopulateEntryForMidi1DeviceUsingMidi1Driver()
{
     MidiEndpointNameTable table;

     uint8_t portIndexSource{0};
     uint8_t portIndexDestination{0};

     // name from the registry isn't always correct, so keeping it different from filterName is important for this thest
     std::wstring nameFromRegistry{ L"TEST DEVICE OLD" };   
     std::wstring filterName{ L"TEST DEVICE" };

     std::vector<std::wstring> sourcePinNames{};
     sourcePinNames.push_back(L"MIDI");
     sourcePinNames.push_back(L"Synth [0]");
     sourcePinNames.push_back(L"Synth [1]");
     sourcePinNames.push_back(L"Synth [2]");
     sourcePinNames.push_back(L"Ext In [0]");
     sourcePinNames.push_back(L"Ext In [1]");

     std::vector<std::wstring> expectedSourceClassicNames{};
     expectedSourceClassicNames.push_back(nameFromRegistry);
     expectedSourceClassicNames.push_back(L"MIDIIN2 (" + nameFromRegistry + L")");
     expectedSourceClassicNames.push_back(L"MIDIIN3 (" + nameFromRegistry + L")");
     expectedSourceClassicNames.push_back(L"MIDIIN4 (" + nameFromRegistry + L")");
     expectedSourceClassicNames.push_back(L"MIDIIN5 (" + nameFromRegistry + L")");
     expectedSourceClassicNames.push_back(L"MIDIIN6 (" + nameFromRegistry + L")");

     std::vector<std::wstring> expectedSourceNewStyleNames{};
     expectedSourceNewStyleNames.push_back(filterName);
     expectedSourceNewStyleNames.push_back(filterName + L" Synth");
     expectedSourceNewStyleNames.push_back(filterName + L" Synth");
     expectedSourceNewStyleNames.push_back(filterName + L" Synth");
     expectedSourceNewStyleNames.push_back(filterName + L" Ext In");
     expectedSourceNewStyleNames.push_back(filterName + L" Ext In");



     std::vector<std::wstring> destinationPinNames{};
     destinationPinNames.push_back(L"MIDI");
     destinationPinNames.push_back(L"Control");
     destinationPinNames.push_back(L"Ext Out [0]");
     destinationPinNames.push_back(L"Ext Out [1]");

     std::vector<std::wstring> expectedDestinationClassicNames{};
     expectedDestinationClassicNames.push_back(nameFromRegistry);
     expectedDestinationClassicNames.push_back(L"MIDIOUT2 (" + nameFromRegistry + L")");
     expectedDestinationClassicNames.push_back(L"MIDIOUT3 (" + nameFromRegistry + L")");
     expectedDestinationClassicNames.push_back(L"MIDIOUT4 (" + nameFromRegistry + L")");

     std::vector<std::wstring> expectedDestinationNewStyleNames{};
     expectedDestinationNewStyleNames.push_back(filterName);
     expectedDestinationNewStyleNames.push_back(filterName + L" Control");
     expectedDestinationNewStyleNames.push_back(filterName + L" Ext Out");
     expectedDestinationNewStyleNames.push_back(filterName + L" Ext Out");


     VERIFY_ARE_EQUAL(sourcePinNames.size(), expectedSourceClassicNames.size());
     VERIFY_ARE_EQUAL(sourcePinNames.size(), expectedSourceNewStyleNames.size());

     VERIFY_ARE_EQUAL(destinationPinNames.size(), expectedDestinationClassicNames.size());
     VERIFY_ARE_EQUAL(destinationPinNames.size(), expectedDestinationNewStyleNames.size());


     for (uint8_t groupIndex = 0; groupIndex < sourcePinNames.size(); groupIndex++)
     {
         VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingMidi1Driver(
             groupIndex,
             MidiFlow::MidiFlowIn,
             L"",                   // custom name
             nameFromRegistry,
             filterName,
             sourcePinNames[groupIndex],
             portIndexSource
             ));

         portIndexSource++;
     }

     for (uint8_t groupIndex = 0; groupIndex < destinationPinNames.size(); groupIndex++)
     {
         VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingMidi1Driver(
             groupIndex,
             MidiFlow::MidiFlowOut,
             L"",                   // custom name
             nameFromRegistry,
             filterName,
             destinationPinNames[groupIndex],
             portIndexDestination
         ));

         portIndexDestination++;
     }

     // validate all the names

     for (uint8_t groupIndex = 0; groupIndex < sourcePinNames.size(); groupIndex++)
     {
         auto entry = table.GetSourceEntry(groupIndex);

         VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
         VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
     }

     for (uint8_t groupIndex = 0; groupIndex < destinationPinNames.size(); groupIndex++)
     {
         auto entry = table.GetDestinationEntry(groupIndex);

         VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
         VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
     }
}


void NamingTests::TestValidateMidi1DriverAndMidi2DriverCreateCompatibleLegacyNames()
{
    MidiEndpointNameTable tableUmpDriver;
    MidiEndpointNameTable tableMidi1Driver;

    uint8_t portIndexSource{ 0 };
    uint8_t portIndexDestination{ 0 };

    // this is using real-world device data
    std::wstring nameFromRegistry{ L"nanoKEY Fold" };
    std::wstring filterName{ L"nanoKEY Fold" };

    std::vector<std::wstring> sourceBlockNames{};
    sourceBlockNames.push_back(L"nanoKEY Fold _ KEYBOARD/CTRL");

    std::vector<std::wstring> expectedSourceClassicNames{};
    expectedSourceClassicNames.push_back(nameFromRegistry);

    std::vector<std::wstring> expectedSourceNewStyleNames{};
    expectedSourceNewStyleNames.push_back(L"nanoKEY Fold _ KEYBOARD/CTRL");


    std::vector<std::wstring> destinationBlockNames{};
    destinationBlockNames.push_back(L"nanoKEY Fold _ CTRL");

    std::vector<std::wstring> expectedDestinationClassicNames{};
    expectedDestinationClassicNames.push_back(nameFromRegistry);

    std::vector<std::wstring> expectedDestinationNewStyleNames{};
    expectedDestinationNewStyleNames.push_back(L"nanoKEY Fold _ CTRL");

    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceClassicNames.size());
    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceNewStyleNames.size());

    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationClassicNames.size());
    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationNewStyleNames.size());


    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(tableUmpDriver.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowIn,
            L"",                   // custom name
            nameFromRegistry,
            sourceBlockNames[groupIndex],
            portIndexSource
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(tableUmpDriver.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            nameFromRegistry,
            destinationBlockNames[groupIndex],
            portIndexDestination
        ));

        portIndexDestination++;
    }

    // to begin with, validate all the names are what are expected

    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        auto entry = tableUmpDriver.GetSourceEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        auto entry = tableUmpDriver.GetDestinationEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
    }


    // Now populate using simulated Midi1 driver path and validate that it produces the same names

    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(tableMidi1Driver.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            groupIndex,
            MidiFlow::MidiFlowIn,
            L"",                   // custom name
            nameFromRegistry,
            filterName,
            sourceBlockNames[groupIndex],
            portIndexSource
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(tableMidi1Driver.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            nameFromRegistry,
            filterName,
            destinationBlockNames[groupIndex],
            portIndexDestination
        ));

        portIndexDestination++;
    }

    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        auto entry = tableUmpDriver.GetSourceEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        auto entry = tableUmpDriver.GetDestinationEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
    }

}


// GTB name is generated using the New Style name from the name table.
// So we do some testing for that here.
void NamingTests::TestGitHubIssue652()
{
    MidiEndpointNameTable table;

    uint8_t portIndexSource{ 0 };
    uint8_t portIndexDestination{ 0 };

    // name from the registry isn't always correct, so keeping it different from filterName is important for this thest
    std::wstring nameFromRegistry{ L"MIDIMATE II" };
    std::wstring filterName{ L"MIDIMATE II" };

    std::vector<std::wstring> sourcePinNames{};
    sourcePinNames.push_back(L"MIDIMATE II [1]");
    sourcePinNames.push_back(L"MIDIMATE II [3]");

    std::vector<std::wstring> expectedSourceClassicNames{};
    expectedSourceClassicNames.push_back(nameFromRegistry);
    expectedSourceClassicNames.push_back(L"MIDIIN2 (" + nameFromRegistry + L")");

    std::vector<std::wstring> expectedSourceNewStyleNames{};
    expectedSourceNewStyleNames.push_back(nameFromRegistry);
    expectedSourceNewStyleNames.push_back(nameFromRegistry + L" 2");



    std::vector<std::wstring> destinationPinNames{};
    destinationPinNames.push_back(L"MIDIMATE II [0]");
    destinationPinNames.push_back(L"MIDIMATE II [2]");

    std::vector<std::wstring> expectedDestinationClassicNames{};
    expectedDestinationClassicNames.push_back(nameFromRegistry);
    expectedDestinationClassicNames.push_back(L"MIDIOUT2 (" + nameFromRegistry + L")");


    std::vector<std::wstring> expectedDestinationNewStyleNames{};
    expectedDestinationNewStyleNames.push_back(nameFromRegistry);
    expectedDestinationNewStyleNames.push_back(nameFromRegistry + L" 2");


    VERIFY_ARE_EQUAL(sourcePinNames.size(), expectedSourceClassicNames.size());
    VERIFY_ARE_EQUAL(sourcePinNames.size(), expectedSourceNewStyleNames.size());

    VERIFY_ARE_EQUAL(destinationPinNames.size(), expectedDestinationClassicNames.size());
    VERIFY_ARE_EQUAL(destinationPinNames.size(), expectedDestinationNewStyleNames.size());


    for (uint8_t groupIndex = 0; groupIndex < sourcePinNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            groupIndex,
            MidiFlow::MidiFlowIn,
            L"",                   // custom name
            nameFromRegistry,
            filterName,
            sourcePinNames[groupIndex],
            portIndexSource
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationPinNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            nameFromRegistry,
            filterName,
            destinationPinNames[groupIndex],
            portIndexDestination
        ));

        portIndexDestination++;
    }

    // validate all the names

    for (uint8_t groupIndex = 0; groupIndex < sourcePinNames.size(); groupIndex++)
    {
        auto entry = table.GetSourceEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationPinNames.size(); groupIndex++)
    {
        auto entry = table.GetDestinationEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
    }
}

// GTB names come directly from the driver in the case of KS, so
// we can only validate the port name approach here.
// https://github.com/microsoft/MIDI/issues/616
void NamingTests::TestGitHubIssue616()
{
    MidiEndpointNameTable table;

    uint8_t portIndexSource{ 0 };
    uint8_t portIndexDestination{ 0 };

    // name from the registry isn't always correct, so keeping it different from filterName is important for this thest
    std::wstring filterName{ L"YAMAHA DTX-MULTI 12" };

    std::vector<std::wstring> sourceBlockNames{};
    sourceBlockNames.push_back(filterName);
    sourceBlockNames.push_back(filterName);
    sourceBlockNames.push_back(filterName);

    std::vector<std::wstring> expectedSourceClassicNames{};
    expectedSourceClassicNames.push_back(filterName);
    expectedSourceClassicNames.push_back(L"MIDIIN2 (" + filterName + L")");
    expectedSourceClassicNames.push_back(L"MIDIIN3 (" + filterName + L")");

    std::vector<std::wstring> expectedSourceNewStyleNames{};
    expectedSourceNewStyleNames.push_back(filterName);
    expectedSourceNewStyleNames.push_back(filterName + L" 2");
    expectedSourceNewStyleNames.push_back(filterName + L" 3");



    std::vector<std::wstring> destinationBlockNames{};
    destinationBlockNames.push_back(filterName);
    destinationBlockNames.push_back(filterName);
    destinationBlockNames.push_back(filterName);

    std::vector<std::wstring> expectedDestinationClassicNames{};
    expectedDestinationClassicNames.push_back(filterName);
    expectedDestinationClassicNames.push_back(L"MIDIOUT2 (" + filterName + L")");
    expectedDestinationClassicNames.push_back(L"MIDIOUT3 (" + filterName + L")");


    std::vector<std::wstring> expectedDestinationNewStyleNames{};
    expectedDestinationNewStyleNames.push_back(filterName);
    expectedDestinationNewStyleNames.push_back(filterName + L" 2");
    expectedDestinationNewStyleNames.push_back(filterName + L" 3");


    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceClassicNames.size());
    VERIFY_ARE_EQUAL(sourceBlockNames.size(), expectedSourceNewStyleNames.size());

    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationClassicNames.size());
    VERIFY_ARE_EQUAL(destinationBlockNames.size(), expectedDestinationNewStyleNames.size());


    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowIn,
            L"",                   // custom name
            filterName,
            sourceBlockNames[groupIndex],
            portIndexSource
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            filterName,
            destinationBlockNames[groupIndex],
            portIndexDestination
        ));

        portIndexDestination++;
    }

    // validate all the names

    for (uint8_t groupIndex = 0; groupIndex < sourceBlockNames.size(); groupIndex++)
    {
        auto entry = table.GetSourceEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedSourceClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedSourceNewStyleNames[groupIndex]);
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        auto entry = table.GetDestinationEntry(groupIndex);

        VERIFY_ARE_EQUAL(entry->LegacyWinMMName, expectedDestinationClassicNames[groupIndex]);
        VERIFY_ARE_EQUAL(entry->NewStyleName, expectedDestinationNewStyleNames[groupIndex]);
    }
}

bool NamingTests::ClassSetup()
{
    PrintStagingStates();

    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    return true;
}


// ============================================================================================
// Device fixtures for new-style naming.
//
// Each entry is one real device, transcribed from `midiksinfo` output plus the USB descriptors.
// To add a device, add a row. No hardware is needed to run these.
//
//   EndpointName ........ what section 1 of the naming article produces: the custom name, the
//                         in-protocol UMP endpoint name, or the product name from the USB device
//                         node (not the &MI_xx interface node).
//   PerFilterRegistry ... true when the driver gives each filter its own MediaCategories entry.
//                         False for a single device-wide entry, because that is shared by every
//                         unit of the model and holds whichever unit wrote it last.
//   Ports ............... group index, direction, then the jack name, the MediaCategories name and
//                         the KS filter name exactly as reported. For a device on the MIDI 2.0
//                         driver, put the group terminal block or function block name in the jack
//                         field and leave the other two empty.
// ============================================================================================

namespace
{
    struct NamingFixturePort
    {
        uint8_t Group;
        MidiFlow Flow;
        std::wstring PinName;
        std::wstring RegistryName;
        std::wstring FilterName;
    };

    struct NamingDeviceFixture
    {
        std::wstring Description;
        std::wstring EndpointName;
        bool PerFilterRegistry;
        std::vector<NamingFixturePort> Ports;
        std::vector<std::wstring> ExpectedSources;          // MIDI In, in group order
        std::vector<std::wstring> ExpectedDestinations;     // MIDI Out, in group order

        // false only for transports whose ports never existed under WinMM names
        bool HasLegacyEquivalent{ true };
    };

    std::vector<NamingFixturePort> MakeRepeatedPorts(
        std::wstring const& pinFormat,
        std::wstring const& registryName,
        std::wstring const& filterName,
        uint8_t const count)
    {
        std::vector<NamingFixturePort> ports{ };

        for (uint8_t i = 0; i < count; i++)
        {
            std::wstring pin{ pinFormat };

            auto position = pin.find(L"{}");
            if (position != std::wstring::npos)
            {
                pin.replace(position, 2, std::to_wstring(i));
            }

            ports.push_back({ i, MidiFlow::MidiFlowIn, pin, registryName, filterName });
            ports.push_back({ i, MidiFlow::MidiFlowOut, pin, registryName, filterName });
        }

        return ports;
    }

    std::vector<std::wstring> RepeatName(std::wstring const& format, uint8_t const count)
    {
        std::vector<std::wstring> names{ };

        for (uint8_t i = 0; i < count; i++)
        {
            std::wstring name{ format };

            auto position = name.find(L"{}");
            if (position != std::wstring::npos)
            {
                name.replace(position, 2, std::to_wstring(i + 1));
            }

            names.push_back(name);
        }

        return names;
    }

    std::vector<NamingDeviceFixture> GetDeviceFixtures()
    {
        std::vector<NamingDeviceFixture> fixtures{ };

        // -- devices whose jacks are named ------------------------------------------------------

        // KMI SoftStep. One filter, three named output jacks and one named input jack. The jack
        // names do not include the product name, so it is prepended.
        fixtures.push_back({
            L"KMI SoftStep",
            L"SoftStep",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Control Surface", L"SoftStep", L"SoftStep" },
                { 0, MidiFlow::MidiFlowOut, L"Control Surface", L"SoftStep", L"SoftStep" },
                { 1, MidiFlow::MidiFlowOut, L"TRS MIDI Out",    L"SoftStep", L"SoftStep" },
                { 2, MidiFlow::MidiFlowOut, L"CV Out",          L"SoftStep", L"SoftStep" },
            },
            { L"SoftStep Control Surface" },
            { L"SoftStep Control Surface", L"SoftStep TRS MIDI Out", L"SoftStep CV Out" },
            });

        // Ableton Push 3. The product name has to come from the USB device node; the &MI_04
        // interface node reports "Ableton Push 3 MIDI", and those five extra characters would push
        // the longest port over 31 and cost every port its product name.
        fixtures.push_back({
            L"Ableton Push 3",
            L"Ableton Push 3",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Live Port",     L"Ableton Push 3 MIDI", L"Ableton Push 3 MIDI" },
                { 1, MidiFlow::MidiFlowIn,  L"User Port",     L"Ableton Push 3 MIDI", L"Ableton Push 3 MIDI" },
                { 2, MidiFlow::MidiFlowIn,  L"External Port", L"Ableton Push 3 MIDI", L"Ableton Push 3 MIDI" },
                { 0, MidiFlow::MidiFlowOut, L"Live Port",     L"Ableton Push 3 MIDI", L"Ableton Push 3 MIDI" },
                { 1, MidiFlow::MidiFlowOut, L"User Port",     L"Ableton Push 3 MIDI", L"Ableton Push 3 MIDI" },
                { 2, MidiFlow::MidiFlowOut, L"External Port", L"Ableton Push 3 MIDI", L"Ableton Push 3 MIDI" },
            },
            { L"Ableton Push 3 Live Port", L"Ableton Push 3 User Port", L"Ableton Push 3 External Port" },
            { L"Ableton Push 3 Live Port", L"Ableton Push 3 User Port", L"Ableton Push 3 External Port" },
            });

        // Blokas Midihub with the product name in every jack name. The product name is not added
        // a second time.
        fixtures.push_back({
            L"Blokas Midihub",
            L"Studio Hub",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Studio Hub A", L"Studio Hub", L"Studio Hub" },
                { 1, MidiFlow::MidiFlowIn,  L"Studio Hub B", L"Studio Hub", L"Studio Hub" },
                { 2, MidiFlow::MidiFlowIn,  L"Studio Hub C", L"Studio Hub", L"Studio Hub" },
                { 3, MidiFlow::MidiFlowIn,  L"Studio Hub D", L"Studio Hub", L"Studio Hub" },
                { 0, MidiFlow::MidiFlowOut, L"Studio Hub A", L"Studio Hub", L"Studio Hub" },
                { 1, MidiFlow::MidiFlowOut, L"Studio Hub B", L"Studio Hub", L"Studio Hub" },
                { 2, MidiFlow::MidiFlowOut, L"Studio Hub C", L"Studio Hub", L"Studio Hub" },
                { 3, MidiFlow::MidiFlowOut, L"Studio Hub D", L"Studio Hub", L"Studio Hub" },
            },
            { L"Studio Hub A", L"Studio Hub B", L"Studio Hub C", L"Studio Hub D" },
            { L"Studio Hub A", L"Studio Hub B", L"Studio Hub C", L"Studio Hub D" },
            });

        // The same model with its jacks renamed on the device by the customer. The product name
        // plus the longest jack name does not fit, so it is dropped for the whole direction rather
        // than for only the port that overflowed. The customer's own text is left exactly as typed.
        fixtures.push_back({
            L"Blokas Midihub, jacks renamed on the device",
            L"Midihub MH-3MDTRP0",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"RENAMED USB A OUT", L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 1, MidiFlow::MidiFlowIn,  L"B OUTPUT",          L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 2, MidiFlow::MidiFlowIn,  L"CCC OUTPUT",        L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 3, MidiFlow::MidiFlowIn,  L"D OUTPUT",          L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 0, MidiFlow::MidiFlowOut, L"RENAMED USB A IN",  L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 1, MidiFlow::MidiFlowOut, L"B RENAMED IN",      L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 2, MidiFlow::MidiFlowOut, L"CCC INPUT",         L"Studio Hub", L"Midihub MH-3MDTRP0" },
                { 3, MidiFlow::MidiFlowOut, L"D INPUT",           L"Studio Hub", L"Midihub MH-3MDTRP0" },
            },
            { L"RENAMED USB A OUT", L"B OUTPUT", L"CCC OUTPUT", L"D OUTPUT" },
            { L"RENAMED USB A IN", L"B RENAMED IN", L"CCC INPUT", L"D INPUT" },
            });

        // -- devices whose per-filter driver entry is the only useful name ----------------------

        // Steinberg CMC-QC. Every Yamaha device reports the same product description, and every
        // filter is called "Yamaha USB-MIDI-n". Only the MediaCategories entries identify it, and
        // they are usable because each filter has its own.
        fixtures.push_back({
            L"Steinberg CMC-QC",
            L"Steinberg CMC-QC",
            true,
            {
                { 0, MidiFlow::MidiFlowIn,  L"MIDI", L"Steinberg CMC-QC-1", L"Yamaha USB-MIDI-1" },
                { 1, MidiFlow::MidiFlowIn,  L"MIDI", L"Steinberg CMC-QC-2", L"Yamaha USB-MIDI-2" },
                { 2, MidiFlow::MidiFlowIn,  L"MIDI", L"Steinberg CMC-QC-3", L"Yamaha USB-MIDI-3" },
                { 0, MidiFlow::MidiFlowOut, L"MIDI", L"Steinberg CMC-QC-1", L"Yamaha USB-MIDI-1" },
                { 1, MidiFlow::MidiFlowOut, L"MIDI", L"Steinberg CMC-QC-2", L"Yamaha USB-MIDI-2" },
                { 2, MidiFlow::MidiFlowOut, L"MIDI", L"Steinberg CMC-QC-3", L"Yamaha USB-MIDI-3" },
            },
            { L"Steinberg CMC-QC-1", L"Steinberg CMC-QC-2", L"Steinberg CMC-QC-3" },
            { L"Steinberg CMC-QC-1", L"Steinberg CMC-QC-2", L"Steinberg CMC-QC-3" },
            });

        // teVirtualMIDI. Four filters, each with its own entry, and a product description far too
        // long to prefix. The established port names are left intact.
        fixtures.push_back({
            L"teVirtualMIDI / loopMIDI",
            L"teVirtualMIDI - Virtual MIDI Driver x64",
            true,
            {
                { 0, MidiFlow::MidiFlowIn,  L"",     L"loopMIDI Port", L"loopMIDI Port" },
                { 1, MidiFlow::MidiFlowIn,  L"",     L"MIDI A",        L"MIDI A" },
                { 2, MidiFlow::MidiFlowIn,  L"",     L"MIDI B",        L"MIDI B" },
                { 3, MidiFlow::MidiFlowIn,  L"",     L"MIDI C",        L"MIDI C" },
                { 0, MidiFlow::MidiFlowOut, L"MIDI", L"loopMIDI Port", L"loopMIDI Port" },
                { 1, MidiFlow::MidiFlowOut, L"MIDI", L"MIDI A",        L"MIDI A" },
                { 2, MidiFlow::MidiFlowOut, L"MIDI", L"MIDI B",        L"MIDI B" },
                { 3, MidiFlow::MidiFlowOut, L"MIDI", L"MIDI C",        L"MIDI C" },
            },
            { L"loopMIDI Port", L"MIDI A", L"MIDI B", L"MIDI C" },
            { L"loopMIDI Port", L"MIDI A", L"MIDI B", L"MIDI C" },
            });

        // -- devices named by their filters ------------------------------------------------------

        // RME HDSPe MADI FX. No jack names, but each port is its own filter and those are named.
        // The filter names repeat most of the product name, so it is not added again.
        fixtures.push_back({
            L"RME HDSPe MADI FX",
            L"RME HDSPe MADI FX",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MADI1 MIDI" },
                { 1, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MIDI" },
                { 2, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MADI2 MIDI" },
                { 3, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MADI3 MIDI" },
                { 0, MidiFlow::MidiFlowOut, L"MIDI", L"", L"HDSPe FX MADI1 MIDI" },
                { 1, MidiFlow::MidiFlowOut, L"MIDI", L"", L"HDSPe FX MIDI" },
                { 2, MidiFlow::MidiFlowOut, L"MIDI", L"", L"HDSPe FX MADI2 MIDI" },
                { 3, MidiFlow::MidiFlowOut, L"MIDI", L"", L"HDSPe FX MADI3 MIDI" },
            },
            { L"HDSPe FX MADI1 MIDI", L"HDSPe FX MIDI", L"HDSPe FX MADI2 MIDI", L"HDSPe FX MADI3 MIDI" },
            { L"HDSPe FX MADI1 MIDI", L"HDSPe FX MIDI", L"HDSPe FX MADI2 MIDI", L"HDSPe FX MADI3 MIDI" },
            });

        // MOTU Express 128. Eight filters, each named for its port. The endpoint name has already
        // been recovered from the shared leading run of those filter names, because the driver
        // describes every MOTU interface identically.
        {
            NamingDeviceFixture motu{ L"MOTU Express 128", L"Express  128", false, { }, { }, { } };

            for (uint8_t i = 0; i < 8; i++)
            {
                std::wstring filter{ L"Express  128: Port " + std::to_wstring(i + 1) };

                motu.Ports.push_back({ i, MidiFlow::MidiFlowIn,  L"MIDI", L"", filter });
                motu.Ports.push_back({ i, MidiFlow::MidiFlowOut, L"MIDI", L"", filter });
                motu.ExpectedSources.push_back(filter);
                motu.ExpectedDestinations.push_back(filter);
            }

            fixtures.push_back(motu);
        }

        // -- devices that say nothing about their individual ports -------------------------------

        // Roland UM-ONE. The jack names are the filter name with an index our own stack appended,
        // so there is nothing here the product name does not already say.
        fixtures.push_back({
            L"Roland UM-ONE",
            L"UM-ONE",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"UM-ONE [1]", L"UM-ONE", L"UM-ONE" },
                { 0, MidiFlow::MidiFlowOut, L"UM-ONE [0]", L"UM-ONE", L"UM-ONE" },
            },
            { L"UM-ONE" },
            { L"UM-ONE" },
            });

        // Native Instruments KOMPLETE KONTROL M32. The product name comes from the USB device node
        // and is shorter than the interface name by the trailing "MIDI".
        fixtures.push_back({
            L"NI KOMPLETE KONTROL M32",
            L"KOMPLETE KONTROL M32",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"KOMPLETE KONTROL M32 MIDI [1]", L"KOMPLETE KONTROL M32 MIDI", L"KOMPLETE KONTROL M32 MIDI" },
                { 0, MidiFlow::MidiFlowOut, L"KOMPLETE KONTROL M32 MIDI [0]", L"KOMPLETE KONTROL M32 MIDI", L"KOMPLETE KONTROL M32 MIDI" },
            },
            { L"KOMPLETE KONTROL M32" },
            { L"KOMPLETE KONTROL M32" },
            });

        // ESI M8U eX. Sixteen ports each way and nothing to tell them apart, which is the case the
        // group numbering exists for. Port 10 reports "[:]" rather than "[10]" because the index
        // was formatted as a character.
        {
            NamingDeviceFixture esi{ L"ESI M8U eX", L"ESI M8U eX", false, { }, { }, { } };

            for (uint8_t i = 0; i < 16; i++)
            {
                std::wstring inPin{ L"ESI M8U eX [" + std::to_wstring(i + 16) + L"]" };
                std::wstring outPin{ i == 10 ? std::wstring{ L"ESI M8U eX [:]" } : L"ESI M8U eX [" + std::to_wstring(i) + L"]" };

                esi.Ports.push_back({ i, MidiFlow::MidiFlowIn,  inPin,  L"ESI M8U eX", L"ESI M8U eX" });
                esi.Ports.push_back({ i, MidiFlow::MidiFlowOut, outPin, L"ESI M8U eX", L"ESI M8U eX" });
            }

            esi.ExpectedSources = RepeatName(L"ESI M8U eX group {}", 16);
            esi.ExpectedDestinations = esi.ExpectedSources;

            fixtures.push_back(esi);
        }

        // -- devices on the MIDI 2.0 driver, named by their blocks --------------------------------

        // Akai MPK mini IV. The group terminal block names already carry the product name.
        fixtures.push_back({
            L"Akai MPK mini IV",
            L"MPK mini IV",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"MPK mini IV MIDI Port",     L"", L"" },
                { 1, MidiFlow::MidiFlowIn,  L"MPK mini IV DAW Port",      L"", L"" },
                { 2, MidiFlow::MidiFlowIn,  L"MPK mini IV Software Port", L"", L"" },
                { 0, MidiFlow::MidiFlowOut, L"MPK mini IV MIDI Port",     L"", L"" },
                { 1, MidiFlow::MidiFlowOut, L"MPK mini IV Din Port",      L"", L"" },
                { 2, MidiFlow::MidiFlowOut, L"MPK mini IV DAW Port",      L"", L"" },
                { 3, MidiFlow::MidiFlowOut, L"MPK mini IV Software Port", L"", L"" },
            },
            { L"MPK mini IV MIDI Port", L"MPK mini IV DAW Port", L"MPK mini IV Software Port" },
            { L"MPK mini IV MIDI Port", L"MPK mini IV Din Port", L"MPK mini IV DAW Port", L"MPK mini IV Software Port" },
            });

        // Moog One. A single block whose name is just the product name, so it says nothing extra.
        fixtures.push_back({
            L"Moog One",
            L"Moog One",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Moog One", L"", L"" },
                { 0, MidiFlow::MidiFlowOut, L"Moog One", L"", L"" },
            },
            { L"Moog One" },
            { L"Moog One" },
            });

        // A MIDI 2.0 device with one function block spanning two groups. The block name applies to
        // both, so the group number is what tells them apart.
        fixtures.push_back({
            L"MIDI 2.0 device, one block over two groups",
            L"Iridium",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Synth", L"", L"" },
                { 1, MidiFlow::MidiFlowIn,  L"Synth", L"", L"" },
                { 0, MidiFlow::MidiFlowOut, L"Synth", L"", L"" },
                { 1, MidiFlow::MidiFlowOut, L"Synth", L"", L"" },
            },
            { L"Iridium Synth group 1", L"Iridium Synth group 2" },
            { L"Iridium Synth group 1", L"Iridium Synth group 2" },
            });

        // A network endpoint with no blocks at all. Every port carries the endpoint name, so every
        // port is numbered.
        {
            NamingDeviceFixture network{ L"Network MIDI 2.0 host, no function blocks", L"My Bome Box", false, { }, { }, { } };
            network.HasLegacyEquivalent = false;

            for (uint8_t i = 0; i < 16; i++)
            {
                network.Ports.push_back({ i, MidiFlow::MidiFlowIn,  L"My Bome Box", L"", L"" });
                network.Ports.push_back({ i, MidiFlow::MidiFlowOut, L"My Bome Box", L"", L"" });
            }

            network.ExpectedSources = RepeatName(L"My Bome Box group {}", 16);
            network.ExpectedDestinations = network.ExpectedSources;

            fixtures.push_back(network);
        }

        // -- names too long to keep whole ---------------------------------------------------------

        // Jack names longer than the limit, distinguished only at the end. Cutting the tail would
        // destroy the only difference, so the repeated product name is removed instead and as much
        // of it as fits is put back. Only the model survives here, not the family.
        fixtures.push_back({
            L"Long jack names that repeat the product name",
            L"Montage M8x",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Montage M8x DAW Remote Control Port 1", L"", L"" },
                { 1, MidiFlow::MidiFlowIn,  L"Montage M8x DAW Remote Control Port 2", L"", L"" },
                { 0, MidiFlow::MidiFlowOut, L"Montage M8x DAW Remote Control Port 1", L"", L"" },
                { 1, MidiFlow::MidiFlowOut, L"Montage M8x DAW Remote Control Port 2", L"", L"" },
            },
            { L"M8x DAW Remote Control Port 1", L"M8x DAW Remote Control Port 2" },
            { L"M8x DAW Remote Control Port 1", L"M8x DAW Remote Control Port 2" },
            });

        // The same shape with a longer product name, where enough of it still fits to keep the
        // front of the name.
        fixtures.push_back({
            L"Long jack names, longer product name",
            L"Studio Reference Monitor Controller",
            false,
            {
                { 0, MidiFlow::MidiFlowIn,  L"Studio Reference Monitor Controller A", L"", L"" },
                { 1, MidiFlow::MidiFlowIn,  L"Studio Reference Monitor Controller B", L"", L"" },
                { 0, MidiFlow::MidiFlowOut, L"Studio Reference Monitor Controller A", L"", L"" },
                { 1, MidiFlow::MidiFlowOut, L"Studio Reference Monitor Controller B", L"", L"" },
            },
            { L"Studio Reference Monitor A", L"Studio Reference Monitor B" },
            { L"Studio Reference Monitor A", L"Studio Reference Monitor B" },
            });

        return fixtures;
    }

    std::vector<Midi1PortNameInput> ToNameInputs(NamingDeviceFixture const& fixture)
    {
        std::vector<Midi1PortNameInput> inputs{ };

        for (auto const& port : fixture.Ports)
        {
            Midi1PortNameInput input{ };

            input.GroupIndex = port.Group;
            input.DataFlowFromUserPerspective = port.Flow;
            input.PinName = port.PinName;
            input.DriverRegistryName = port.RegistryName;
            input.FilterName = port.FilterName;

            inputs.push_back(input);
        }

        return inputs;
    }

    std::vector<std::wstring> NamesForFlow(std::vector<Midi1PortNameResult> const& results, MidiFlow const flow)
    {
        std::vector<Midi1PortNameResult> matching{ };

        for (auto const& result : results)
        {
            if (result.DataFlowFromUserPerspective == flow) { matching.push_back(result); }
        }

        std::sort(matching.begin(), matching.end(),
            [](auto const& a, auto const& b) { return a.GroupIndex < b.GroupIndex; });

        std::vector<std::wstring> names{ };

        for (auto const& result : matching) { names.push_back(result.Name); }

        return names;
    }
}

void NamingTests::TestDeviceFixtures()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    for (auto const& fixture : GetDeviceFixtures())
    {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(L"Device: %s", fixture.Description.c_str()));

        auto results = BuildMidi1PortNamesForEndpoint(
            fixture.EndpointName,
            fixture.PerFilterRegistry,
            ToNameInputs(fixture));

        auto sources = NamesForFlow(results, MidiFlow::MidiFlowIn);
        auto destinations = NamesForFlow(results, MidiFlow::MidiFlowOut);

        VERIFY_ARE_EQUAL(fixture.ExpectedSources.size(), sources.size());
        VERIFY_ARE_EQUAL(fixture.ExpectedDestinations.size(), destinations.size());

        for (size_t i = 0; i < sources.size() && i < fixture.ExpectedSources.size(); i++)
        {
            WEX::Logging::Log::Comment(WEX::Common::String().Format(
                L"  in  group %d: '%s'", static_cast<int>(i + 1), sources[i].c_str()));

            VERIFY_ARE_EQUAL(fixture.ExpectedSources[i], sources[i]);
        }

        for (size_t i = 0; i < destinations.size() && i < fixture.ExpectedDestinations.size(); i++)
        {
            WEX::Logging::Log::Comment(WEX::Common::String().Format(
                L"  out group %d: '%s'", static_cast<int>(i + 1), destinations[i].c_str()));

            VERIFY_ARE_EQUAL(fixture.ExpectedDestinations[i], destinations[i]);
        }
    }
}

void NamingTests::TestDeviceFixturesProduceUsableNames()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // Properties that must hold for every device, whatever the expected strings are. A new fixture
    // gets these checks without anyone having to remember to write them.
    for (auto const& fixture : GetDeviceFixtures())
    {
        auto results = BuildMidi1PortNamesForEndpoint(
            fixture.EndpointName,
            fixture.PerFilterRegistry,
            ToNameInputs(fixture));

        VERIFY_IS_GREATER_THAN(results.size(), (size_t)0);

        for (auto const& flow : { MidiFlow::MidiFlowIn, MidiFlow::MidiFlowOut })
        {
            std::vector<std::wstring> seen{ };

            for (auto const& result : results)
            {
                if (result.DataFlowFromUserPerspective != flow) { continue; }

                if (result.Name.empty() || result.Name.length() > MAXPNAMELEN - 1)
                {
                    WEX::Logging::Log::Error(WEX::Common::String().Format(
                        L"%s: '%s' is %d characters",
                        fixture.Description.c_str(), result.Name.c_str(), static_cast<int>(result.Name.length())));
                }

                VERIFY_IS_FALSE(result.Name.empty());
                VERIFY_IS_LESS_THAN_OR_EQUAL(result.Name.length(), (size_t)(MAXPNAMELEN - 1));

                auto compare = WindowsMidiServicesInternal::ToUpperWStringCopy(result.Name);

                if (std::find(seen.begin(), seen.end(), compare) != seen.end())
                {
                    WEX::Logging::Log::Error(WEX::Common::String().Format(
                        L"%s: duplicate port name '%s'", fixture.Description.c_str(), result.Name.c_str()));
                }

                VERIFY_IS_TRUE(std::find(seen.begin(), seen.end(), compare) == seen.end());

                seen.push_back(compare);
            }
        }
    }
}

void NamingTests::TestPlaceholderPortNameDetection()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // what our own USB and KS stack inserts
    VERIFY_IS_TRUE(IsPlaceholderPortName(L""));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"MIDI"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"midi"));

    // values manufacturers commonly supply that say nothing
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"In"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"Out"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"IO"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"Port"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"Port 2"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"MIDI Out"));
    VERIFY_IS_TRUE(IsPlaceholderPortName(L"MIDI 3"));

    // real names, including ones that contain an uninformative word
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"Control Surface"));
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"TRS MIDI Out"));
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"CV Out"));
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"Live Port"));
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"loopMIDI Port"));
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"MIDI A"));
    VERIFY_IS_FALSE(IsPlaceholderPortName(L"UM-ONE"));

    // the index our stack appends is removed before the name is judged
    VERIFY_ARE_EQUAL(std::wstring{ L"SoftStep" }, RemoveGeneratedPinNameSuffix(L"SoftStep [0]"));
    VERIFY_ARE_EQUAL(std::wstring{ L"ESI M8U eX" }, RemoveGeneratedPinNameSuffix(L"ESI M8U eX [:]"));
    VERIFY_ARE_EQUAL(std::wstring{ L"ESI M8U eX" }, RemoveGeneratedPinNameSuffix(L"ESI M8U eX [31]"));

    // a device really named "[0]" keeps it, because there is no separating space
    VERIFY_ARE_EQUAL(std::wstring{ L"CircuitPython[0]" }, RemoveGeneratedPinNameSuffix(L"CircuitPython[0]"));
}

void NamingTests::TestPortNameCarriesDeviceName()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // the device name at the front
    VERIFY_IS_TRUE(PortNameCarriesDeviceName(L"MPK mini IV DAW Port", L"MPK mini IV"));
    VERIFY_IS_TRUE(PortNameCarriesDeviceName(L"Steinberg CMC-QC-1", L"Steinberg CMC-QC"));
    VERIFY_IS_TRUE(PortNameCarriesDeviceName(L"Studio Hub A", L"Studio Hub"));

    // most of the device name, rearranged
    VERIFY_IS_TRUE(PortNameCarriesDeviceName(L"HDSPe FX MADI1 MIDI", L"RME HDSPe MADI FX"));

    // unrelated names
    VERIFY_IS_FALSE(PortNameCarriesDeviceName(L"Control Surface", L"SoftStep"));
    VERIFY_IS_FALSE(PortNameCarriesDeviceName(L"Live Port", L"Ableton Push 3"));
    VERIFY_IS_FALSE(PortNameCarriesDeviceName(L"loopMIDI Port", L"teVirtualMIDI - Virtual MIDI Driver x64"));
}

void NamingTests::TestShortenDeviceNameToFit()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // whole words from the front while they fit
    VERIFY_ARE_EQUAL(std::wstring{ L"Studio Reference Monitor" }, ShortenDeviceNameToFit(L"Studio Reference Monitor Controller", 29));
    VERIFY_ARE_EQUAL(std::wstring{ L"teVirtualMIDI" }, ShortenDeviceNameToFit(L"teVirtualMIDI - Virtual MIDI Driver x64", 17));

    // nothing fits from the front, so the model is kept rather than the family
    VERIFY_ARE_EQUAL(std::wstring{ L"M8x" }, ShortenDeviceNameToFit(L"Montage M8x", 5));
    VERIFY_ARE_EQUAL(std::wstring{ L"M32" }, ShortenDeviceNameToFit(L"KOMPLETE KONTROL M32", 5));

    // a name that already fits is returned whole, punctuation intact
    VERIFY_ARE_EQUAL(std::wstring{ L"Midihub MH-3MDTRP0" }, ShortenDeviceNameToFit(L"Midihub MH-3MDTRP0", 31));

    // nothing can fit
    VERIFY_ARE_EQUAL(std::wstring{ L"" }, ShortenDeviceNameToFit(L"Montage M8x", 2));
    VERIFY_ARE_EQUAL(std::wstring{ L"" }, ShortenDeviceNameToFit(L"Montage M8x", 0));
}

void NamingTests::TestResolveDeviceSuppliedPortNamePrecedence()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // the jack name wins when it says something
    auto fromPin = ResolveDeviceSuppliedPortName(L"Control Surface", L"SoftStep", false, L"SoftStep", L"SoftStep");
    VERIFY_IS_TRUE(fromPin.Source == Midi1PortNameSource::Pin);
    VERIFY_ARE_EQUAL(std::wstring{ L"Control Surface" }, fromPin.Name);

    // the driver entry is used when the driver gives each filter its own
    auto fromRegistry = ResolveDeviceSuppliedPortName(L"MIDI", L"Steinberg CMC-QC-1", true, L"Yamaha USB-MIDI-1", L"Steinberg CMC-QC");
    VERIFY_IS_TRUE(fromRegistry.Source == Midi1PortNameSource::DriverRegistry);
    VERIFY_ARE_EQUAL(std::wstring{ L"Steinberg CMC-QC-1" }, fromRegistry.Name);

    // and is refused when it is a single device-wide entry, because two units of the model share
    // it. Here it holds the other unit's name entirely.
    auto sharedEntry = ResolveDeviceSuppliedPortName(L"MIDI", L"Studio Hub", false, L"Midihub MH-3MDTRP0", L"Midihub MH-3MDTRP0");
    VERIFY_IS_TRUE(sharedEntry.Source == Midi1PortNameSource::None);

    // the filter name is used when it is not simply the device name again
    auto fromFilter = ResolveDeviceSuppliedPortName(L"MIDI", L"", false, L"HDSPe FX MADI1 MIDI", L"RME HDSPe MADI FX");
    VERIFY_IS_TRUE(fromFilter.Source == Midi1PortNameSource::Filter);
    VERIFY_ARE_EQUAL(std::wstring{ L"HDSPe FX MADI1 MIDI" }, fromFilter.Name);

    // nothing usable anywhere
    auto nothing = ResolveDeviceSuppliedPortName(L"UM-ONE [0]", L"UM-ONE", false, L"UM-ONE", L"UM-ONE");
    VERIFY_IS_TRUE(nothing.Source == Midi1PortNameSource::None);
    VERIFY_IS_TRUE(nothing.Name.empty());
}

void NamingTests::TestPortNameSourceFlags()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }


    // This is what the Automatic style reads. A device that said nothing must not report that it
    // supplied names, or its ports would be renamed for no gain.
    for (auto const& fixture : GetDeviceFixtures())
    {
        auto results = BuildMidi1PortNamesForEndpoint(
            fixture.EndpointName,
            fixture.PerFilterRegistry,
            ToNameInputs(fixture));

        auto flags = CalculateMidi1PortNameSourceFlags(fixture.EndpointName, results);
        bool deviceSupplied = (flags & MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED) != 0;

        WEX::Logging::Log::Comment(WEX::Common::String().Format(
            L"%s: device supplied port names = %s", fixture.Description.c_str(), deviceSupplied ? L"yes" : L"no"));

        if (fixture.Description == L"Roland UM-ONE" ||
            fixture.Description == L"NI KOMPLETE KONTROL M32" ||
            fixture.Description == L"ESI M8U eX" ||
            fixture.Description == L"Moog One" ||
            fixture.Description == L"Network MIDI 2.0 host, no function blocks")
        {
            VERIFY_IS_FALSE(deviceSupplied);
        }
        else
        {
            VERIFY_IS_TRUE(deviceSupplied);
        }
    }
}


void NamingTests::TestResolveAutomaticPortNameSelection()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // An endpoint with no legacy equivalent has no older name to stay compatible with.
    VERIFY_ARE_EQUAL(Midi1PortNameSelection::UseNewStyleName,
        ResolveAutomaticPortNameSelection(false, MIDI_MIDI1_PORT_NAME_SOURCE_NONE));

    // The device said something about its individual ports, so the new name carries information.
    VERIFY_ARE_EQUAL(Midi1PortNameSelection::UseNewStyleName,
        ResolveAutomaticPortNameSelection(true, MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED));

    // The device said nothing, so renaming would break name matching and give nothing back.
    VERIFY_ARE_EQUAL(Midi1PortNameSelection::UseLegacyWinMM,
        ResolveAutomaticPortNameSelection(true, MIDI_MIDI1_PORT_NAME_SOURCE_NONE));

    // Until a transport writes the flags, every existing endpoint reads as "said nothing" with a
    // legacy equivalent, which must resolve to exactly the names that shipped.
    VERIFY_ARE_EQUAL(Midi1PortNameSelection::UseLegacyWinMM,
        ResolveAutomaticPortNameSelection(true, 0));

    // The other flags never promote on their own.
    VERIFY_ARE_EQUAL(Midi1PortNameSelection::UseLegacyWinMM,
        ResolveAutomaticPortNameSelection(true, MIDI_MIDI1_PORT_NAME_SOURCE_NAMES_ARE_DISTINCT | MIDI_MIDI1_PORT_NAME_SOURCE_ALL_PORTS_NAMED));

    // Every fixture, resolved the way the service will resolve it.
    for (auto const& fixture : GetDeviceFixtures())
    {
        auto results = BuildMidi1PortNamesForEndpoint(
            fixture.EndpointName,
            fixture.PerFilterRegistry,
            ToNameInputs(fixture));

        auto flags = CalculateMidi1PortNameSourceFlags(fixture.EndpointName, results);
        auto selection = ResolveAutomaticPortNameSelection(fixture.HasLegacyEquivalent, flags);

        WEX::Logging::Log::Comment(WEX::Common::String().Format(
            L"%s: automatic picks %s", fixture.Description.c_str(),
            selection == Midi1PortNameSelection::UseNewStyleName ? L"new style" : L"legacy"));

        // These are the devices the published table says stay on legacy names.
        bool expectLegacy =
            fixture.Description == L"Roland UM-ONE" ||
            fixture.Description == L"NI KOMPLETE KONTROL M32" ||
            fixture.Description == L"ESI M8U eX" ||
            fixture.Description == L"Moog One";

        VERIFY_ARE_EQUAL(
            expectLegacy ? Midi1PortNameSelection::UseLegacyWinMM : Midi1PortNameSelection::UseNewStyleName,
            selection);
    }
}


// The table is what the transports actually drive, and it is populated one port at a time and then
// rebuilt. This covers that seam rather than the pure builder underneath it.
void NamingTests::TestNameTableRebuild()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    std::wstring const endpointName{ L"Steinberg CMC-QC" };

    MidiEndpointNameTable table{ };

    for (uint8_t group = 0; group < 3; group++)
    {
        auto registryName = L"Steinberg CMC-QC-" + std::to_wstring(group + 1);
        auto filterName = L"Yamaha USB-MIDI-" + std::to_wstring(group + 1);

        for (auto const flow : { MidiFlow::MidiFlowIn, MidiFlow::MidiFlowOut })
        {
            VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingMidi1Driver(
                group, flow, L"", registryName, filterName, L"MIDI", 0));

            table.RecordPortInput(group, flow, L"MIDI", registryName, filterName);
        }
    }

    VERIFY_SUCCEEDED(table.RebuildNewStyleNames(endpointName, true));

    for (uint8_t group = 0; group < 3; group++)
    {
        auto expected = L"Steinberg CMC-QC-" + std::to_wstring(group + 1);

        VERIFY_ARE_EQUAL(expected, std::wstring{ table.GetSourceEntry(group)->NewStyleName });
        VERIFY_ARE_EQUAL(expected, std::wstring{ table.GetDestinationEntry(group)->NewStyleName });

        // the legacy name must be left exactly as the original generator produced it
        VERIFY_IS_GREATER_THAN(wcslen(table.GetSourceEntry(group)->LegacyWinMMName), (size_t)0);
    }

    VERIFY_ARE_NOT_EQUAL((uint32_t)MIDI_MIDI1_PORT_NAME_SOURCE_NONE,
        table.NameSourceFlags() & MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED);

    // A second pass with fewer ports, as happens when one of the filters goes away. The port that
    // remains must not be numbered against ports that no longer exist.
    table.ResetPortInputs();

    VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingMidi1Driver(
        0, MidiFlow::MidiFlowIn, L"", L"Steinberg CMC-QC-1", L"Yamaha USB-MIDI-1", L"MIDI", 0));

    table.RecordPortInput(0, MidiFlow::MidiFlowIn, L"MIDI", L"Steinberg CMC-QC-1", L"Yamaha USB-MIDI-1");

    VERIFY_SUCCEEDED(table.RebuildNewStyleNames(endpointName, true));

    VERIFY_ARE_EQUAL(std::wstring{ L"Steinberg CMC-QC-1" },
        std::wstring{ table.GetSourceEntry(0)->NewStyleName });

    // Populating nothing at all must leave the previous names alone rather than blanking them.
    table.ResetPortInputs();
    VERIFY_SUCCEEDED(table.RebuildNewStyleNames(endpointName, true));

    VERIFY_ARE_EQUAL(std::wstring{ L"Steinberg CMC-QC-1" },
        std::wstring{ table.GetSourceEntry(0)->NewStyleName });
}


// The duplicate-device marker goes on the endpoint name, and every port composes from it. This is
// the worked example in docs/kb/how-midi1-port-names-are-generated.md.
void NamingTests::TestSecondDeviceOfSameModel()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    for (auto const& fixture : GetDeviceFixtures())
    {
        if (fixture.Description != L"ESI M8U eX") continue;

        auto first = BuildMidi1PortNamesForEndpoint(
            fixture.EndpointName, fixture.PerFilterRegistry, ToNameInputs(fixture));

        // the transport hands the disambiguated endpoint name to the second unit
        auto second = BuildMidi1PortNamesForEndpoint(
            fixture.EndpointName + L" (2)", fixture.PerFilterRegistry, ToNameInputs(fixture));

        VERIFY_ARE_EQUAL(first.size(), second.size());

        for (size_t i = 0; i < first.size(); i++)
        {
            WEX::Logging::Log::Comment(WEX::Common::String().Format(
                L"%s  ->  %s", first[i].Name.c_str(), second[i].Name.c_str()));

            // no port may collide with the same port on the other unit
            VERIFY_ARE_NOT_EQUAL(first[i].Name, second[i].Name);

            VERIFY_IS_LESS_THAN_OR_EQUAL(second[i].Name.length(), (size_t)(MAXPNAMELEN - 1));
            VERIFY_IS_FALSE(second[i].Name.empty());
        }

        // and the two units' ports must be distinct as whole sets, not merely pairwise
        std::vector<std::wstring> all{ };
        for (auto const& r : first) { all.push_back(r.Name + (r.DataFlowFromUserPerspective == MidiFlow::MidiFlowIn ? L" in" : L" out")); }
        for (auto const& r : second) { all.push_back(r.Name + (r.DataFlowFromUserPerspective == MidiFlow::MidiFlowIn ? L" in" : L" out")); }

        auto originalCount = all.size();
        std::sort(all.begin(), all.end());
        all.erase(std::unique(all.begin(), all.end()), all.end());

        VERIFY_ARE_EQUAL(originalCount, all.size());

        break;
    }

    // The MOTU shape: the port names already carry the model, so the endpoint name gets dropped
    // during composition. The marker still has to survive somewhere, or the two units collide.
    {
        std::vector<Midi1PortNameInput> ports{ };

        for (uint8_t i = 0; i < 8; i++)
        {
            std::wstring filter{ L"Express  128: Port " + std::to_wstring(i + 1) };
            ports.push_back({ i, MidiFlow::MidiFlowIn, L"MIDI", L"", filter });
        }

        auto first = BuildMidi1PortNamesForEndpoint(L"Express  128", false, ports);
        auto second = BuildMidi1PortNamesForEndpoint(L"Express  128 (2)", false, ports);

        VERIFY_ARE_EQUAL(first.size(), second.size());

        for (size_t i = 0; i < first.size(); i++)
        {
            WEX::Logging::Log::Comment(WEX::Common::String().Format(
                L"%s  ->  %s", first[i].Name.c_str(), second[i].Name.c_str()));

            VERIFY_ARE_NOT_EQUAL(first[i].Name, second[i].Name);
            VERIFY_IS_LESS_THAN_OR_EQUAL(second[i].Name.length(), (size_t)(MAXPNAMELEN - 1));
        }
    }
}


// Mirrors what the service does once function blocks arrive: populate from the block, then rebuild.
// A block spanning several groups used to name every one of them identically.
void NamingTests::TestFunctionBlockSpanningSeveralGroups()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    std::wstring const endpointName{ L"Iridium" };

    MidiEndpointNameTable table{ };

    uint8_t sourceIndex{ 0 };
    uint8_t destinationIndex{ 0 };

    for (uint8_t group = 0; group < 2; group++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForNativeUmpDevice(
            group, MidiFlow::MidiFlowIn, L"", endpointName, endpointName, L"Synth", sourceIndex++));

        VERIFY_SUCCEEDED(table.PopulateEntryForNativeUmpDevice(
            group, MidiFlow::MidiFlowOut, L"", endpointName, endpointName, L"Synth", destinationIndex++));
    }

    VERIFY_SUCCEEDED(table.RebuildNewStyleNames(endpointName, false));

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium Synth group 1" },
        std::wstring{ table.GetSourceEntry(0)->NewStyleName });

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium Synth group 2" },
        std::wstring{ table.GetSourceEntry(1)->NewStyleName });

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium Synth group 1" },
        std::wstring{ table.GetDestinationEntry(0)->NewStyleName });

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium Synth group 2" },
        std::wstring{ table.GetDestinationEntry(1)->NewStyleName });

    // A block covering a single group carries no number, because nothing collides with it.
    MidiEndpointNameTable single{ };

    VERIFY_SUCCEEDED(single.PopulateEntryForNativeUmpDevice(
        0, MidiFlow::MidiFlowIn, L"", L"Acme Synth", L"Acme Synth", L"Piano", 0));

    VERIFY_SUCCEEDED(single.PopulateEntryForNativeUmpDevice(
        1, MidiFlow::MidiFlowIn, L"", L"Acme Synth", L"Acme Synth", L"Drums", 1));

    VERIFY_SUCCEEDED(single.RebuildNewStyleNames(L"Acme Synth", false));

    VERIFY_ARE_EQUAL(std::wstring{ L"Acme Synth Piano" },
        std::wstring{ single.GetSourceEntry(0)->NewStyleName });

    VERIFY_ARE_EQUAL(std::wstring{ L"Acme Synth Drums" },
        std::wstring{ single.GetSourceEntry(1)->NewStyleName });

    // The network shape: one block covering every group, named the same as the endpoint. Nothing
    // tells the ports apart, so they are numbered.
    MidiEndpointNameTable network{ };

    for (uint8_t group = 0; group < 16; group++)
    {
        VERIFY_SUCCEEDED(network.PopulateEntryForNativeUmpDevice(
            group, MidiFlow::MidiFlowIn, L"", L"My Bome Box", L"My Bome Box", L"My Bome Box", group));
    }

    VERIFY_SUCCEEDED(network.RebuildNewStyleNames(L"My Bome Box", false));

    VERIFY_ARE_EQUAL(std::wstring{ L"My Bome Box group 1" },
        std::wstring{ network.GetSourceEntry(0)->NewStyleName });

    VERIFY_ARE_EQUAL(std::wstring{ L"My Bome Box group 16" },
        std::wstring{ network.GetSourceEntry(15)->NewStyleName });
}


void NamingTests::TestModelNameRecovery()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // MOTU names every one of its devices after the driver, and only the filters carry the model.
    {
        std::vector<Midi1PortNameInput> ports{ };

        for (uint8_t i = 0; i < 8; i++)
        {
            std::wstring filter{ L"Express  128: Port " + std::to_wstring(i + 1) };

            ports.push_back({ i, MidiFlow::MidiFlowIn,  L"MIDI", L"", filter });
            ports.push_back({ i, MidiFlow::MidiFlowOut, L"MIDI", L"", filter });
        }

        // the run is "Express  128: Port"; "Port" says nothing, and the spacing the device used
        // is preserved rather than rebuilt
        VERIFY_ARE_EQUAL(std::wstring{ L"Express  128" },
            RecoverModelNameFromPortNames(L"MOTU USB MIDI Device for 64 bit Windows", false, ports));
    }

    // RME is named correctly already, and its filters repeat the model rather than adding to it.
    {
        std::vector<Midi1PortNameInput> ports{
            { 0, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MADI1 MIDI" },
            { 1, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MIDI" },
            { 2, MidiFlow::MidiFlowIn,  L"MIDI", L"", L"HDSPe FX MADI2 MIDI" },
        };

        VERIFY_ARE_EQUAL(std::wstring{ L"RME HDSPe MADI FX" },
            RecoverModelNameFromPortNames(L"RME HDSPe MADI FX", false, ports));
    }

    // Nothing to learn from: a single port, or ports which share no leading run.
    {
        std::vector<Midi1PortNameInput> one{ { 0, MidiFlow::MidiFlowIn, L"Control Surface", L"", L"SoftStep" } };

        VERIFY_ARE_EQUAL(std::wstring{ L"SoftStep" },
            RecoverModelNameFromPortNames(L"SoftStep", false, one));

        std::vector<Midi1PortNameInput> unrelated{
            { 0, MidiFlow::MidiFlowIn, L"Live Port", L"", L"Ableton Push 3" },
            { 1, MidiFlow::MidiFlowIn, L"User Port", L"", L"Ableton Push 3" },
        };

        VERIFY_ARE_EQUAL(std::wstring{ L"Ableton Push 3" },
            RecoverModelNameFromPortNames(L"Ableton Push 3", false, unrelated));
    }

    // Identical port names are the device saying one thing many times, not a model name.
    {
        std::vector<Midi1PortNameInput> same{
            { 0, MidiFlow::MidiFlowIn, L"MIDI", L"", L"Some Filter" },
            { 1, MidiFlow::MidiFlowIn, L"MIDI", L"", L"Some Filter" },
        };

        VERIFY_ARE_EQUAL(std::wstring{ L"Generic Driver Name" },
            RecoverModelNameFromPortNames(L"Generic Driver Name", false, same));
    }

    // A second unit of the same model: the marker is on the name we are handed, and it has to
    // survive, or both units end up called the same thing.
    {
        std::vector<Midi1PortNameInput> ports{ };

        for (uint8_t i = 0; i < 8; i++)
        {
            std::wstring filter{ L"Express  128: Port " + std::to_wstring(i + 1) };

            ports.push_back({ i, MidiFlow::MidiFlowIn,  L"MIDI", L"", filter });
            ports.push_back({ i, MidiFlow::MidiFlowOut, L"MIDI", L"", filter });
        }

        VERIFY_ARE_EQUAL(std::wstring{ L"Express  128 (2)" },
            RecoverModelNameFromPortNames(L"MOTU USB MIDI Device for 64 bit Windows (2)", false, ports));
    }
}


void NamingTests::TestRederiveAgainstInProtocolEndpointName()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // Two identical units on the UMP driver, neither of which names its blocks. The second is
    // handed a name carrying a duplicate marker, and that marker must not be mistaken for a
    // device-supplied port name: it would be applied a second time during composition, and it
    // would make the two units resolve to different naming styles.
    auto buildUnnamedBlockUnit = [](std::wstring const& endpointName)
        {
            auto table = std::make_shared<MidiEndpointNameTable>();

            for (uint8_t group = 0; group < 16; group++)
            {
                VERIFY_SUCCEEDED(table->PopulateEntryForMidi1DeviceUsingUmpDriver(
                    group, MidiFlow::MidiFlowIn, L"", endpointName, L"", group));
            }

            VERIFY_SUCCEEDED(table->RebuildNewStyleNames(endpointName, false));

            return table;
        };

    auto firstUnit = buildUnnamedBlockUnit(L"ESI M8U eX");
    auto secondUnit = buildUnnamedBlockUnit(L"ESI M8U eX (2)");

    VERIFY_ARE_EQUAL(std::wstring{ L"ESI M8U eX group 1" },
        std::wstring{ firstUnit->GetSourceEntry(0)->NewStyleName });

    VERIFY_ARE_EQUAL(std::wstring{ L"ESI M8U eX (2) group 1" },
        std::wstring{ secondUnit->GetSourceEntry(0)->NewStyleName });

    // neither unit said anything, so neither may claim a device-supplied name
    VERIFY_ARE_EQUAL(0u, firstUnit->NameSourceFlags() & MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED);
    VERIFY_ARE_EQUAL(0u, secondUnit->NameSourceFlags() & MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED);

    // identical hardware must not end up on two different naming styles
    VERIFY_ARE_EQUAL(firstUnit->NameSourceFlags(), secondUnit->NameSourceFlags());

    // A MIDI 2.0 endpoint is enumerated under its USB product string and may rename itself
    // in-protocol afterwards. The ports have to follow, or the endpoint and its ports disagree.
    auto buildTable = [](std::wstring const& endpointName)
        {
            std::vector<internal::GroupTerminalBlockInternal> blocks{ };

            internal::GroupTerminalBlockInternal block{ };
            block.Number = 1;
            block.Direction = MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL;
            block.FirstGroupIndex = 0;
            block.GroupCount = 1;
            block.Name = L"Synth";
            blocks.push_back(block);

            auto table = std::make_shared<MidiEndpointNameTable>();

            VERIFY_SUCCEEDED(table->PopulateAllEntriesForNativeUmpDevice(endpointName, blocks));
            VERIFY_SUCCEEDED(table->RebuildNewStyleNames(endpointName, false));

            return table;
        };

    auto enumerated = buildTable(L"Iridium (MIDI 2.0)");

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium (MIDI 2.0) Synth" },
        std::wstring{ enumerated->GetSourceEntry(0)->NewStyleName });

    auto renamed = buildTable(L"Iridium");

    VERIFY_ARE_EQUAL(std::wstring{ L"Iridium Synth" },
        std::wstring{ renamed->GetSourceEntry(0)->NewStyleName });

    // the re-derived table has to be seen as different, or the service would never publish it
    VERIFY_IS_FALSE(renamed->IsEqualTo(enumerated.get()));

    // re-deriving against the same name must be a no-op, so an unchanged endpoint never churns
    VERIFY_IS_TRUE(buildTable(L"Iridium (MIDI 2.0)")->IsEqualTo(enumerated.get()));
}


void NamingTests::TestLegacyDuplicateDeviceMarker()
{
    if (SkipUnlessPortNamingReworkEnabled()) { return; }

    // WinMM's own format string was L"%d- %s", applied only from the second unit on. An app
    // matching a stored name will not recognize any other spacing.
    VERIFY_ARE_EQUAL(std::wstring{ L"Some Device" }, ApplyLegacyDuplicateDeviceMarker(L"Some Device", 1));
    VERIFY_ARE_EQUAL(std::wstring{ L"2- Some Device" }, ApplyLegacyDuplicateDeviceMarker(L"Some Device", 2));
    VERIFY_ARE_EQUAL(std::wstring{ L"3- Some Device" }, ApplyLegacyDuplicateDeviceMarker(L"Some Device", 3));
    VERIFY_ARE_EQUAL(std::wstring{ L"10- Some Device" }, ApplyLegacyDuplicateDeviceMarker(L"Some Device", 10));

    // index 0 is not a thing, and an empty name cannot be marked
    VERIFY_ARE_EQUAL(std::wstring{ L"Some Device" }, ApplyLegacyDuplicateDeviceMarker(L"Some Device", 0));
    VERIFY_ARE_EQUAL(std::wstring{ L"" }, ApplyLegacyDuplicateDeviceMarker(L"", 2));

    // The marker has to reach the published legacy name, wrapped by the port decoration, so the
    // second unit's sixth output reads exactly the way WinMM wrote it.
    MidiEndpointNameTable table{ };

    for (uint8_t group = 0; group < 8; group++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingUmpDriver(
            group, MidiFlow::MidiFlowOut, L"", L"2- Some Device", L"", group));
    }

    VERIFY_ARE_EQUAL(std::wstring{ L"2- Some Device" },
        std::wstring{ table.GetDestinationEntry(0)->LegacyWinMMName });

    VERIFY_ARE_EQUAL(std::wstring{ L"MIDIOUT6 (2- Some Device)" },
        std::wstring{ table.GetDestinationEntry(5)->LegacyWinMMName });

    // Whichever form the marker takes, it is ours and not something the device said, so it must
    // never count as a device-supplied port name.
    std::vector<Midi1PortNameInput> prefixed{
        { 0, MidiFlow::MidiFlowIn, L"", L"", L"2- Some Device" },
        { 1, MidiFlow::MidiFlowIn, L"", L"", L"2- Some Device" },
    };

    auto results = BuildMidi1PortNamesForEndpoint(L"Some Device (2)", false, prefixed);

    VERIFY_ARE_EQUAL((size_t)2, results.size());

    for (auto const& result : results)
    {
        VERIFY_ARE_EQUAL(Midi1PortNameSource::None, result.Resolved.Source);
    }
}



