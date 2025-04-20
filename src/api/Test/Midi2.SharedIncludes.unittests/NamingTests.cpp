// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"



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

