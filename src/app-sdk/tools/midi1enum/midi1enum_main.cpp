// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#include "color.hpp"
#pragma warning(pop)

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


struct MidiPort
{
    uint16_t Index;
    std::wstring Name;
    bool IsError{ false };
};

std::vector<MidiPort> m_midiInputs{};
std::vector<MidiPort> m_midiOutputs{};


void LoadWinMMDevices()
{
    //std::map<uint16_t, MIDIINCAPSW> midiInputDevices;

    auto inputDeviceCount = midiInGetNumDevs();

    for (uint16_t i = 0; i < inputDeviceCount; i++)
    {
        MIDIINCAPSW inputCaps{ 0 };

        auto result = midiInGetDevCaps(i, &inputCaps, sizeof(inputCaps));

        MidiPort port{};
        port.Index = i;

        if (result == MMSYSERR_NOERROR)
        {
            port.Name = inputCaps.szPname;
            port.IsError = false;
        }
        else
        {
            port.Name = L"** Error **";
            port.IsError = true;
        }

        m_midiInputs.push_back(port);

    }

    // -----------------------------

    auto outputDeviceCount = midiOutGetNumDevs();

    for (uint16_t i = 0; i < outputDeviceCount; i++)
    {
        MIDIOUTCAPSW outputCaps{ 0 };

        auto result = midiOutGetDevCaps(i, &outputCaps, sizeof(outputCaps));

        MidiPort port{};
        port.Index = i;

        if (result == MMSYSERR_NOERROR)
        {
            port.Name = outputCaps.szPname;
            port.IsError = false;
        }
        else
        {
            port.Name = L"** Error **";
            port.IsError = true;
        }

        m_midiOutputs.push_back(port);
    }

}

void DisplayAllWinMMInputs()
{
    auto deviceCount = midiInGetNumDevs();
    WriteInfo(" " + std::to_string(deviceCount) + " ports reported by midiInGetNumDevs");
    WriteInfo(" " + std::to_string(m_midiInputs.size()) + " available Input Ports (MIDI Sources) found.");
    std::wcout << std::endl;

    for (auto const& port : m_midiInputs)
    {
        if (port.IsError)
        {
            std::cout
                << std::setw(3) << dye::red(port.Index)
                << dye::grey(" : ");
        }
        else
        {
            std::cout
                << std::setw(3) << dye::yellow(port.Index)
                << dye::grey(" : ");
        }

        std::wcout
            << port.Name
            << std::endl;
    }

    std::wcout << std::endl;
}

void DisplayAllWinMMOutputs()
{
    auto deviceCount = midiOutGetNumDevs();

    WriteInfo(" " + std::to_string(deviceCount) + " ports reported by midiOutGetNumDevs");
    WriteInfo(" " + std::to_string(m_midiOutputs.size()) + " available Output Ports (MIDI Destinations) found.");
    std::wcout << std::endl;

    for (auto const& port : m_midiOutputs)
    {
        if (port.IsError)
        {
            std::cout
                << std::setw(3) << dye::red(port.Index)
                << dye::grey(" : ");
        }
        else
        {
            std::cout
                << std::setw(3) << dye::yellow(port.Index)
                << dye::grey(" : ");
        }

        std::wcout
            << port.Name
            << std::endl;
    }

    std::wcout << std::endl;
}

#define LINE_LENGTH 79


#define RETURN_INVALID_PORT_NUMBER 1
#define RETURN_UNABLE_TO_OPEN_PORT 2

int __cdecl main(int /*argc*/, char* /*argv[]*/)
{
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" This tool is part of the Windows MIDI Services SDK and tools") << std::endl;
    std::cout << dye::aqua(" Copyright 2025- Microsoft Corporation.") << std::endl;
    std::cout << dye::aqua(" Information, license, and source available at https ://aka.ms/midi") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" List of WinMM/MME ports") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    LoadWinMMDevices();

    DisplayAllWinMMInputs();

    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    DisplayAllWinMMOutputs();


    return 0;
}





