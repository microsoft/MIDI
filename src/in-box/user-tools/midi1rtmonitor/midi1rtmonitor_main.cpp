// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// Monitors a MIDI 1.0 input port through the original Windows.Devices.Midi WinRT API, the way
// midi1monitor does through WinMM, so the two paths can be compared when debugging.

#include "pch.h"

#include "console_tools_shared.h"

using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Midi;

struct MidiInputPort
{
    uint32_t Index{ 0 };
    winrt::hstring Id{};
    winrt::hstring Name{};
};

struct ReceivedMidiMessage
{
    winrt::Windows::Foundation::TimeSpan ApiTimestamp{};
    std::vector<uint8_t> Data{};
};

std::vector<MidiInputPort> m_midiInputs{};

std::atomic<bool> m_showActiveSense{ false };
std::atomic<bool> m_showClock{ false };

std::mutex m_messagesLock;
std::condition_variable_any m_messagesAvailable;
std::queue<ReceivedMidiMessage> m_messages{};

// Only the display thread touches these until it has been joined.
uint32_t m_countAllBytesReceived{ 0 };
uint32_t m_countStatusBytesReceived{ 0 };
uint32_t m_countMessagesNotShown{ 0 };
bool m_displayStopped{ false };

std::atomic<uint64_t> m_timestampFirstMessageReceived{ 0 };
std::atomic<uint64_t> m_timestampLastMessageReceived{ 0 };
std::atomic<uint32_t> m_countMessagesNotRead{ 0 };

// Revoking the event handler does not wait for a call already in progress, so stopping waits
// for this count to reach zero before anything the handler uses can go away.
std::atomic<bool> m_stoppingInput{ false };
std::atomic<uint32_t> m_handlersInProgress{ 0 };


template <typename... TArgs>
std::wstring FormatResourceString(_In_ UINT const resourceId, TArgs&&... args)
{
    // make_wformat_args needs lvalues
    auto values = std::make_tuple(std::forward<TArgs>(args)...);

    return std::apply([resourceId](auto&... unpacked)
        {
            return std::vformat(internal::ResourceGetWString(resourceId), std::make_wformat_args(unpacked...));
        }, values);
}

// Accepts only a plain decimal number, so "-1" or "3x" is not read as some other port.
bool TryParsePortNumber(_In_ std::wstring_view const text, _Out_ uint32_t& portNumber)
{
    portNumber = 0;

    if (text.empty() || text.size() > 9)
    {
        return false;
    }

    uint32_t value{ 0 };

    for (auto const ch : text)
    {
        if (ch < L'0' || ch > L'9')
        {
            return false;
        }

        value = value * 10 + static_cast<uint32_t>(ch - L'0');
    }

    portNumber = value;

    return true;
}


void WriteInputPortSelector(_In_ MidiInputPort const& port)
{
    fmt::println(L"{} : {}",
        Styled(port.Index, fmt::fg(fmt::color::golden_rod)),
        Styled(std::wstring_view{ port.Name }, fmt::fg(fmt::color::light_gray))
        );
}

void LoadInputPorts()
{
    auto const devices = DeviceInformation::FindAllAsync(MidiInPort::GetDeviceSelector()).get();

    uint32_t index{ 0 };

    for (auto const& device : devices)
    {
        m_midiInputs.push_back(MidiInputPort{ index++, device.Id(), device.Name() });
    }
}

void DisplayAllInputPorts()
{
    WriteInfoLine(FormatResourceString(IDS_STATUS_AVAILABLE_INPUT_PORTS, m_midiInputs.size()));

    for (auto const& port : m_midiInputs)
    {
        WriteInputPortSelector(port);
    }
}


// Each message starts its own line with the timestamp Windows.Devices.Midi gave it.
void BeginMessageLine(_In_ winrt::Windows::Foundation::TimeSpan const apiTimestamp)
{
    // TimeSpan counts 100-nanosecond ticks, so four decimal places of milliseconds show it exactly.
    auto const ticks = apiTimestamp.count();
    auto const magnitude = ticks < 0 ? (0 - static_cast<uint64_t>(ticks)) : static_cast<uint64_t>(ticks);
    auto const milliseconds = fmt::format(L"{}{}.{:04}", ticks < 0 ? L"-" : L"", magnitude / 10'000, magnitude % 10'000);

    fmt::print(L"\n{} ", Styled(fmt::format(L"{:>13}", milliseconds), fmt::fg(fmt::color::slate_gray)));
}

