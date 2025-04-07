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


struct MidiPort
{
    uint16_t Index;
    std::wstring Name;
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

        if (result == MMSYSERR_NOERROR)
        {
            MidiPort port{};
            port.Index = i;
            port.Name = inputCaps.szPname;

            m_midiInputs.push_back(port);
        }
    }

    //std::sort(m_midiInputs.begin(), m_midiInputs.end(),
    //    [](MidiPort a, MidiPort b)
    //    {
    //        return internal::ToLowerWStringCopy(a.Name) < internal::ToLowerWStringCopy(b.Name);
    //    });

    // -----------------------------

    auto outputDeviceCount = midiOutGetNumDevs();

    for (uint16_t i = 0; i < outputDeviceCount; i++)
    {
        MIDIOUTCAPSW outputCaps{ 0 };

        auto result = midiOutGetDevCaps(i, &outputCaps, sizeof(outputCaps));

        if (result == MMSYSERR_NOERROR)
        {
            MidiPort port{};
            port.Index = i;
            port.Name = outputCaps.szPname;

            m_midiOutputs.push_back(port);
        }
    }

    //std::sort(m_midiOutputs.begin(), m_midiOutputs.end(),
    //    [](MidiPort a, MidiPort b)
    //    {
    //        return internal::ToLowerWStringCopy(a.Name) < internal::ToLowerWStringCopy(b.Name);
    //    });


}

void DisplayAllWinMMInputs()
{
    WriteInfo(std::to_string(m_midiInputs.size()) + " Available Input Ports (MIDI Sources)");
    std::wcout << std::endl;

    for (auto const& port : m_midiInputs)
    {
        std::cout
            << std::setw(3) << dye::yellow(port.Index)
            << dye::grey(" : ");

        std::wcout
            << port.Name
            << std::endl;
    }

    std::wcout << std::endl;
}

void DisplayAllWinMMOutputs()
{
    WriteInfo(std::to_string(m_midiOutputs.size()) + " Available Output Ports (MIDI Destinations)");
    std::wcout << std::endl;

    for (auto const& port : m_midiOutputs)
    {
        std::cout
            << std::setw(3) << dye::yellow(port.Index)
            << dye::grey(" : ");

        std::wcout
            << port.Name
            << std::endl;
    }

    std::wcout << std::endl;
}

#define LINE_LENGTH 79


#define RETURN_INVALID_PORT_NUMBER 1
#define RETURN_UNABLE_TO_OPEN_PORT 2

int __cdecl main(int argc, char* argv[])
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





