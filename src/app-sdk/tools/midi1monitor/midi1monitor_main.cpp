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


struct MidiInputPort
{
    uint16_t Index;
    std::wstring Name;
};

std::vector<MidiInputPort> m_midiInputs{};


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
            MidiInputPort port{};
            port.Index = i;
            port.Name = inputCaps.szPname;

            m_midiInputs.push_back(port);
        }
    }

    //std::sort(m_midiInputs.begin(), m_midiInputs.end(),
    //    [](MidiInputPort a, MidiInputPort b)
    //    {
    //        return internal::ToLowerWStringCopy(a.Name) < internal::ToLowerWStringCopy(b.Name);
    //    });

}

void DisplayAllWinMMInputs()
{
    WriteInfo(std::to_string(m_midiInputs.size()) + " Available Input Ports");

    for (auto const& port : m_midiInputs)
    {
        std::cout
            << std::setw(3) << dye::yellow(port.Index)
            << dye::grey(" : ");

        std::wcout
            << port.Name
            << std::endl;
    }
}

#define LINE_LENGTH 79

uint32_t m_countAllBytesReceived{ 0 };
uint32_t m_countStatusBytesReceived{ 0 };

void DisplayStatusByte(byte status, bool isError)
{
    if (status == MIDI_EOX)
    {
        std::cout << " ";
    }
    else
    {
        std::cout << std::endl;
    }

    if (isError)
    {
        std::cout << std::hex << std::setw(2) << dye::light_red((uint16_t)status);
        return;
    }


    if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(status))
    {
        switch (status & 0xF0)
        {
        case MIDI_NOTEOFF:
            std::cout << std::hex << std::setw(2) << dye::aqua((uint16_t)status);
            break;
        case MIDI_NOTEON:
            std::cout << std::hex << std::setw(2) << dye::light_aqua((uint16_t)status);
            break;
        case MIDI_MONOAFTERTOUCH:
            std::cout << std::hex << std::setw(2) << dye::yellow((uint16_t)status);
            break;
        case MIDI_CONTROLCHANGE:
            std::cout << std::hex << std::setw(2) << dye::light_blue((uint16_t)status);
            break;
        default:
            std::cout << std::hex << std::setw(2) << dye::light_purple((uint16_t)status);
        }
    }
    else if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(status))
    {


        std::cout << std::hex << std::setw(2) << dye::grey((uint16_t)status);
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


void DisplayDecodedChannelVoiceMessage(std::string messageName, uint8_t channel, std::string labelForByte1, uint8_t byte1)
{
    std::cout << std::left << std::setw(18) << std::setfill(' ') << dye::aqua(messageName) << "  ";
    std::cout << dye::grey("Channel: ") << std::setw(2) << std::right << std::dec << dye::yellow((uint16_t)channel) << ",  ";
    std::cout << std::setw(12) << dye::grey(labelForByte1) << ": " << std::setw(3) << std::right << std::dec << dye::yellow((uint16_t)byte1);

}

void DisplayDecodedChannelVoiceMessage(std::string messageName, uint8_t channel, std::string labelForByte1, uint8_t byte1, std::string labelForByte2, uint8_t byte2)
{
    DisplayDecodedChannelVoiceMessage(messageName, channel, labelForByte1, byte1);

    std::cout << ",  ";
    std::cout << std::setw(12) << dye::grey(labelForByte2) << ": " << std::setw(3) << std::right << std::dec << dye::yellow((uint16_t)byte2);

}

void DisplayMidiMessage(DWORD dwMidiMessage, DWORD dwTimestamp, bool isError)
{
    UNREFERENCED_PARAMETER(dwTimestamp);

    // message format 0 | data 2 | data 1 | status

    byte status = static_cast<byte>(dwMidiMessage & 0x000000FF);
    byte data1 = static_cast<byte>((dwMidiMessage & 0x0000FF00) >> 8);
    byte data2 = static_cast<byte>((dwMidiMessage & 0x00FF0000) >> 16);

    m_countStatusBytesReceived++;
    m_countAllBytesReceived++;

    if (status == MIDI_ACTIVESENSE && !m_showActiveSense)
    {
        return;
    }

    if (status == MIDI_TIMINGCLOCK && !m_showClock)
    {
        return;
    }

    DisplayStatusByte(status, isError);


    if (MIDI_MESSAGE_IS_TWO_BYTES(status) ||
        MIDI_MESSAGE_IS_THREE_BYTES(status))
    {
        DisplayDataByte(data1, isError);
        m_countAllBytesReceived++;
    }

    if (MIDI_MESSAGE_IS_THREE_BYTES(status))
    {
        DisplayDataByte(data2, isError);
        m_countAllBytesReceived++;
    }

    // display a decoding of the message to the right

    if (status != MIDI_SYSEX && status != MIDI_EOX)
    {
        uint16_t spaces{ 0 };

        if (MIDI_MESSAGE_IS_ONE_BYTE(status))
        {
            spaces = 6;
        }
        else if (MIDI_MESSAGE_IS_TWO_BYTES(status))
        {
            spaces = 3;
        }
        else
        {
            spaces = 1;
        }

        std::cout << std::setw(spaces + 2) << std::setfill(' ') << "";

        if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(status))
        {
            uint8_t channel = (status & 0x0F) + 1;

            switch (status & 0xF0)
            {
            case MIDI_NOTEOFF:
                DisplayDecodedChannelVoiceMessage("Note Off", channel, "Note", data1, "Velocity", data2);
                break;
            case MIDI_NOTEON:
                DisplayDecodedChannelVoiceMessage("Note On", channel, "Note", data1, "Velocity", data2);
                break;
            case MIDI_POLYAFTERTOUCH:
                DisplayDecodedChannelVoiceMessage("Poly Aftertouch", channel, "Note", data1, "Pressure", data2);
                break;
            case MIDI_CONTROLCHANGE:
                DisplayDecodedChannelVoiceMessage("Control Change", channel, "Controller", data1, "Value", data2);
                break;
            case MIDI_PROGRAMCHANGE:
                DisplayDecodedChannelVoiceMessage("Program Change", channel, "Program", data1);
                break;
            case MIDI_MONOAFTERTOUCH:
                DisplayDecodedChannelVoiceMessage("Channel Pressure", channel, "Pressure", data1);
                break;
            case MIDI_PITCHBEND:
                DisplayDecodedChannelVoiceMessage("Pitch Bend", channel, "LSB", data1, "MSB", data2);
                break;
            default:
                break;
            }
        }
        else if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(status))
        {
            switch (status)
            {
            case MIDI_TIMINGCLOCK:
                std::cout << dye::light_aqua("System Real-Time: Clock");
                break;
            case MIDI_START:
                std::cout << dye::green("System Real-Time: Start");
                break;
            case MIDI_CONTINUE:
                std::cout << dye::light_yellow("System Real-Time: Continue");
                break;
            case MIDI_STOP:
                std::cout << dye::red("System Real-Time: Stop");
                break;
            case MIDI_ACTIVESENSE:
                std::cout << dye::grey("System Real-Time: Active Sense");
                break;
            case MIDI_RESET:
                std::cout << dye::light_red("System Real-Time: Reset");
                break;
            }
        }
    }

}