// Every byte is written with a leading space, so the whole message stays on its timestamp's line.
void DisplayStatusByte(_In_ uint8_t const status)
{
    if (status == MIDI_SYSEX || status == MIDI_EOX)
    {
        fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::light_green)));
    }
    else if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(status))
    {
        switch (status & 0xF0)
        {
        case MIDI_NOTEOFF:
            fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::dark_cyan)));
            break;
        case MIDI_NOTEON:
            fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::cyan)));
            break;
        case MIDI_MONOAFTERTOUCH:
            fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::golden_rod)));
            break;
        case MIDI_CONTROLCHANGE:
            fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::light_blue)));
            break;
        default:
            fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::magenta)));
        }
    }
    else if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(status))
    {
        fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::gray)));
    }
    else
    {
        // system common: MTC quarter frame, song position, song select, tune request
        fmt::print(L" {}", Styled(fmt::format(L"{:02X}", status), fmt::fg(fmt::color::light_salmon)));
    }
}

void DisplayDataByte(_In_ uint8_t const data)
{
    fmt::print(L" {}", Styled(fmt::format(L"{:02X}", data), fmt::fg(fmt::color::light_gray)));
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

// Decodes only a message of the right length, so a short or long buffer from the API shows as
// raw bytes instead of as a decoding of bytes that were never sent.
void DisplayDecodedChannelVoiceMessage(_In_ std::vector<uint8_t> const& data)
{
    auto const status = data[0];
    auto const channel = static_cast<uint8_t>((status & 0x0F) + 1);

    if (MIDI_MESSAGE_IS_THREE_BYTES(status) && data.size() == 3)
    {
        switch (status & 0xF0)
        {
        case MIDI_NOTEOFF:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_NOTE_OFF), channel, internal::ResourceGetWString(IDS_LABEL_NOTE), data[1], internal::ResourceGetWString(IDS_LABEL_VELOCITY), data[2]);
            break;
        case MIDI_NOTEON:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_NOTE_ON), channel, internal::ResourceGetWString(IDS_LABEL_NOTE), data[1], internal::ResourceGetWString(IDS_LABEL_VELOCITY), data[2]);
            break;
        case MIDI_POLYAFTERTOUCH:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_POLY_AFTERTOUCH), channel, internal::ResourceGetWString(IDS_LABEL_NOTE), data[1], internal::ResourceGetWString(IDS_LABEL_PRESSURE), data[2]);
            break;
        case MIDI_CONTROLCHANGE:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_CONTROL_CHANGE), channel, internal::ResourceGetWString(IDS_LABEL_CONTROLLER), data[1], internal::ResourceGetWString(IDS_LABEL_VALUE), data[2]);
            break;
        case MIDI_PITCHBEND:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_PITCH_BEND), channel, internal::ResourceGetWString(IDS_LABEL_LSB), data[1], internal::ResourceGetWString(IDS_LABEL_MSB), data[2]);
            break;
        default:
            break;
        }
    }
    else if (MIDI_MESSAGE_IS_TWO_BYTES(status) && data.size() == 2)
    {
        switch (status & 0xF0)
        {
        case MIDI_PROGRAMCHANGE:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_PROGRAM_CHANGE), channel, internal::ResourceGetWString(IDS_LABEL_PROGRAM), data[1]);
            break;
        case MIDI_MONOAFTERTOUCH:
            DisplayDecodedChannelVoiceMessage(internal::ResourceGetWString(IDS_MESSAGE_CHANNEL_PRESSURE), channel, internal::ResourceGetWString(IDS_LABEL_PRESSURE), data[1]);
            break;
        default:
            break;
        }
    }
}

