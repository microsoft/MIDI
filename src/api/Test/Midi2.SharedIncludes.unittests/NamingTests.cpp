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

    // for MIDI 2 devices, the classic names are also new-style names
    std::vector<std::wstring> expectedSourceClassicNames{};
    std::copy(expectedSourceNewStyleNames.begin(), expectedSourceNewStyleNames.end(), std::back_inserter(expectedSourceClassicNames));


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

    // for MIDI 2 devices, the classic names are also new-style names
    std::vector<std::wstring> expectedDestinationClassicNames{};
    std::copy(expectedDestinationNewStyleNames.begin(), expectedDestinationNewStyleNames.end(), std::back_inserter(expectedDestinationClassicNames));


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
    std::copy(expectedSourceNewStyleNames.begin(), expectedSourceNewStyleNames.end(), std::back_inserter(expectedSourceClassicNames));


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
    std::copy(expectedDestinationNewStyleNames.begin(), expectedDestinationNewStyleNames.end(), std::back_inserter(expectedDestinationClassicNames));

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
            sourceBlockNames[groupIndex]
        ));

        portIndexSource++;
    }

    for (uint8_t groupIndex = 0; groupIndex < destinationBlockNames.size(); groupIndex++)
    {
        VERIFY_SUCCEEDED(table.PopulateEntryForMidi1DeviceUsingUmpDriver(
            groupIndex,
            MidiFlow::MidiFlowOut,
            L"",                   // custom name
            destinationBlockNames[groupIndex]
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







#if false

// checks the StringEndsWithNumber function
void NamingTests::TestCheckForEndingNumber()
{
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 2", 2));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 12", 12));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 123", 123));

    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test2", 2));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test12", 12));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test123", 123));

    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test-2", 2));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test-12", 12));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test-123", 123));

    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"867-123", 123));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"867 123", 123));

    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 1-23", 23));
    VERIFY_IS_TRUE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 1 23", 23));


    VERIFY_IS_FALSE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 2", 20));
    VERIFY_IS_FALSE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 12", 2));
    VERIFY_IS_FALSE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test-12", 2));
    VERIFY_IS_FALSE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 123", 3));
    VERIFY_IS_FALSE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 123", 23));

    VERIFY_IS_FALSE(internal::Midi1PortNaming::StringEndsWithNumber(L"Test 1-23", 123));
}

void NamingTests::TestFullyCleanupBlockName()
{
    std::wstring parentDeviceName{ L"MOTU USB MIDI Device" };
    std::wstring filterName{ L"MOTU USB MIDI" };

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupBlockName(parentDeviceName, parentDeviceName, filterName), L"");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupBlockName(filterName, parentDeviceName, filterName), L"");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupBlockName(L"MIDI", parentDeviceName, filterName), L"");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupBlockName(filterName + L"Input 1", parentDeviceName, filterName), L"Input 1");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupBlockName(parentDeviceName + L"Input 1", parentDeviceName, filterName), L"Input 1");

}

void NamingTests::TestFullyCleanupKSPinName()
{
    std::wstring parentDeviceName{ L"MOTU USB MIDI Device" };
    std::wstring filterName{ L"Midi Port 1" };

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L"Port [0]", parentDeviceName, filterName), L"Port");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L"MIDI", parentDeviceName, filterName), L"");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L"IN", parentDeviceName, filterName), L"");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L"OUT", parentDeviceName, filterName), L"");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L"MIDIIN ", parentDeviceName, filterName), L"MIDIIN");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L" MIDIOUT", parentDeviceName, filterName), L"MIDIOUT");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L" MIDI IN", parentDeviceName, filterName), L"MIDI IN");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(L"MIDI OUT ", parentDeviceName, filterName), L"MIDI OUT");

    // pin name is same as parent device name
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(parentDeviceName, parentDeviceName, filterName), L"");

    // pin name is same as filter name
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::FullyCleanupKSPinName(filterName, parentDeviceName, filterName), L"");
}

void NamingTests::TestRemoveJustKSPinGeneratedSuffix()
{
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::RemoveJustKSPinGeneratedSuffix(L"Port [0]"), L"Port");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::RemoveJustKSPinGeneratedSuffix(L"Port [10]"), L"Port");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::RemoveJustKSPinGeneratedSuffix(L"Port [20] "), L"Port");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::RemoveJustKSPinGeneratedSuffix(L"Port  [20] "), L"Port");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::RemoveJustKSPinGeneratedSuffix(L"Port [16] Input"), L"Port [16] Input");
}

