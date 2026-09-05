// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include "cmd_sysex.h"
#include "console_output.h"
#include "console_table.h"
#include "endpoint_picker.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "strings.h"
#include "word_parsing.h"

namespace midi2console
{
    namespace
    {
        constexpr int KeyEscape = 27;

        struct SysExSession
        {
            midi2::MidiSession Session{ nullptr };
            midi2::MidiEndpointConnection Connection{ nullptr };
            int FailureCode{ 0 };

            bool IsValid() const noexcept { return Connection != nullptr; }
        };

        // The SDK delivers messages only to handlers that were registered before the connection
        // was opened, so the receive path needs the connection created but not yet open.
        SysExSession OpenConnection(
            _In_ std::string endpointDeviceId,
            _In_ std::wstring_view sessionName,
            _In_ UINT headingResourceId,
            _In_ bool deferOpen = false)
        {
            SysExSession result;

            std::string endpointName;

            if (!ResolveEndpointDeviceId(endpointDeviceId, endpointName))
            {
                result.FailureCode = 2;
                return result;
            }

            result.Session = midi2::MidiSession::Create(winrt::hstring{ sessionName });

            if (result.Session == nullptr)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_CREATING_SESSION));
                result.FailureCode = 1;
                return result;
            }

            auto connection = result.Session.CreateEndpointConnection(winrt::hstring{ FromUtf8(endpointDeviceId) });

            if (connection == nullptr)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_CREATING_CONNECTION));
                result.FailureCode = 1;
                return result;
            }

            if (!deferOpen && !connection.Open())
            {
                WriteErrorLine(ResourceString(IDS_ERROR_OPENING_CONNECTION));
                result.FailureCode = 1;
                return result;
            }

            result.Connection = connection;

            WriteLine(fmt::format("{} {}",
                Styled(ResourceString(headingResourceId), infoTextStyle),
                Styled(endpointName.empty() ? endpointDeviceId : endpointName, endpointNameTextStyle)));

            return result;
        }

        // Single status line rewritten in place, so a long transfer does not scroll the console.
        void WriteProgressLine(_In_ std::string_view text)
        {
            if (StylingEnabled())
            {
                fmt::print("\r\x1b[0K  {}", Styled(text, infoTextStyle));
                std::fflush(stdout);
            }
        }

        void EndProgressLine()
        {
            if (StylingEnabled())
            {
                fmt::print("\r\x1b[0K");
                std::fflush(stdout);
            }
        }

        bool EscapePressed()
        {
            while (_kbhit())
            {
                if (_getch() == KeyEscape)
                {
                    return true;
                }
            }

            return false;
        }

        bool TryValidateGroup(_In_ int groupNumber, _Out_ midi2::MidiGroup& group)
        {
            group = midi2::MidiGroup{ static_cast<uint8_t>(0) };

            if (groupNumber < 1 || groupNumber > 16)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_INVALID_GROUP));
                return false;
            }

            group = midi2::MidiGroup{ static_cast<uint8_t>(groupNumber - 1) };

            return true;
        }
    }

    int RunSysExSendFileCommand(_In_ SysExSendFileOptions const& options)
    {
        midi2::MidiGroup group{ static_cast<uint8_t>(0) };

        if (!TryValidateGroup(options.GroupNumber, group))
        {
            return 1;
        }

        auto const path = ExpandEnvironmentPath(options.InputFile);

        std::error_code fileError{};

        if (!std::filesystem::exists(std::filesystem::path{ FromUtf8(path) }, fileError))
        {
            WriteErrorLine(FormatResourceString(IDS_ERROR_FILE_NOT_FOUND, path));
            return 1;
        }

        auto session = OpenConnection(options.EndpointDeviceId, L"MIDI Console - SysEx Send", IDS_EP_SENDING_TO);

        if (!session.IsValid())
        {
            return session.FailureCode;
        }

        winrt::Windows::Storage::StorageFile file{ nullptr };

        try
        {
            file = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(
                winrt::hstring{ FromUtf8(path) }).get();
        }
        catch (...)
        {
            WriteErrorLine(FormatResourceString(IDS_ERROR_FILE_NOT_FOUND, path));
            return 1;
        }

        auto const properties = file.GetBasicPropertiesAsync().get();
        auto const totalBytes = properties.Size();

        auto const stream = file.OpenSequentialReadAsync().get();

        midi2msg::MidiBytestreamToUmpMessageConverterState converterState{};

        WriteInfoLine(ResourceString(IDS_SYSEX_SENDING));

        auto operation = midi2sysex::MidiSystemExclusiveSender::SendBinarySysEx7ByteDataAsync(
            session.Connection,
            group,
            stream,
            static_cast<uint32_t>(options.MessageTransferCount),
            static_cast<uint16_t>(options.DelayBetweenMessages),
            converterState);

        std::mutex progressLock;
        uint64_t lastBytesRead{ 0 };
        uint64_t lastMessagesSent{ 0 };
        std::atomic<bool> progressChanged{ false };

        operation.Progress([&](auto&&, midi2sysex::MidiSystemExclusiveSendProgress const& progress)
        {
            if (progress == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> guard{ progressLock };

            lastBytesRead = progress.CountBytesRead();
            lastMessagesSent = progress.CountMessagesSent();

            progressChanged = true;
        });

        bool canceled{ false };
        bool succeeded{ false };

        // The progress callback arrives on a WinRT thread; drawing happens here so the console
        // is only written from one thread.
        while (operation.Status() == foundation::AsyncStatus::Started)
        {
            if (progressChanged.exchange(false))
            {
                std::lock_guard<std::mutex> guard{ progressLock };

                WriteProgressLine(FormatResourceString(IDS_SYSEX_SEND_PROGRESS,
                    FormatNumberWithSeparators(lastBytesRead),
                    FormatNumberWithSeparators(lastMessagesSent)));
            }

            if (EscapePressed())
            {
                operation.Cancel();
                canceled = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        try
        {
            succeeded = operation.get();
        }
        catch (winrt::hresult_canceled const&)
        {
            canceled = true;
        }
        catch (...)
        {
            succeeded = false;
        }

        EndProgressLine();

        if (canceled)
        {
            WriteWarningLine(ResourceString(IDS_STATUS_CANCELED));
            return 2;
        }

        if (!succeeded)
        {
            WriteErrorLine(ResourceString(IDS_SYSEX_SEND_FAILED));
            return 1;
        }

        WriteField(ResourceString(IDS_SYSEX_MESSAGES), FormatNumberWithSeparators(lastMessagesSent), numberTextStyle);
        WriteSuccessLine(FormatResourceString(IDS_SYSEX_SEND_SUCCEEDED, FormatNumberWithSeparators(totalBytes)));

        return 0;
    }

    int RunSysExReceiveFileCommand(_In_ SysExReceiveFileOptions const& options)
    {
        midi2::MidiGroup group{ static_cast<uint8_t>(0) };

        if (!TryValidateGroup(options.GroupNumber, group))
        {
            return 1;
        }

        auto const path = ExpandEnvironmentPath(options.OutputFile);
        auto const filePath = std::filesystem::path{ FromUtf8(path) };

        std::error_code fileError{};

        if (!options.Overwrite && std::filesystem::exists(filePath, fileError))
        {
            WriteErrorLine(FormatResourceString(IDS_SYSEX_FILE_EXISTS, path));
            return 1;
        }

        auto session = OpenConnection(options.EndpointDeviceId, L"MIDI Console - SysEx Receive",
            IDS_SYSEX_RECEIVING_FROM, true);

        if (!session.IsValid())
        {
            return session.FailureCode;
        }

        std::mutex bufferLock;
        std::vector<uint8_t> received;
        std::atomic<bool> dataChanged{ false };

        midi2sysex::MidiSystemExclusiveReceiver receiver{ session.Connection, group, 256 };

        auto const token = receiver.BytesReceived(
            [&](auto&&, midi2sysex::MidiSystemExclusiveReceivedEventArgs const& args)
            {
                if (args == nullptr)
                {
                    return;
                }

                auto const view = args.Bytes();

                if (view == nullptr || view.Size() == 0)
                {
                    return;
                }

                std::vector<uint8_t> chunk(view.Size());

                view.GetMany(0, winrt::array_view<uint8_t>{ chunk.data(), chunk.data() + chunk.size() });

                std::lock_guard<std::mutex> guard{ bufferLock };

                received.insert(received.end(), chunk.begin(), chunk.end());

                dataChanged = true;
            });

        auto const revoke = wil::scope_exit([&] { receiver.BytesReceived(token); });

        if (!session.Connection.Open())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_OPENING_CONNECTION));
            return 1;
        }

        if (!receiver.Start())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_OPENING_CONNECTION));
            return 1;
        }

        WriteInfoLine(ResourceString(IDS_SYSEX_RECEIVING));

        auto const timeout = std::chrono::seconds(options.TimeoutSeconds);
        auto lastActivity = std::chrono::steady_clock::now();
        bool timedOut{ false };

        for (;;)
        {
            if (dataChanged.exchange(false))
            {
                lastActivity = std::chrono::steady_clock::now();

                std::lock_guard<std::mutex> guard{ bufferLock };

                WriteProgressLine(FormatResourceString(IDS_SYSEX_RECEIVE_PROGRESS,
                    FormatNumberWithSeparators(received.size())));
            }

            if (EscapePressed())
            {
                break;
            }

            // The timer restarts on every arrival, so a long multi-part dump is not cut short.
            if (options.TimeoutSeconds > 0 && std::chrono::steady_clock::now() - lastActivity >= timeout)
            {
                timedOut = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        // Stop flushes anything still buffered, which can raise further events.
        receiver.Stop();

        EndProgressLine();

        if (timedOut)
        {
            WriteInfoLine(FormatResourceString(IDS_SYSEX_RECEIVE_TIMED_OUT, options.TimeoutSeconds));
        }

        std::lock_guard<std::mutex> guard{ bufferLock };

        if (received.empty())
        {
            WriteWarningLine(ResourceString(IDS_SYSEX_RECEIVE_NOTHING));
            return 0;
        }

        std::ofstream output{ filePath, std::ios::binary | std::ios::trunc };

        if (!output.is_open())
        {
            WriteErrorLine(FormatResourceString(IDS_ERROR_FILE_NOT_FOUND, path));
            return 1;
        }

        output.write(reinterpret_cast<char const*>(received.data()), static_cast<std::streamsize>(received.size()));
        output.close();

        WriteSuccessLine(FormatResourceString(IDS_SYSEX_RECEIVE_SAVED,
            FormatNumberWithSeparators(received.size()), path));

        return 0;
    }
}