void DisplayDecodedSystemRealTimeMessage(_In_ uint8_t const status)
{
    switch (status)
    {
    case MIDI_TIMINGCLOCK:
        fmt::print(L" {}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_CLOCK), fmt::fg(fmt::color::light_cyan)));
        break;
    case MIDI_START:
        fmt::print(L" {}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_START), fmt::fg(fmt::color::green)));
        break;
    case MIDI_CONTINUE:
        fmt::print(L" {}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_CONTINUE), fmt::fg(fmt::color::light_yellow)));
        break;
    case MIDI_STOP:
        fmt::print(L" {}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_STOP), fmt::fg(fmt::color::red)));
        break;
    case MIDI_ACTIVESENSE:
        fmt::print(L" {}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_ACTIVE_SENSE), fmt::fg(fmt::color::gray)));
        break;
    case MIDI_RESET:
        fmt::print(L" {}", Styled(internal::ResourceGetWString(IDS_MESSAGE_SYSTEM_REAL_TIME_RESET), fmt::fg(fmt::color::light_salmon)));
        break;
    }
}

void DisplayMidiMessage(_In_ ReceivedMidiMessage const& msg)
{
    for (auto const b : msg.Data)
    {
        m_countAllBytesReceived++;

        if (MIDI_BYTE_IS_STATUS_BYTE(b))
        {
            m_countStatusBytesReceived++;
        }
    }

    if (msg.Data.empty())
    {
        // An empty message from the API is a defect worth seeing, so it still gets its line.
        BeginMessageLine(msg.ApiTimestamp);
        return;
    }

    auto const status = msg.Data[0];

    if (status == MIDI_ACTIVESENSE && !m_showActiveSense)
    {
        return;
    }

    if (status == MIDI_TIMINGCLOCK && !m_showClock)
    {
        return;
    }

    BeginMessageLine(msg.ApiTimestamp);

    // Every byte the API handed over, so anything unexpected in the buffer can be seen.
    for (auto const b : msg.Data)
    {
        if (MIDI_BYTE_IS_STATUS_BYTE(b))
        {
            DisplayStatusByte(b);
        }
        else
        {
            DisplayDataByte(b);
        }
    }

    if (!MIDI_BYTE_IS_STATUS_BYTE(status))
    {
        return;
    }

    if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(status))
    {
        DisplayDecodedChannelVoiceMessage(msg.Data);
    }
    else if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(status) && msg.Data.size() == 1)
    {
        DisplayDecodedSystemRealTimeMessage(status);
    }
}


void MessageDisplayWorker(_In_ std::stop_token token)
{
    try
    {
        while (true)
        {
            std::queue<ReceivedMidiMessage> ready{};

            {
                std::unique_lock lock(m_messagesLock);

                // After a stop request this still returns true while messages are waiting, so
                // everything received before the port closed is shown.
                if (!m_messagesAvailable.wait(lock, token, [] { return !m_messages.empty(); }))
                {
                    return;
                }

                std::swap(ready, m_messages);
            }

            while (!ready.empty())
            {
                try
                {
                    DisplayMidiMessage(ready.front());
                }
                catch (...)
                {
                    m_countMessagesNotShown++;
                }

                ready.pop();
            }
        }
    }
    catch (...)
    {
        // Reported in the summary. An exception leaving a thread body ends the process.
        m_displayStopped = true;
    }
}


void OnMidiMessageReceived(
    _In_ MidiInPort const& /*sender*/,
    _In_ MidiMessageReceivedEventArgs const& args) noexcept
{
    m_handlersInProgress.fetch_add(1);
    auto handlerDone = wil::scope_exit([]() noexcept { m_handlersInProgress.fetch_sub(1); });

    if (m_stoppingInput.load())
    {
        return;
    }

    try
    {
        auto const receivedTimestamp = internal::GetCurrentMidiTimestamp();

        auto const message = args.Message();
        auto const rawData = message.RawData();

        ReceivedMidiMessage received{};
        received.ApiTimestamp = message.Timestamp();
        received.Data.assign(rawData.data(), rawData.data() + rawData.Length());

        uint64_t noTimestampYet{ 0 };
        m_timestampFirstMessageReceived.compare_exchange_strong(noTimestampYet, receivedTimestamp);
        m_timestampLastMessageReceived.store(receivedTimestamp);

        {
            std::scoped_lock lock(m_messagesLock);
            m_messages.push(std::move(received));
        }

        m_messagesAvailable.notify_one();
    }
    catch (...)
    {
        m_countMessagesNotRead.fetch_add(1);
    }
}

void StopInput(_In_ MidiInPort const& inputPort, _In_ winrt::event_token const& messageReceivedToken) noexcept
{
    m_stoppingInput.store(true);

    inputPort.MessageReceived(messageReceivedToken);

    while (m_handlersInProgress.load() != 0)
    {
        std::this_thread::yield();
    }
}


void WriteSummaryCount(_In_ UINT const labelResourceId, _In_ uint32_t const count)
{
    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(labelResourceId), fmt::fg(fmt::color::light_gray)),
        Styled(count, fmt::fg(fmt::color::pink)));
}

void WriteSummary()
{
    fmt::println(L"");
    fmt::println(L"");

    if (m_displayStopped)
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_DISPLAY_STOPPED));
    }

    if (auto const notRead = m_countMessagesNotRead.load(); notRead > 0)
    {
        WriteSummaryCount(IDS_SUMMARY_MESSAGES_NOT_READ, notRead);
    }

    if (m_countMessagesNotShown > 0)
    {
        WriteSummaryCount(IDS_SUMMARY_MESSAGES_NOT_SHOWN, m_countMessagesNotShown);
    }

    if (m_countAllBytesReceived == 0)
    {
        WriteInfoLine(internal::ResourceGetWString(IDS_SUMMARY_NO_MESSAGES_RECEIVED));
        fmt::println(L"");

        return;
    }

    // Show counts of data received as well as first and last timestamp

    auto const timestampFirstMessageReceived = m_timestampFirstMessageReceived.load();
    auto const timestampLastMessageReceived = m_timestampLastMessageReceived.load();

    uint64_t elapsedTicks = timestampLastMessageReceived - timestampFirstMessageReceived;

    uint64_t freq = internal::GetMidiTimestampFrequency();
    auto elapsedMilliseconds = internal::ConvertTimestampToFractionalMilliseconds(elapsedTicks, freq);

    auto averageMillisecondsPerByte = elapsedMilliseconds / m_countAllBytesReceived;

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_TOTAL_BYTES_RECEIVED), fmt::fg(fmt::color::light_gray)),
        Styled(m_countAllBytesReceived, fmt::fg(fmt::color::aqua)));

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_STATUS_BYTES_RECEIVED), fmt::fg(fmt::color::light_gray)),
        Styled(m_countStatusBytesReceived, fmt::fg(fmt::color::aqua)));

    fmt::println(L"");

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_TIMESTAMP_FIRST_MESSAGE), fmt::fg(fmt::color::light_gray)),
        Styled(timestampFirstMessageReceived, fmt::fg(fmt::color::light_green)));

    fmt::println(L"{:<31} {} ",
        Styled(internal::ResourceGetWString(IDS_SUMMARY_TIMESTAMP_LAST_MESSAGE), fmt::fg(fmt::color::light_gray)),
        Styled(timestampLastMessageReceived, fmt::fg(fmt::color::light_green)));

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
}