void NamingTests::TestAddGroupNumberToNameIfNeeded()
{
    std::wstring parentDeviceName{ L"MOTU USB MIDI Device 5" };
    std::wstring filterName{ L"MIDI Port 2" };

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, L"MIDI Port 2", 0), L"MIDI Port 2");   // we don't add for group index 0
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, L"MIDI Port 2", 3), L"MIDI Port 2 4");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, L"MOTU USB MIDI Device 5", 3), L"MOTU USB MIDI Device 5 4");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, L"MOTU USB MIDI Device 5", 4), L"MOTU USB MIDI Device 5");
}


// Scenario: We have a registry entry for the device name
// and we want to generate the port names based upon that
// information.
void NamingTests::TestGenerateWinMMLegacyName()
{
    std::wstring filterName{ L"YAMAHA DTX-MULTI 12" };
    std::wstring registryName{ L"YAMAHA DTX-MULTI 12" };


    for (auto const flow : {MidiFlow::MidiFlowIn, MidiFlow::MidiFlowOut})
    {
        std::wstring prefix{};
        if (flow == MidiFlow::MidiFlowIn)
        {
            prefix = L"MIDIIN";
        }
        else
        {
            prefix = L"MIDIOUT";
        }

        VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateLegacyMidi1PortName(registryName, filterName, flow, 0), registryName);
        VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateLegacyMidi1PortName(registryName, filterName, flow, 1), prefix + L"2 (" + registryName + L")");
        VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateLegacyMidi1PortName(registryName, filterName, flow, 2), prefix + L"3 (" + registryName + L")");

        VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateLegacyMidi1PortName(L"", filterName, flow, 0), filterName);
        VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateLegacyMidi1PortName(L"", filterName, flow, 1), prefix + L"2 (" + filterName + L")");
        VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateLegacyMidi1PortName(L"", filterName, flow, 2), prefix + L"3 (" + filterName + L")");
    }
}

// Scenario: We have a device using the MIDI 2 driver and it has no
// iJack names, so each GTB is named after the device itself
void NamingTests::TestGeneratePinNameBasedMidi1PortName()
{
    std::wstring filterName{ L"YAMAHA DTX-MULTI 12" };

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GeneratePinNameBasedMidi1PortName(filterName, L"MIDI Out [1]", MidiFlow::MidiFlowOut, 0), L"MIDI Out");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GeneratePinNameBasedMidi1PortName(filterName, L"[1]", MidiFlow::MidiFlowOut, 0), L"");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GeneratePinNameBasedMidi1PortName(filterName, L"MIDI Out", MidiFlow::MidiFlowOut, 0), L"MIDI Out");

    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GeneratePinNameBasedMidi1PortName(filterName, L"YAMAHA DTX-MULTI 12 Out 1", MidiFlow::MidiFlowOut, 0), L"YAMAHA DTX-MULTI 12 Out 1");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GeneratePinNameBasedMidi1PortName(filterName, L"YAMAHA DTX-MULTI 12 Out [1]", MidiFlow::MidiFlowOut, 0), L"YAMAHA DTX-MULTI 12 Out");

}


// Scenario: 
void NamingTests::TestGenerateFilterPlusBlockMidi1PortName()
{
    std::wstring parentDeviceName{ L"YAMAHA USB-MIDI Driver (WDM)" };
    std::wstring filterName{ L"YAMAHA USB-MIDI-1" };


    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateFilterPlusBlockMidi1PortName(parentDeviceName, filterName, L"MIDI Out [1]", 0), L"YAMAHA USB-MIDI-1 MIDI Out");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateFilterPlusBlockMidi1PortName(parentDeviceName, filterName, L"MIDI", 0), L"YAMAHA USB-MIDI-1");
    VERIFY_ARE_EQUAL(internal::Midi1PortNaming::GenerateFilterPlusBlockMidi1PortName(parentDeviceName, filterName, L"MIDI", 2), L"YAMAHA USB-MIDI-1 3");
}

#endif