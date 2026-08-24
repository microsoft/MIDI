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

struct ReceivedMidiMessage
{
    uint64_t WindowsMidiServicesTimestamp;
    //DWORD WinMMTimestamp;

    bool IsLongMessage{ false };
    bool IsError{ false };

    std::vector<byte> Data { 0,0,0 };   // the normal case for anything but sysex
};

std::queue<ReceivedMidiMessage> m_messages{ };
wil::srwlock m_messagesLock;

struct MidiInputPort
{
    uint16_t Index;
    std::wstring Name;
};

std::vector<MidiInputPort> m_midiInputs{};

uint32_t m_countAllBytesReceived{ 0 };          
uint32_t m_countStatusBytesReceived{ 0 };

uint64_t m_timestampFirstMessageReceived{ 0 };
uint64_t m_timestampLastMessageReceived{ 0 };

#define MIDI_BUFFER_SIZE 4096

byte m_buffer[MIDI_BUFFER_SIZE]{ 0 };
MIDIHDR m_header{ };
HMIDIIN m_hMidiIn{ };


void WriteInputPortSelector(_In_ MidiInputPort const& port)
{
    fmt::println(L"{} : {}",
        Styled(port.Index, fmt::fg(fmt::color::golden_rod)),
        Styled(port.Name, fmt::fg(fmt::color::light_gray))
        );

}

void WriteSysExDataByte(_In_ uint8_t const dataByte)
{
    fmt::print(L"{}", Styled(fmt::format(L"{:02X} ", dataByte), fmt::fg(fmt::color::gray)));
}


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
}

void DisplayAllWinMMInputs()
{
    WriteInfoLine(std::format(L"{} Available Input Ports", m_midiInputs.size()));

    for (auto const& port : m_midiInputs)
    {
        WriteInputPortSelector(port);
    }
}


void DisplayStatusByte(byte status, bool isError)
{
    if (status == MIDI_SYSEX)
    {
        fmt::print(L"\n{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::light_green)));

    }
    else if (status == MIDI_EOX)
    {
        fmt::println(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::light_green)));
    }
    else
    {
        fmt::println(L"");
    }

    if (isError)
    {
        fmt::println(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::pink)));
        return;
    }


    if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(status))
    {
        switch (status & 0xF0)
        {
        case MIDI_NOTEOFF:
            fmt::print(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::dark_cyan)));
            break;
        case MIDI_NOTEON:
            fmt::print(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::cyan)));
            break;
        case MIDI_MONOAFTERTOUCH:
            fmt::print(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::golden_rod)));
            break;
        case MIDI_CONTROLCHANGE:
            fmt::print(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::light_blue)));
            break;
        default:
            fmt::print(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::magenta)));
        }
    }
    else if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(status))
    {
        fmt::println(L"{}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::gray)));
    }

}

void DisplayDataByte(byte data, bool isError)
{
    if (isError)
    {
        fmt::print(L" {}", Styled(fmt::format(L"{:02X}", data), fmt::fg(fmt::color::pink)));
    }
    else
    {
        fmt::print(L" {}", Styled(fmt::format(L"{:02X}", data), fmt::fg(fmt::color::light_gray)));
    }
}


void DisplayDecodedChannelVoiceMessage(
    _In_ std::wstring const& messageName, 
    _In_ uint8_t const channel, 
    _In_ std::wstring const& labelForByte1, 
    _In_ uint8_t const byte1)
{
    fmt::print(L" {} {} {} {} {}", 
        Styled(fmt::format(L"{:<18}", messageName), fmt::fg(fmt::color::aqua)),
        Styled(internal::ResourceGetWString(IDS_LABEL_CHANNEL), fmt::fg(fmt::color::light_gray)),
        Styled(fmt::format(L"{:>2},", channel), fmt::fg(fmt::color::golden_rod)),
        Styled(fmt::format(L"{:<12}:", labelForByte1), fmt::fg(fmt::color::light_gray)),
        Styled(fmt::format(L"{:3}", byte1), fmt::fg(fmt::color::golden_rod))
        );
}

void DisplayDecodedChannelVoiceMessage(
    _In_ std::wstring const& messageName, 
    _In_ uint8_t const channel, 
    _In_ std::wstring const& labelForByte1, 
    _In_ uint8_t const byte1, 
    _In_ std::wstring const& labelForByte2, 
    _In_ uint8_t const byte2)
{
    DisplayDecodedChannelVoiceMessage(messageName, channel, labelForByte1, byte1);

    fmt::print(L", {} {}",
        Styled(fmt::format(L"{:<12}:", labelForByte2), fmt::fg(fmt::color::light_gray)),
        Styled(fmt::format(L"{:3}", byte2), fmt::fg(fmt::color::golden_rod))
    );
}



