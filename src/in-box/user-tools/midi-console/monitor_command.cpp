// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <thread>

#include "console_output.h"
#include "endpoint_picker.h"
#include "endpoint_utility.h"
#include "message_capture_writer.h"
#include "midi_message_table.h"
#include "monitor_command.h"
#include "pickers.h"
#include "strings.h"
#include "text_entry_prompt.h"

namespace midi2console
{
    namespace
    {
        constexpr int KeyEscape = 27;

        constexpr uint8_t FirstRealTimeStatus = 0xF8;

        bool IsRealTimeMessage(_In_ uint32_t word0)
        {
            if (midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(word0) !=
                midi2::MidiMessageType::SystemCommon32)
            {
                return false;
            }

            return static_cast<uint8_t>((word0 >> 16) & 0xFF) >= FirstRealTimeStatus;
        }

        bool IsUtilityMessage(_In_ uint32_t word0)
        {
            return midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(word0) ==
                midi2::MidiMessageType::UtilityMessage32;
        }

        // Written by the service callback thread, drained by the main thread. The callback does
        // only the arithmetic that has to be accurate at receive time; formatting is much slower
        // and belongs on the display side.
        class MessageQueue
        {
        public:
            void Push(_In_ ReceivedMidiMessage const& message)
            {
                std::lock_guard<std::mutex> guard{ m_lock };
                m_messages.push_back(message);
            }

            std::vector<ReceivedMidiMessage> DrainAll()
            {
                std::lock_guard<std::mutex> guard{ m_lock };

                std::vector<ReceivedMidiMessage> drained;
                drained.reserve(m_messages.size());

                while (!m_messages.empty())
                {
                    drained.push_back(m_messages.front());
                    m_messages.pop_front();
                }

                return drained;
            }

        private:
            std::mutex m_lock;
            std::deque<ReceivedMidiMessage> m_messages;
        };
    }

    int RunMonitorCommand(_In_ MonitorOptions const& options)
    {
        auto endpointDeviceId = options.EndpointDeviceId;
        std::string endpointName;

        if (!ResolveEndpointDeviceId(endpointDeviceId, endpointName))
        {
            return 2;
        }

        auto const endpointInfo = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
            winrt::hstring{ FromUtf8(endpointDeviceId) });

        if (endpointInfo == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_ENDPOINT_NOT_FOUND));
            return 1;
        }

        WriteLine(fmt::format("{} {}",
            Styled(ResourceString(IDS_EP_MONITOR_MONITORING), infoTextStyle),
            Styled(endpointName, endpointNameTextStyle)));

        WriteLine(fmt::format("{}", Styled(endpointDeviceId, endpointIdTextStyle)));

        SetConsoleTitleText(FormatResourceString(IDS_EP_MONITOR_CONSOLE_TITLE, endpointName));

        auto session = midi2::MidiSession::Create(L"MIDI Console - Monitor");

        if (session == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_CREATING_SESSION));
            return 1;
        }

        midi2::MidiEndpointConnectionSettings const connectionSettings{ false, options.AutoReconnect };

        auto connection = session.CreateEndpointConnection(
            winrt::hstring{ FromUtf8(endpointDeviceId) }, connectionSettings);

        if (connection == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_CREATING_CONNECTION));
            return 1;
        }

        MidiMessageTable displayTable{
            endpointInfo.GetDeclaredEndpointInfo().SupportsMidi20Protocol(),
            options.IncludeTimestamp,
            options.DecodeMessages,
            options.Verbose };

        MessageCaptureWriter captureWriter;

        if (!options.CaptureToFile.empty())
        {
            if (!captureWriter.Open(options.CaptureToFile, options.CaptureFieldDelimiter, options.AnnotateCapture))
            {
                WriteErrorLine(ResourceString(IDS_CAPTURE_FAILED));
                return 1;
            }

            WriteLine(fmt::format("{} {}",
                Styled(ResourceString(IDS_CAPTURE_TO), infoTextStyle),
                Styled(captureWriter.ResolvedFileName(), fileNameTextStyle)));
        }

        MessageQueue queue;

        std::atomic<uint32_t> messageIndex{ 0 };
        std::atomic<uint64_t> lastReceivedTimestamp{ 0 };

        auto const messageReceivedToken = connection.MessageReceived(
            [&](midi2::IMidiMessageReceivedEventSource const& /*sender*/,
                midi2::MidiMessageReceivedEventArgs const& args)
            {
                ReceivedMidiMessage message{};

                message.NumWords = args.FillWords(message.Word0, message.Word1, message.Word2, message.Word3);

                if (message.NumWords == 0)
                {
                    return;
                }

                if (!options.IncludeRealTimeMessages && IsRealTimeMessage(message.Word0))
                {
                    return;
                }

                if (!options.IncludeUtilityMessages && IsUtilityMessage(message.Word0))
                {
                    return;
                }

                message.ReceivedTimestamp = midi2::MidiClock::Now();
                message.MessageTimestamp = args.Timestamp();
                message.Index = messageIndex.fetch_add(1) + 1;

                auto const previous = lastReceivedTimestamp.exchange(message.ReceivedTimestamp);

                message.ReceivedOffsetFromLastMessage =
                    previous == 0 ? 0 : message.ReceivedTimestamp - previous;

                queue.Push(message);
            });

        auto const revokeMessageReceived = wil::scope_exit([&]
        {
            connection.MessageReceived(messageReceivedToken);
        });

        if (!connection.Open())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_OPENING_CONNECTION));
            return 1;
        }

        WriteBlankLine();
        WriteInfoLine(ResourceString(IDS_EP_MONITOR_KEYS));
        WriteBlankLine();

        displayTable.OutputHeader();

        bool keepGoing{ true };

        while (keepGoing)
        {
            auto const drained = queue.DrainAll();

            for (auto const& message : drained)
            {
                displayTable.OutputRow(message);

                if (captureWriter.IsOpen())
                {
                    captureWriter.Write(message.MessageTimestamp, message.NumWords, &message.Word0);
                }

                if (options.SingleMessage)
                {
                    keepGoing = false;
                    break;
                }
            }

            // Flush per batch, not per row. Without this a redirected capture stays empty until
            // the buffer fills, and shows nothing at all if the process is terminated.
            if (!drained.empty())
            {
                std::fflush(stdout);
                captureWriter.Flush();
            }

            if (!keepGoing)
            {
                break;
            }

            while (_kbhit())
            {
                auto const key = _getch();

                if (key == KeyEscape)
                {
                    keepGoing = false;
                    break;
                }

                if (key == 'c' || key == 'C')
                {
                    auto const comment = PromptForText(
                        ResourceString(IDS_EP_MONITOR_COMMENT_TITLE),
                        ResourceString(IDS_EP_MONITOR_COMMENT_HINT),
                        ResourceString(IDS_EP_MONITOR_COMMENT_PLACEHOLDER));

                    if (comment.has_value() && !comment->empty())
                    {
                        displayTable.OutputComment(*comment);
                    }
                }
            }

            if (drained.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        displayTable.OutputSeparatorLine();

        WriteLine(fmt::format("{} {}",
            Styled(ResourceString(IDS_EP_MONITOR_MESSAGES_RECEIVED), infoTextStyle),
            Styled(messageIndex.load(), successTextStyle)));

        if (captureWriter.IsOpen())
        {
            captureWriter.Flush();

            WriteInfoLine(FormatResourceString(IDS_CAPTURE_CLOSED,
                fmt::format("{}", captureWriter.MessagesWritten())));
        }

        return 0;
    }
}
