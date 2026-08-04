// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include <io.h>
#include <fcntl.h>

#include <fmt/base.h>
#include <fmt/xchar.h>
#include <fmt/format.h>
#include <fmt/color.h>



bool m_showActiveSense{ false };
bool m_showClock{ false };


#define DRV_RESERVED                    0x0800
#define DRV_QUERYDEVICEINTERFACE        (DRV_RESERVED + 12)
#define DRV_QUERYDEVICEINTERFACESIZE    (DRV_RESERVED + 13)

#define LINE_LENGTH 79

const auto infoTextStyle = fmt::fg(fmt::color::steel_blue);
const auto errorTextStyle = fmt::fg(fmt::color::pink);
const auto normalTextStyle = fmt::fg(fmt::color::light_gray);
const auto separatorTextStyle = fmt::fg(fmt::color::gray);

void WriteInfo(_In_ std::wstring info)
{
    fmt::println(L"{}", fmt::styled(info, infoTextStyle));
}


void WriteError(_In_ std::wstring error)
{
    fmt::println(L"{}", fmt::styled(error, errorTextStyle));
}

void WriteDoubleSeparator()
{
    fmt::println(L"{}", fmt::styled(std::wstring(LINE_LENGTH, L'='), separatorTextStyle));
}

void WriteSingleSeparator()
{
    fmt::println(L"{}", fmt::styled(std::wstring(LINE_LENGTH, L'-'), separatorTextStyle));
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
                    m_midiOutputCountNoErrors++;
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


void DisplayPort(_In_ MidiPort const& port)
{
    if (port.IsError)
    {
        fmt::print(L"{:<3}", fmt::styled(port.Index, errorTextStyle));
    }
    else
    {
        fmt::print(L"{:<3}", fmt::styled(port.Index, fmt::fg(fmt::color::golden_rod)));
    }

    fmt::println(L"{:<33} - {}: {}", 
        fmt::styled(port.Name, normalTextStyle), 
        fmt::styled(L"Dev Interface", fmt::fg(fmt::color::gray)), 
        fmt::styled(port.DriverInterface, normalTextStyle));

    fmt::println(L"");
}

void DisplayAllWinMMInputs()
{
    auto deviceCount = midiInGetNumDevs();
    WriteInfo(std::format(L" {} ports reported by midiInGetNumDevs", deviceCount));
    WriteInfo(std::format(L" {} valid Input Ports (MIDI Sources) found.", m_midiInputCountNoErrors));
    fmt::println(L"");

    for (auto const& port : m_midiInputs)
    {
        DisplayPort(port);
    }

    std::wcout << std::endl;
}

void DisplayAllWinMMOutputs()
{
    auto deviceCount = midiOutGetNumDevs();

    WriteInfo(std::format(L" {} ports reported by midiOutGetNumDevs", deviceCount));
    WriteInfo(std::format(L" {} valid Output Ports (MIDI Destinations) found.", m_midiOutputCountNoErrors));
    fmt::println(L"");

    for (auto const& port : m_midiOutputs)
    {
        DisplayPort(port);
    }

    std::wcout << std::endl;
}




#define RETURN_INVALID_PORT_NUMBER 1
#define RETURN_UNABLE_TO_OPEN_PORT 2

int __cdecl main(_In_ int argc, _In_ char* argv[])
{
    auto setModeResult = _setmode(_fileno(stdout), _O_U16TEXT);  // _O_WTEXT
    
    if (setModeResult == -1)   
    {
        perror("Unable to set stdout to UTF-16 mode. ");
        return 1;
    }

    WriteDoubleSeparator();
    WriteInfo(L" This tool is part of the Windows MIDI Services API and tools");
    WriteInfo(L" Copyright 2026- Microsoft Corporation.");
    WriteInfo(L" Information, license, and source available at https://aka.ms/midi");
    WriteDoubleSeparator();
    WriteInfo(L" List of WinMM/MME ports");
    WriteDoubleSeparator();

    bool loop{ false };

    if (argc >= 2)
    {
        std::string loopParam{ "--loop" };
        std::string loopParamShort{ "-l" };

        std::string providedParam(argv[1]);
       
        if (CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, loopParam.c_str(), static_cast<int>(loopParam.size() + 1), providedParam.c_str(), static_cast<int>(providedParam.size() + 1))
            == CSTR_EQUAL)
        {
            loop = true;
        }

        if (CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, loopParamShort.c_str(), static_cast<int>(loopParamShort.size() + 1), providedParam.c_str(), static_cast<int>(providedParam.size() + 1))
            == CSTR_EQUAL)
        {
            loop = true;
        }

    }

    while (true)
    {
        LoadWinMMDevices();

        DisplayAllWinMMInputs();

        WriteDoubleSeparator();


        DisplayAllWinMMOutputs();

        if (loop)
        {
            WriteInfo(L"Press space to enumerate again, or escape to close.");

            auto ch = _getch();

            if (ch == KEY_ESCAPE)
            {
                WriteInfo(L"\nClosing");
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