//concurrency::task<void> 
void DisplayMidiMessage(ReceivedMidiMessage& msg)
{
    m_countStatusBytesReceived++;
    m_countAllBytesReceived++;

    auto status = msg.Data[0];

    if (status == MIDI_ACTIVESENSE && !m_showActiveSense)
    {
        return;
    }

    if (status == MIDI_TIMINGCLOCK && !m_showClock)
    {
        return;
    }

    DisplayStatusByte(status, msg.IsError);


    if (MIDI_MESSAGE_IS_TWO_BYTES(status) ||
        MIDI_MESSAGE_IS_THREE_BYTES(status))
    {
        DisplayDataByte(msg.Data[1], msg.IsError);
        m_countAllBytesReceived++;
    }

    if (MIDI_MESSAGE_IS_THREE_BYTES(status))
    {
        DisplayDataByte(msg.Data[2], msg.IsError);
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

     //   std::cout << std::setw(spaces + 2) << std::setfill(' ') << "";

        if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(status))
        {
            uint8_t channel = (status & 0x0F) + 1;

            switch (status & 0xF0)
            {
            case MIDI_NOTEOFF:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_NOTE_OFF), channel, internal::ResourceGetWString(IDS_LABEL_NOTE), msg.Data[1], internal::ResourceGetWString(IDS_LABEL_VELOCITY), msg.Data[2]);
                break;
            case MIDI_NOTEON:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_NOTE_ON), channel, internal::ResourceGetWString(IDS_LABEL_NOTE), msg.Data[1], internal::ResourceGetWString(IDS_LABEL_VELOCITY), msg.Data[2]);
                break;
            case MIDI_POLYAFTERTOUCH:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_POLY_AFTERTOUCH), channel, internal::ResourceGetWString(IDS_LABEL_NOTE), msg.Data[1], internal::ResourceGetWString(IDS_LABEL_PRESSURE), msg.Data[2]);
                break;
            case MIDI_CONTROLCHANGE:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_CONTROL_CHANGE), channel, internal::ResourceGetWString(IDS_LABEL_CONTROLLER), msg.Data[1], internal::ResourceGetWString(IDS_LABEL_VALUE), msg.Data[2]);
                break;
            case MIDI_PROGRAMCHANGE:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_PROGRAM_CHANGE), channel, internal::ResourceGetWString(IDS_LABEL_PROGRAM), msg.Data[1]);
                break;
            case MIDI_MONOAFTERTOUCH:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_CHANNEL_PRESSURE), channel, internal::ResourceGetWString(IDS_LABEL_PRESSURE), msg.Data[1]);
                break;
            case MIDI_PITCHBEND:
                DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_PITCH_BEND), channel, internal::ResourceGetWString(IDS_LABEL_LSB), msg.Data[1], internal::ResourceGetWString(IDS_LABEL_MSB), msg.Data[2]);
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
                fmt::print(L"{}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_CLOCK), fmt::fg(fmt::color::light_cyan)));
                break;
            case MIDI_START:
                fmt::print(L"{}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_START), fmt::fg(fmt::color::green)));
                break;
            case MIDI_CONTINUE:
                fmt::print(L"{}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_CONTINUE), fmt::fg(fmt::color::light_yellow)));
                break;
            case MIDI_STOP:
                fmt::print(L"{}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_STOP), fmt::fg(fmt::color::red)));
                break;
            case MIDI_ACTIVESENSE:
                fmt::print(L"{}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_ACTIVE_SENSE), fmt::fg(fmt::color::gray)));
                break;
            case MIDI_RESET:
                fmt::print(L"{}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_RESET), fmt::fg(fmt::color::light_salmon)));
                break;
            }
        }
    }
}

void DisplayMidiLongMessage(ReceivedMidiMessage& msg)
{
    for (auto const& b : msg.Data)
    {
        if (MIDI_BYTE_IS_STATUS_BYTE(b))
        {
            DisplayStatusByte(b, msg.IsError);
            m_countStatusBytesReceived++;
            m_countAllBytesReceived++;
        }
        else
        {
            DisplayDataByte(b, msg.IsError);
            m_countAllBytesReceived++;
        }

        m_countAllBytesReceived++;
    }
}


void MessageDisplayWorker(std::stop_token token)
{
    while (!token.stop_requested())
    {       
        if (m_messages.size() > 0)
        {
            auto lock = m_messagesLock.lock_exclusive();
            auto msg = m_messages.front();
            m_messages.pop();
            lock.reset();

            if (msg.IsLongMessage)
            {
                DisplayMidiLongMessage(msg);
            }
            else
            {
                DisplayMidiMessage(msg);
            }

        }
        else
        {
            Sleep(0);
        }
    }
}


void EnqueueMidiDataMessage(
    uint64_t windowsMidiServicesTimestamp,
    DWORD dwParam1,
    DWORD dwParam2,
    bool isError)
{
    UNREFERENCED_PARAMETER(dwParam2);

    byte status = static_cast<byte>(dwParam1 & 0x000000FF);
    byte data1 = static_cast<byte>((dwParam1 & 0x0000FF00) >> 8);
    byte data2 = static_cast<byte>((dwParam1 & 0x00FF0000) >> 16);

    ReceivedMidiMessage msg{};
    msg.WindowsMidiServicesTimestamp = windowsMidiServicesTimestamp;
    msg.Data[0] = status;
    msg.Data[1] = data1;
    msg.Data[2] = data2;
    msg.IsError = isError;
    msg.IsLongMessage = false;

    auto lock = m_messagesLock.lock_exclusive();
    m_messages.push(msg);
}

void EnqueueMidiLongDataMessage(
    uint64_t windowsMidiServicesTimestamp,
    LPMIDIHDR header,
    DWORD dwParam2,
    bool isError)
{
    UNREFERENCED_PARAMETER(dwParam2);

    ReceivedMidiMessage msg{};

    auto count = header->dwBytesRecorded;
    auto start = (byte*)(header->lpData);

    std::vector<byte> data(start, start+count);
    msg.WindowsMidiServicesTimestamp = windowsMidiServicesTimestamp;
    msg.Data = std::move(data);
    msg.IsError = isError;
    msg.IsLongMessage = true;

    auto lock = m_messagesLock.lock_exclusive();
    m_messages.push(msg);
}


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
    case MIM_DATA:
        m_timestampLastMessageReceived = WindowsMidiServicesInternal::GetCurrentMidiTimestamp();
        EnqueueMidiDataMessage(
            m_timestampLastMessageReceived, 
            static_cast<DWORD>(dwParam1), 
            static_cast<DWORD>(dwParam2), 
            false);
        break;

    case MIM_ERROR:
        m_timestampLastMessageReceived = WindowsMidiServicesInternal::GetCurrentMidiTimestamp();
        EnqueueMidiDataMessage(
            m_timestampLastMessageReceived,
            static_cast<DWORD>(dwParam1),
            static_cast<DWORD>(dwParam2),
            true);
        break;

    case MIM_LONGDATA:
        m_timestampLastMessageReceived = WindowsMidiServicesInternal::GetCurrentMidiTimestamp();
        EnqueueMidiLongDataMessage(
            m_timestampLastMessageReceived,
            (LPMIDIHDR)dwParam1,
            static_cast<DWORD>(dwParam2),
            false);

        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        break;

    case MIM_LONGERROR:
        m_timestampLastMessageReceived = WindowsMidiServicesInternal::GetCurrentMidiTimestamp();
        EnqueueMidiLongDataMessage(
            m_timestampLastMessageReceived,
            (LPMIDIHDR)dwParam1,
            static_cast<DWORD>(dwParam2),
            true);

        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        break;

    case MIM_MOREDATA:
        break;

    default:
        break;
    }

    if (m_timestampFirstMessageReceived == 0)
    {
        m_timestampFirstMessageReceived = m_timestampLastMessageReceived;
    }

}