void DisplayMidiLongMessage(LPMIDIHDR header, DWORD dwTimestamp, bool error)
{
    UNREFERENCED_PARAMETER(dwTimestamp);

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
    UNREFERENCED_PARAMETER(dwInstance);
    UNREFERENCED_PARAMETER(hMidiIn);

    switch (wMsg)
    {
    case MIM_OPEN:
        break;
    case MIM_CLOSE:
        break;
    case MIM_DATA:
        DisplayMidiMessage(static_cast<DWORD>(dwParam1), static_cast<DWORD>(dwParam2), false);
        break;
    case MIM_LONGDATA:
        DisplayMidiLongMessage((LPMIDIHDR)dwParam1, static_cast<DWORD>(dwParam2), false);
        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        break;
    case MIM_ERROR:
        DisplayMidiMessage(static_cast<DWORD>(dwParam1), static_cast<DWORD>(dwParam2), true);
        break;
    case MIM_LONGERROR:
        DisplayMidiLongMessage((LPMIDIHDR)dwParam1, static_cast<DWORD>(dwParam2), true);
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
    std::cout << dye::aqua(" This tool is part of the Windows MIDI Services SDK and tools") << std::endl;
    std::cout << dye::aqua(" Copyright 2025- Microsoft Corporation.") << std::endl;
    std::cout << dye::aqua(" Information, license, and source available at https://aka.ms/midi") << std::endl;
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
            portNumber = static_cast<uint16_t>(std::stoi(argv[1]));
            portNumberProvided = true;
        }
        catch (...)
        {
            portNumber = 0;
            portNumberProvided = false;
        }
    }

    // todo: need to take a command-line arg to show active sense messages. Defaults to false.

    // todo: need to take a command-line arg to show timing clock messages. Defaults to false.

    if (!portNumberProvided)
    {
        DisplayAllWinMMInputs();

        std::cout << std::endl;
        WriteInfo("Enter port number to monitor:");
        std::cin >> portNumber;
    }


    if (auto const& port = std::find_if(m_midiInputs.begin(), m_midiInputs.end(),
        [&portNumber](const MidiInputPort& p) { return p.Index == portNumber; });
        port != m_midiInputs.end())
    {
        std::cout << std::endl;
        std::cout << dye::aqua("Monitoring ");
        std::wcout << port->Name;
        std::cout << dye::aqua(" for input. Hit ");
        std::cout << dye::green("escape");
        std::cout << dye::aqua(" to cancel. Hit ");
        std::cout << dye::green("spacebar");
        std::cout << dye::aqua(" to toggle showing hidden messages.");
        std::cout << std::endl;

        if (!m_showActiveSense)
        {
            std::cout << dye::aqua("Hiding") << dye::light_red(" active sense ") << dye::aqua("messages. ");
        }

        if (!m_showClock)
        {
            std::cout << dye::aqua("Hiding") << dye::light_red(" clock ") << dye::aqua("messages.");
        }

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
        auto ch = _getch();

        if (ch == KEY_ESCAPE)
        {
            WriteInfo("\nClosing");
            break;
        }
        else if (ch == KEY_SPACE)
        {
            // toggle showing hidden messages

            WriteInfo("\nToggling showing active sense and clock messages");

            m_showActiveSense = !m_showActiveSense;
            m_showClock = !m_showClock;
        }

    }

    midiInUnprepareHeader(m_hMidiIn, &m_header, sizeof(MIDIHDR));

    midiInStop(m_hMidiIn);
    midiInClose(m_hMidiIn);

    return 0;
}


