// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "color.hpp"


bool m_showActiveSense{ false };
bool m_showClock{ false };



void WriteInfo(std::string info)
{
    std::cout << dye::aqua(info) << std::endl;
}


void WriteError(std::string error)
{
    std::cout << dye::light_red(error) << std::endl;
}

void WriteBrightLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::white(fullLabel);
}

void WriteLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::grey(fullLabel);
}

#define LINE_LENGTH 80

#define RETURN_SUCCESS                  return 0
#define RETURN_INSUFFICIENT_PERMISSIONS return 1
#define RETURN_USER_ABORTED             return 2

int __cdecl main(int argc, char* argv[])
{
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" This tool is part of the Windows MIDI Services SDK and tools") << std::endl;
    std::cout << dye::aqua(" Copyright 2025- Microsoft Corporation.") << std::endl;
    std::cout << dye::aqua(" Information, license, and source available at https ://aka.ms/midi") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" This resets the midi-midi9 entries under drivers32 in the registry.") << std::endl;
    std::cout << dye::aqua(" Typically, you only need to run this if you've used various apps to remove") << std::endl;
    std::cout << dye::aqua(" Korg or other MIDI 1 drivers from your system, and as a result no longer") << std::endl;
    std::cout << dye::aqua(" see MIDI 1 ports in WinMM-using apps and DAWs. If your MIDI 1.0 devices") << std::endl;
    std::cout << dye::aqua(" are working in your applications, you do not need to run this.") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::light_red(" IMPORTANT") << std::endl;
    std::cout << std::endl;
    std::cout << dye::light_aqua(" This will place wdmaud.drv as midi and wdmaud2.drv as midi1 because that is") << std::endl;
    std::cout << dye::light_aqua(" all that is needed for Windows MIDI Services to see MIDI 1 devices and then") << std::endl;
    std::cout << dye::light_aqua(" make them available to MIDI applications.") << std::endl;
    std::cout << std::endl;
    std::cout << dye::light_aqua(" It then removes any other midi values, as they are not needed for USB MIDI 1.0") << std::endl;
    std::cout << dye::light_aqua(" devices. However, if you are using non-USB devices (BLE, virtual), you may") << std::endl;
    std::cout << dye::light_aqua(" need to uninstall and reinstall those drivers.") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    // check that we're running as admin. Bail if we're not.

    std::wstring response{};

    std::cout << "Enter 'y' to continue or any other key to exit. ";
    std::wcin >> response;

    if (internal::ToLowerTrimmedWStringCopy(response) != L"y")
    {
        // exit

        std::cout << std::endl << "No changes made." << std::endl;
        RETURN_USER_ABORTED;
    }

    // prompt for positive confirmation to continue

    std::wstring drivers32HklmKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32";
    std::wstring controlSetMediaRootHklmKey = L"SYSTEM\\ControlSet001\\Control\Class\\{4d36e96c-e325-11ce-bfc1-08002be10318}";


    // look for wdmaud2.drv in the midi list (including "midi0"). If not found, then check the root to see if there
    // are any entries there which use wdmaud2.drv. If nothing there either, tell the user that this PC does not
    // appear to have Windows MIDI Services installed, and may need an OS reset.


    // prompt the user to close any apps using MIDI


    // restart service




    RETURN_SUCCESS;
}





