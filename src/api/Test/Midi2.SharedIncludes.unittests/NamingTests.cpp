// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


#include "MidiEndpointNameTable.h"

using namespace WindowsMidiServicesNamingLib;



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