#define RETURN_INVALID_PORT_NUMBER 1
#define RETURN_UNABLE_TO_OPEN_PORT 2


std::jthread m_displayThread;


int __cdecl main(int argc, char* argv[])
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }


    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteBlankLine();


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

        WriteBlankLine();
        WriteInfoLine(internal::ResourceGetWString(IDS_PROMPT_ENTER_PORT_NUMBER));
        std::wcin >> portNumber;
    }


    if (auto const& port = std::find_if(m_midiInputs.begin(), m_midiInputs.end(),
        [&portNumber](const MidiInputPort& p) { return p.Index == portNumber; });
        port != m_midiInputs.end())
    {
        fmt::println(L"\n{} {} {} {} {} {} {}",
            Styled(internal::ResourceGetWString(IDS_MONITORING_MONITORING), fmt::fg(fmt::color::light_gray)),
            Styled(port->Name, fmt::fg(fmt::color::aqua)),
            Styled(internal::ResourceGetWString(IDS_MONITORING_FOR_INPUT_HIT), fmt::fg(fmt::color::light_gray)),
            Styled(internal::ResourceGetWString(IDS_MONITORING_KEY_ESCAPE), fmt::fg(fmt::color::light_green)),
            Styled(internal::ResourceGetWString(IDS_MONITORING_TO_CANCEL_HIT), fmt::fg(fmt::color::light_gray)),
            Styled(internal::ResourceGetWString(IDS_MONITORING_KEY_SPACEBAR), fmt::fg(fmt::color::light_green)),
            Styled(internal::ResourceGetWString(IDS_MONITORING_TO_TOGGLE_HIDDEN_MESSAGES), fmt::fg(fmt::color::light_gray))
        );

        if (!m_showActiveSense)
        {
            fmt::print(L"{} {} {} ",
                Styled(internal::ResourceGetWString(IDS_MONITORING_HIDING), fmt::fg(fmt::color::light_gray)),
                Styled(internal::ResourceGetWString(IDS_MONITORING_ACTIVE_SENSE), fmt::fg(fmt::color::pink)),
                Styled(internal::ResourceGetWString(IDS_MONITORING_MESSAGES), fmt::fg(fmt::color::light_gray))
            );
        }

        if (!m_showClock)
        {
            fmt::println(L"{} {} {}",
                Styled(internal::ResourceGetWString(IDS_MONITORING_HIDING), fmt::fg(fmt::color::light_gray)),
                Styled(internal::ResourceGetWString(IDS_MONITORING_CLOCK), fmt::fg(fmt::color::pink)),
                Styled(internal::ResourceGetWString(IDS_MONITORING_MESSAGES), fmt::fg(fmt::color::light_gray))
            );
        }

        fmt::println(L"");
    }
    else
    {
        WriteErrorLine(std::format(L"{} is not a valid port number.", portNumber));
        return RETURN_INVALID_PORT_NUMBER;
    }

    if (midiInOpen(&m_hMidiIn, portNumber, (DWORD_PTR)&OnMidiMessageReceived, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR)
    {
        m_header.lpData = (LPSTR)(m_buffer);
        m_header.dwBufferLength = MIDI_BUFFER_SIZE;
        m_header.dwFlags = 0;

        midiInPrepareHeader(m_hMidiIn, &m_header, sizeof(MIDIHDR));
        midiInAddBuffer(m_hMidiIn, &m_header, sizeof(MIDIHDR));


        


        // Create background thread for displaying messages
        std::jthread displayThread(&MessageDisplayWorker);
        m_displayThread = std::move(displayThread);

        midiInStart(m_hMidiIn);
    }
    else
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_OPEN_PORT));
        return RETURN_UNABLE_TO_OPEN_PORT;
    }

    while (true)
    {
        auto ch = _getch();

        if (ch == KEY_ESCAPE)
        {
            WriteInfoLine(L"\n" + internal::ResourceGetWString(IDS_STATUS_CLOSING));
            break;
        }
        else if (ch == KEY_SPACE)
        {
            // toggle showing hidden messages

            WriteInfoLine(L"\n" + internal::ResourceGetWString(IDS_STATUS_TOGGLING_HIDDEN_MESSAGES));

            m_showActiveSense = !m_showActiveSense;
            m_showClock = !m_showClock;
        }

        Sleep(0);
    }

    midiInUnprepareHeader(m_hMidiIn, &m_header, sizeof(MIDIHDR));

    m_displayThread.request_stop();

    midiInStop(m_hMidiIn);
    midiInClose(m_hMidiIn);



    // Show counts of data received as well as first and last timestamp

    uint64_t elapsedTicks = m_timestampLastMessageReceived - m_timestampFirstMessageReceived;

    uint64_t freq = WindowsMidiServicesInternal::GetMidiTimestampFrequency();
    auto elapsedMilliseconds = WindowsMidiServicesInternal::ConvertTimestampToFractionalMilliseconds(elapsedTicks, freq);

    auto averageMillisecondsPerByte = elapsedMilliseconds / m_countAllBytesReceived ;

    fmt::println(L"");
    fmt::println(L"");

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_TOTAL_BYTES_RECEIVED), fmt::fg(fmt::color::light_gray)),
        Styled(m_countAllBytesReceived, fmt::fg(fmt::color::aqua)));

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_STATUS_BYTES_RECEIVED), fmt::fg(fmt::color::light_gray)),
        Styled(m_countStatusBytesReceived, fmt::fg(fmt::color::aqua)));

    fmt::println(L"");

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_TIMESTAMP_FIRST_MESSAGE), fmt::fg(fmt::color::light_gray)),
        Styled(m_timestampFirstMessageReceived, fmt::fg(fmt::color::light_green)));

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_TIMESTAMP_LAST_MESSAGE), fmt::fg(fmt::color::light_gray)),
        Styled(m_timestampLastMessageReceived, fmt::fg(fmt::color::light_green)));

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_ELAPSED_TICKS), fmt::fg(fmt::color::light_gray)),
        Styled(elapsedTicks, fmt::fg(fmt::color::light_green)));

    fmt::println(L"");

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_ELAPSED_MILLISECONDS), fmt::fg(fmt::color::light_gray)),
        Styled(elapsedMilliseconds, fmt::fg(fmt::color::yellow)));

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_AVERAGE_MILLISECONDS_PER_BYTE), fmt::fg(fmt::color::light_gray)),
        Styled(averageMillisecondsPerByte, fmt::fg(fmt::color::yellow)));

    fmt::println(L"");

    return 0;
}