int MonitorPort(_In_ int const argc, _In_reads_(argc) char* argv[])
{
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteBlankLine();

    LoadInputPorts();

    uint32_t portNumber{ 0 };
    bool portNumberProvided{ false };

    if (argc > 1)
    {
        portNumberProvided = TryParsePortNumber(std::wstring_view{ winrt::to_hstring(std::string_view{ argv[1] }) }, portNumber);
    }

    if (!portNumberProvided)
    {
        DisplayAllInputPorts();

        WriteBlankLine();
        WriteInfoLine(internal::ResourceGetWString(IDS_PROMPT_ENTER_PORT_NUMBER));

        std::wstring entry{};
        std::wcin >> entry;

        if (!TryParsePortNumber(entry, portNumber))
        {
            WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_PORT_NUMBER, entry));
            return RETURN_INVALID_PORT_NUMBER;
        }
    }

    if (portNumber >= m_midiInputs.size())
    {
        WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_PORT_NUMBER, portNumber));
        return RETURN_INVALID_PORT_NUMBER;
    }

    auto const& selectedPort = m_midiInputs[portNumber];

    fmt::println(L"\n{} {} {} {} {} {} {}",
        Styled(internal::ResourceGetWString(IDS_MONITORING_MONITORING), fmt::fg(fmt::color::light_gray)),
        Styled(std::wstring_view{ selectedPort.Name }, fmt::fg(fmt::color::aqua)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_FOR_INPUT_HIT), fmt::fg(fmt::color::light_gray)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_KEY_ESCAPE), fmt::fg(fmt::color::light_green)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_TO_CANCEL_HIT), fmt::fg(fmt::color::light_gray)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_KEY_SPACEBAR), fmt::fg(fmt::color::light_green)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_TO_TOGGLE_HIDDEN_MESSAGES), fmt::fg(fmt::color::light_gray))
    );

    fmt::println(L"{} {}",
        Styled(internal::ResourceGetWString(IDS_MONITORING_PORT_ID), fmt::fg(fmt::color::light_gray)),
        Styled(std::wstring_view{ selectedPort.Id }, fmt::fg(fmt::color::gray))
    );

    fmt::println(L"{} {} {} {} {} {}",
        Styled(internal::ResourceGetWString(IDS_MONITORING_HIDING), fmt::fg(fmt::color::light_gray)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_ACTIVE_SENSE), fmt::fg(fmt::color::pink)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_MESSAGES), fmt::fg(fmt::color::light_gray)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_HIDING), fmt::fg(fmt::color::light_gray)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_CLOCK), fmt::fg(fmt::color::pink)),
        Styled(internal::ResourceGetWString(IDS_MONITORING_MESSAGES), fmt::fg(fmt::color::light_gray))
    );

    WriteNormalLine(internal::ResourceGetWString(IDS_MONITORING_TIMESTAMPS));

    fmt::println(L"");

    MidiInPort inputPort{ nullptr };

    try
    {
        inputPort = MidiInPort::FromIdAsync(selectedPort.Id).get();
    }
    catch (winrt::hresult_error const& ex)
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_OPEN_PORT));
        WriteErrorLine(std::wstring{ ex.message() });

        return RETURN_UNABLE_TO_OPEN_PORT;
    }

    if (inputPort == nullptr)
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_OPEN_PORT));

        return RETURN_UNABLE_TO_OPEN_PORT;
    }

    std::jthread displayThread(&MessageDisplayWorker);

    auto const messageReceivedToken = inputPort.MessageReceived(&OnMidiMessageReceived);

    // Runs on every way out of here, so a handler call can never outlive what it uses.
    auto stopInput = wil::scope_exit([&]() noexcept
    {
        StopInput(inputPort, messageReceivedToken);
    });

    while (true)
    {
        auto const ch = _getch();

        if (ch == KEY_ESCAPE)
        {
            WriteInfoLine(L"\n" + internal::ResourceGetWString(IDS_STATUS_CLOSING));
            break;
        }
        else if (ch == KEY_SPACE)
        {
            WriteInfoLine(L"\n" + internal::ResourceGetWString(IDS_STATUS_TOGGLING_HIDDEN_MESSAGES));

            m_showActiveSense.store(!m_showActiveSense.load());
            m_showClock.store(!m_showClock.load());
        }
    }

    stopInput.reset();
    inputPort.Close();

    displayThread.request_stop();
    displayThread.join();

    WriteSummary();

    return RETURN_SUCCESS;
}

// Nothing thrown may leave main.
void WriteUnexpectedError() noexcept
{
    try
    {
        HRESULT hr{ E_UNEXPECTED };
        std::wstring message{};

        try
        {
            throw;
        }
        catch (winrt::hresult_error const& ex)
        {
            hr = ex.code();
            message = std::wstring{ ex.message() };
        }
        catch (...)
        {
            hr = wil::ResultFromCaughtException();
            message = std::wstring{ winrt::hresult_error(hr).message() };
        }

        WriteErrorLine(FormatResourceString(IDS_ERROR_UNEXPECTED, static_cast<uint32_t>(hr), message));
    }
    catch (...)
    {
        // Writing to the console failed too, so the exit code is all that is left.
    }
}

int __cdecl main(_In_ int argc, _In_reads_(argc) char* argv[])
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    try
    {
        winrt::init_apartment();

        return MonitorPort(argc, argv);
    }
    catch (...)
    {
        WriteUnexpectedError();
    }

    return RETURN_GENERAL_FAILURE;
}
