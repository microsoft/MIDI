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


#define DRV_RESERVED                    0x0800
#define DRV_QUERYDEVICEINTERFACE        (DRV_RESERVED + 12)
#define DRV_QUERYDEVICEINTERFACESIZE    (DRV_RESERVED + 13)


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

    uint16_t ManufacturerId{ 0 };
    uint16_t ProductId{ 0 };
    uint32_t DriverVersion{ 0 };

    bool IsError{ false };

    std::wstring DriverInterface;
};

std::vector<MidiPort> m_midiInputs{};
std::vector<MidiPort> m_midiOutputs{};

uint32_t m_midiInputCountNoErrors{ 0 };
uint32_t m_midiOutputCountNoErrors{ 0 };


void LoadWinMMDevices()
{
    // needed for the looping option
    m_midiInputs.clear();
    m_midiOutputs.clear();

    m_midiInputCountNoErrors = 0;
    m_midiOutputCountNoErrors = 0;

    auto inputDeviceCount = midiInGetNumDevs();

    for (uint16_t i = 0; i < inputDeviceCount; i++)
    {
        MIDIINCAPSW caps{ 0 };

        auto result = midiInGetDevCaps(i, &caps, sizeof(caps));

        MidiPort port{};
        port.Index = i;

        if (result == MMSYSERR_NOERROR)
        {
            port.Name = caps.szPname;
            port.IsError = false;
            port.ManufacturerId = caps.wMid;
            port.ProductId = caps.wPid;
            port.DriverVersion = caps.vDriverVersion;

            ULONG interfaceStringSize{ 0 };
            auto queryResult = midiInMessage((HMIDIIN)i, DRV_QUERYDEVICEINTERFACESIZE, (DWORD_PTR)(&interfaceStringSize), 0);

            if (queryResult == MMSYSERR_NOERROR)
            {
                port.DriverInterface.resize(interfaceStringSize / sizeof(wchar_t));

                queryResult = midiInMessage((HMIDIIN)i, DRV_QUERYDEVICEINTERFACE, (DWORD_PTR)(port.DriverInterface.data()), interfaceStringSize);

                if (queryResult == MMSYSERR_NOERROR)
                {
                    m_midiInputCountNoErrors++;
                }
                else
                {
                    port.IsError = true;
                }
            }
            else
            {
                port.IsError = true;
            }

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
        MIDIOUTCAPSW caps{ 0 };

        auto result = midiOutGetDevCaps(i, &caps, sizeof(caps));

        MidiPort port{};
        port.Index = i;

        if (result == MMSYSERR_NOERROR)
        {
            port.Name = caps.szPname;
            port.IsError = false;
            port.ManufacturerId = caps.wMid;
            port.ProductId = caps.wPid;
            port.DriverVersion = caps.vDriverVersion;

            ULONG interfaceStringSize{ 0 };
            auto queryResult = midiOutMessage((HMIDIOUT)i, DRV_QUERYDEVICEINTERFACESIZE, (DWORD_PTR)(&interfaceStringSize), 0);

            if (queryResult == MMSYSERR_NOERROR)
            {
                port.DriverInterface.resize(interfaceStringSize / sizeof(wchar_t));

                queryResult = midiOutMessage((HMIDIOUT)i, DRV_QUERYDEVICEINTERFACE, (DWORD_PTR)(port.DriverInterface.data()), interfaceStringSize);

                if (queryResult == MMSYSERR_NOERROR)
                {
                    m_midiInputCountNoErrors++;
                }
                else
                {
                    port.IsError = true;
                }
            }
            else
            {
                port.IsError = true;
            }

        }
        else
        {
            port.Name = L"** Error **";
            port.IsError = true;
        }

        m_midiOutputs.push_back(port);
    }

}

void DisplayPort(MidiPort const& port)
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
        << std::setw(33)
        << std::left
        << port.Name;

    //std::cout
    //    << dye::grey(" MID: ")
    //    << std::setw(4)
    //    << dye::aqua(port.ManufacturerId)
    //    << dye::grey(", PID: ")
    //    << std::setw(4)
    //    << dye::aqua(port.ProductId)
    //    << dye::grey(", VER: ")
    //    << std::setw(8)
    //    << dye::aqua(port.DriverVersion);

    std::cout
        << dye::grey(" - Dev Interface: ");
        
    std::wcout
        << port.DriverInterface
        << std::endl;


}

void DisplayAllWinMMInputs()
{
    auto deviceCount = midiInGetNumDevs();
    WriteInfo(" " + std::to_string(deviceCount) + " ports reported by midiInGetNumDevs");
    WriteInfo(" " + std::to_string(m_midiInputCountNoErrors) + " valid Input Ports (MIDI Sources) found.");
    std::wcout << std::endl;

    for (auto const& port : m_midiInputs)
    {
        DisplayPort(port);
    }

    std::wcout << std::endl;
}

void DisplayAllWinMMOutputs()
{
    auto deviceCount = midiOutGetNumDevs();

    WriteInfo(" " + std::to_string(deviceCount) + " ports reported by midiOutGetNumDevs");
    WriteInfo(" " + std::to_string(m_midiOutputCountNoErrors) + " valid Output Ports (MIDI Destinations) found.");
    std::wcout << std::endl;

    for (auto const& port : m_midiOutputs)
    {
        DisplayPort(port);
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
    std::cout << dye::aqua(" Copyright 2026- Microsoft Corporation.") << std::endl;
    std::cout << dye::aqua(" Information, license, and source available at https://aka.ms/midi") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" List of WinMM/MME ports") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    bool loop{ false };

    if (argc >= 2)
    {
        std::string loopParam{ "--loop" };
        std::string loopParamShort{ "-l" };

        std::string providedParam(argv[1]);
       
        if (CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, loopParam.c_str(), loopParam.size() + 1, providedParam.c_str(), providedParam.size() + 1)
            == CSTR_EQUAL)
        {
            loop = true;
        }

        if (CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, loopParamShort.c_str(), loopParamShort.size() + 1, providedParam.c_str(), providedParam.size() + 1)
            == CSTR_EQUAL)
        {
            loop = true;
        }

    }

    while (true)
    {
        LoadWinMMDevices();

        DisplayAllWinMMInputs();

        std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

        DisplayAllWinMMOutputs();

        if (loop)
        {
            std::cout << dye::grey("Press space to enumerate again, or escape to close.") << std::endl;

            auto ch = _getch();

            if (ch == KEY_ESCAPE)
            {
                WriteInfo("\nClosing");
                break;
            }
            else if (ch == KEY_SPACE)
            {
                // continue looping
            }
        }
        else
        {
            // not looping, so bail
            break;
        }
    }


    return 0;
}





