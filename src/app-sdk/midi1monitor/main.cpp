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

std::map<uint16_t, MIDIINCAPSW> m_midiInputDevices;

void LoadWinMMDevices()
{
    auto inputDeviceCount = midiInGetNumDevs();

    for (uint16_t i = 0; i < inputDeviceCount; i++)
    {
        MIDIINCAPSW inputCaps{};

        auto result = midiInGetDevCaps(i, &inputCaps, sizeof(inputCaps));

        if (result == MMSYSERR_NOERROR)
        {
            m_midiInputDevices.insert_or_assign(i, inputCaps);
        }
    }
}

void DisplayAllWinMMInputs()
{
    WriteInfo("Available Input Ports");;

    for (auto const& capsEntry : m_midiInputDevices)
    {
        std::cout
            << std::setw(3) << dye::yellow(capsEntry.first)
            << dye::grey(" : ");

        std::wcout
            << capsEntry.second.szPname
            << std::endl;
    }
}

#define LINE_LENGTH 79

uint32_t m_countAllBytesReceived{ 0 };
uint32_t m_countStatusBytesReceived{ 0 };

void DisplayStatusByte(byte status, bool isError)
{
    if (status == MIDI_SYSEX)
    {
        std::cout << std::endl;
        if (isError)
        {
            std::cout << std::hex << std::setw(2) << dye::light_red((uint16_t)status);
        }
        else
        {
            std::cout << std::hex << std::setw(2) << dye::yellow((uint16_t)status);
        }
    }
    else if (status == MIDI_EOX)
    {
        std::cout << " ";

        if (isError)
        {
            std::cout << std::hex << std::setw(2) << dye::light_red((uint16_t)status);
        }
        else
        {
            std::cout << std::hex << std::setw(2) << dye::yellow((uint16_t)status);
        }
    }
    else
    {
        std::cout << std::endl;
        if (isError)
        {
            std::cout << std::hex << std::setw(2) << dye::light_red((uint16_t)status);
        }
        else
        {
            std::cout << std::hex << std::setw(2) << dye::light_purple((uint16_t)status);
        }

    }
}

void DisplayDataByte(byte data, bool isError)
{
    std::cout << " ";

    if (isError)
    {
        std::cout << std::setfill('0') << std::hex << std::setw(2) << dye::red((uint16_t)data);
    }
    else
    {
        std::cout << std::setfill('0') << std::hex << std::setw(2) << dye::grey((uint16_t)data);
    }
}


void DisplayMidiMessage(DWORD dwMidiMessage, DWORD dwTimestamp, bool isError)
{
    // message format 0 | data 2 | data 1 | status

    byte status = dwMidiMessage & 0x000000FF;
    byte data1 = (dwMidiMessage & 0x0000FF00) >> 8;
    byte data2 = (dwMidiMessage & 0x00FF0000) >> 16;

    DisplayStatusByte(status, isError);
    m_countStatusBytesReceived++;
    m_countAllBytesReceived++;

    if (MIDI_MESSAGE_IS_TWO_BYTES(status) || MIDI_MESSAGE_IS_THREE_BYTES(status))
    {
        DisplayDataByte(data1, isError);
        m_countAllBytesReceived++;
    }

    if (MIDI_MESSAGE_IS_THREE_BYTES(status))
    {
        DisplayDataByte(data2, isError);
        m_countAllBytesReceived++;
    }
}

void DisplayMidiLongMessage(LPMIDIHDR header, DWORD dwTimestamp, bool error)
{
    if (header)
    {
        byte* current = (byte*)(header->lpData);

        while (header->dwBytesRecorded-- && current != nullptr)
        {
            if (MIDI_BYTE_IS_STATUS_BYTE(*current))
            {
                DisplayStatusByte(*current, error);
                m_countStatusBytesReceived++;
                m_countAllBytesReceived++;
            }
            else
            {
                DisplayDataByte(*current, error);
                m_countAllBytesReceived++;
            }

            current++;
            m_countAllBytesReceived++;
        }
    }
    else
    {
        WriteError("Header is null");
    }
}

#define MIDI_BUFFER_SIZE 4096

byte m_buffer[MIDI_BUFFER_SIZE]{ 0 };
MIDIHDR m_header{ };
HMIDIIN m_hMidiIn{ };



void CALLBACK OnMidiMessageReceived(
    HMIDIIN hMidiIn,
    UINT wMsg,
    DWORD dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
)
{
    switch (wMsg)
    {
    case MIM_OPEN:
        break;
    case MIM_CLOSE:
        break;
    case MIM_DATA:
        DisplayMidiMessage(dwParam1, dwParam2, false);
        break;
    case MIM_LONGDATA:
        DisplayMidiLongMessage((LPMIDIHDR)dwParam1, dwParam2, false);
        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        break;
    case MIM_ERROR:
        DisplayMidiMessage(dwParam1, dwParam2, true);
        break;
    case MIM_LONGERROR:
        DisplayMidiLongMessage((LPMIDIHDR)dwParam1, dwParam2, true);
        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        break;
    case MIM_MOREDATA:
        break;
    default:
        break;
    }
}

#define RETURN_INVALID_PORT_NUMBER 1
#define RETURN_UNABLE_TO_OPEN_PORT 2

int __cdecl main(int argc, char* argv[])
{
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" Monitor a MIDI 1.0 port through WinMM/MME") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    LoadWinMMDevices();

    // enter number to monitor

    uint16_t portNumber{ 0 };
    bool portNumberProvided{ false };

    if (argc > 1)
    {
        try
        {
            portNumber = std::stoi(argv[1]);
            portNumberProvided = true;
        }
        catch (...)
        {
            portNumber = 0;
            portNumberProvided = false;
        }
    }

    if (!portNumberProvided)
    {
        DisplayAllWinMMInputs();

        std::cout << std::endl;
        WriteInfo("Enter port number to monitor:");
        std::cin >> portNumber;
    }


    if (auto const& port = m_midiInputDevices.find(portNumber); port != m_midiInputDevices.end())
    {
        std::cout << dye::aqua("Monitoring ");
        std::wcout << port->second.szPname;
        std::cout << dye::aqua(" for input. Hit escape to cancel.");
        std::cout << std::endl << std::endl;

    }
    else
    {
        WriteError(std::to_string(portNumber) + " is not a valid port number.");
        return RETURN_INVALID_PORT_NUMBER;
    }

    if (midiInOpen(&m_hMidiIn, portNumber, (DWORD_PTR)&OnMidiMessageReceived, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR)
    {
        m_header.lpData = (LPSTR)(m_buffer);
        m_header.dwBufferLength = MIDI_BUFFER_SIZE;
        m_header.dwFlags = 0;

        midiInPrepareHeader(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));

        midiInStart(m_hMidiIn);
    }
    else
    {
        WriteError("Unable to open port for input.");
        return RETURN_UNABLE_TO_OPEN_PORT;
    }

    while (true)
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            std::cout << std::endl;

            WriteInfo("Closing");
            break;
        }
    }

    midiInUnprepareHeader(m_hMidiIn, &m_header, sizeof(MIDIHDR));

    midiInStop(m_hMidiIn);

    midiInClose(m_hMidiIn);

    return 0;
}

