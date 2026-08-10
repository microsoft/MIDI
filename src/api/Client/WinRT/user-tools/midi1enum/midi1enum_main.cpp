// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "console_tools_shared.h"

bool m_showActiveSense{ false };
bool m_showClock{ false };


#define DRV_RESERVED                    0x0800
#define DRV_QUERYDEVICEINTERFACE        (DRV_RESERVED + 12)
#define DRV_QUERYDEVICEINTERFACESIZE    (DRV_RESERVED + 13)


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
            port.Name = internal::ResourceGetWString(IDS_PORT_NAME_ERROR);
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
            port.Name = internal::ResourceGetWString(IDS_PORT_NAME_ERROR);
            port.IsError = true;
        }

        m_midiOutputs.push_back(port);
    }

}


void DisplayPort(_In_ MidiPort const& port)
{
    if (port.IsError)
    {
        fmt::print(L"{:<3}", Styled(port.Index, errorTextStyle));
    }
    else
    {
        fmt::print(L"{:<3}", Styled(port.Index, fmt::fg(fmt::color::golden_rod)));
    }

    fmt::println(L"{:<33} - {}: {}", 
        Styled(port.Name, normalTextStyle), 
        Styled(internal::ResourceGetWString(IDS_LABEL_DEVICE_INTERFACE), fmt::fg(fmt::color::gray)), 
        Styled(port.DriverInterface, normalTextStyle));

    WriteBlankLine();
}

void DisplayAllWinMMInputs()
{
    auto deviceCount = midiInGetNumDevs();
    WriteInfoLine(std::format(L" {} {}", deviceCount, internal::ResourceGetWString(IDS_ENUM_PORTS_REPORTED_BY_MIDIINGETNUMDEVS)));
    WriteInfoLine(std::format(L" {} {}", m_midiInputCountNoErrors, internal::ResourceGetWString(IDS_ENUM_VALID_INPUT_PORTS_FOUND)));
    fmt::println(L"");

    for (auto const& port : m_midiInputs)
    {
        DisplayPort(port);
    }

    WriteBlankLine();
}

void DisplayAllWinMMOutputs()
{
    auto deviceCount = midiOutGetNumDevs();

    WriteInfoLine(std::format(L" {} {}", deviceCount, internal::ResourceGetWString(IDS_ENUM_PORTS_REPORTED_BY_MIDIOUTGETNUMDEVS)));
    WriteInfoLine(std::format(L" {} {}", m_midiOutputCountNoErrors, internal::ResourceGetWString(IDS_ENUM_VALID_OUTPUT_PORTS_FOUND)));
    fmt::println(L"");

    for (auto const& port : m_midiOutputs)
    {
        DisplayPort(port);
    }

    WriteBlankLine();
}






int __cdecl main(_In_ int argc, _In_ wchar_t* argv[])
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_COPYRIGHT));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_INFO_URL));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteDoubleSeparatorLine();

    bool loop{ false };

    if (argc >= 2)
    {
        std::wstring loopParam{ L"--loop" };
        std::wstring loopParamShort{ L"-l" };

        std::wstring providedParam(argv[1]);
       
        if (CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, loopParam.c_str(), static_cast<int>(loopParam.size() + 1), providedParam.c_str(), static_cast<int>(providedParam.size() + 1))
            == CSTR_EQUAL)
        {
            loop = true;
        }

        if (CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, loopParamShort.c_str(), static_cast<int>(loopParamShort.size() + 1), providedParam.c_str(), static_cast<int>(providedParam.size() + 1))
            == CSTR_EQUAL)
        {
            loop = true;
        }

    }

    while (true)
    {
        LoadWinMMDevices();

        DisplayAllWinMMInputs();

        WriteDoubleSeparatorLine();


        DisplayAllWinMMOutputs();

        if (loop)
        {
            WriteInfoLine(internal::ResourceGetWString(IDS_PROMPT_ENUMERATE_AGAIN));

            auto ch = _getch();

            if (ch == KEY_ESCAPE)
            {
                WriteInfoLine(L"\n" + internal::ResourceGetWString(IDS_STATUS_CLOSING));
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


    return RETURN_SUCCESS;
}





