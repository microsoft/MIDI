// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>
#include <thread>

#include "cmd_system.h"
#include "console_output.h"
#include "console_table.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        constexpr int KeyEscape = 27;

        // Shared by both watchers: block until escape, letting the WinRT events print as they
        // arrive on their own threads.
        void WaitForEscape()
        {
            for (;;)
            {
                if (_kbhit() && _getch() == KeyEscape)
                {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        }

        std::string DescribeServiceState(_In_ DWORD state)
        {
            switch (state)
            {
            case SERVICE_STOPPED:           return "Stopped";
            case SERVICE_START_PENDING:     return "Start Pending";
            case SERVICE_STOP_PENDING:      return "Stop Pending";
            case SERVICE_RUNNING:           return "Running";
            case SERVICE_CONTINUE_PENDING:  return "Continue Pending";
            case SERVICE_PAUSE_PENDING:     return "Pause Pending";
            case SERVICE_PAUSED:            return "Paused";
            default:                        return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }

        std::string DescribeStartType(_In_ DWORD startType)
        {
            switch (startType)
            {
            case SERVICE_BOOT_START:    return "Boot";
            case SERVICE_SYSTEM_START:  return "System";
            case SERVICE_AUTO_START:    return "Automatic";
            case SERVICE_DEMAND_START:  return "Manual";
            case SERVICE_DISABLED:      return "Disabled";
            default:                    return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }
    }

    int RunTimeCommand()
    {
        auto const frequency = midi2::MidiClock::TimestampFrequency();
        auto const now = midi2::MidiClock::Now();

        ConsoleTable table{ ResourceString(IDS_TIME_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, fieldLabelTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_VALUE), ColumnAlignment::Right, numberTextStyle);

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_FREQUENCY));
        table.AddCell(FormatNumberWithSeparators(frequency));

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_NOW));
        table.AddCell(FormatNumberWithSeparators(now));

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_TICKS_PER_MICROSECOND));
        table.AddCell(FormatDecimal(static_cast<double>(frequency) / 1000000.0, 3));

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_TICKS_PER_MILLISECOND));
        table.AddCell(FormatDecimal(static_cast<double>(frequency) / 1000.0, 3));

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_TICKS_PER_SECOND));
        table.AddCell(FormatNumberWithSeparators(frequency));

        if (frequency > 0)
        {
            constexpr double secondsPerYear = 60.0 * 60.0 * 24.0 * 365.0;

            auto const remainingTicks = static_cast<double>(UINT64_MAX - now);
            auto const years = remainingTicks / static_cast<double>(frequency) / secondsPerYear;

            table.BeginRow();
            table.AddCell(ResourceString(IDS_TIME_LABEL_TIME_UNTIL_WRAP));
            table.AddCell(FormatDecimal(years, 1));
        }

        auto const timerSettings = midi2::MidiClock::GetCurrentSystemTimerInfo();

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_SYSTEM_TIMER_MINIMUM));
        table.AddCell(FormatDecimal(
            midi2::MidiClock::ConvertTimestampTicksToMilliseconds(timerSettings.MinimumIntervalTicks), 4) + " ms");

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_SYSTEM_TIMER_MAXIMUM));
        table.AddCell(FormatDecimal(
            midi2::MidiClock::ConvertTimestampTicksToMilliseconds(timerSettings.MaximumIntervalTicks), 4) + " ms");

        table.BeginRow();
        table.AddCell(ResourceString(IDS_TIME_LABEL_SYSTEM_TIMER_CURRENT));
        table.AddCell(FormatDecimal(
            midi2::MidiClock::ConvertTimestampTicksToMilliseconds(timerSettings.CurrentIntervalTicks), 4) + " ms");

        table.Render();

        return 0;
    }

    int RunServiceStatusCommand(_In_ ServiceStatusOptions const& options)
    {
        wil::unique_schandle manager{ OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

        if (!manager)
        {
            WriteErrorLine(ResourceString(IDS_SERVICE_NOT_INSTALLED));
            return 1;
        }

        wil::unique_schandle service{ OpenServiceW(manager.get(), L"MidiSrv", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG) };

        if (!service)
        {
            WriteErrorLine(ResourceString(IDS_SERVICE_NOT_INSTALLED));
            return 1;
        }

        SERVICE_STATUS_PROCESS status{};
        DWORD bytesNeeded{ 0 };

        if (!QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
        {
            WriteErrorLine(ResourceString(IDS_ERROR_GENERAL_FAILURE));
            return 1;
        }

        WriteSectionHeading(ResourceString(IDS_SERVICE_STATUS_TABLE_TITLE));

        auto const isRunning = status.dwCurrentState == SERVICE_RUNNING;

        WriteField(ResourceString(IDS_SERVICE_LABEL_STATE),
            DescribeServiceState(status.dwCurrentState),
            isRunning ? successTextStyle : warningTextStyle);

        // QueryServiceConfig needs a caller-sized buffer; ask for the size first.
        DWORD configBytes{ 0 };

        QueryServiceConfigW(service.get(), nullptr, 0, &configBytes);

        if (configBytes > 0)
        {
            std::vector<BYTE> buffer(configBytes);

            auto* const config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());

            if (QueryServiceConfigW(service.get(), config, configBytes, &configBytes))
            {
                WriteField(ResourceString(IDS_SERVICE_LABEL_START_TYPE), DescribeStartType(config->dwStartType));
                WriteField(ResourceString(IDS_SERVICE_LABEL_SERVICE_NAME), "MidiSrv");

                if (config->lpDisplayName != nullptr)
                {
                    WriteField(ResourceString(IDS_SERVICE_LABEL_DISPLAY_NAME),
                        ToUtf8(std::wstring_view{ config->lpDisplayName }));
                }

                if (options.Verbose && config->lpBinaryPathName != nullptr)
                {
                    WriteField(ResourceString(IDS_SERVICE_LABEL_BINARY_PATH),
                        ToUtf8(std::wstring_view{ config->lpBinaryPathName }));
                }
            }
        }

        if (options.Verbose)
        {
            WriteField(ResourceString(IDS_LABEL_ID), fmt::format("{}", status.dwProcessId), numberTextStyle);
        }

        return isRunning ? 0 : 1;
    }

    int RunServicePingCommand(_In_ ServicePingOptions const& options)
    {
        if (options.Count < 1)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_PING_COUNT_TOO_LOW));
            return 1;
        }

        if (options.Count > 255)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_PING_COUNT_TOO_HIGH));
            return 1;
        }

        if (options.Timeout < 10)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_PING_TIMEOUT_TOO_LOW));
            return 1;
        }

        auto const summary = midi2diag::MidiDiagnostics::PingService(
            static_cast<uint8_t>(options.Count),
            static_cast<uint32_t>(options.Timeout));

        if (summary == nullptr || !summary.Success())
        {
            auto const reason = summary == nullptr ? std::string{} : ToUtf8(summary.FailureReason());

            WriteErrorLine(FormatResourceString(IDS_SERVICE_PING_FAILED, reason));
            return 1;
        }

        auto const responses = summary.Responses();

        if (options.Verbose && responses != nullptr)
        {
            ConsoleTable table{ ResourceString(IDS_SERVICE_PING_TABLE_TITLE) };

            table.AddColumn(ResourceString(IDS_SERVICE_PING_LABEL_INDEX), ColumnAlignment::Right, numberTextStyle);
            table.AddColumn(ResourceString(IDS_SERVICE_PING_LABEL_ROUND_TRIP), ColumnAlignment::Right, timestampTextStyle);

            for (auto const& response : responses)
            {
                table.BeginRow();
                table.AddCell(fmt::format("{}", response.Index()));
                table.AddCell(FormatDecimal(
                    midi2::MidiClock::ConvertTimestampTicksToMicroseconds(response.ClientDeltaTimestamp()), 2) + " \u03BCs");
            }

            table.Render();
        }

        uint64_t minimum{ UINT64_MAX };
        uint64_t maximum{ 0 };

        if (responses != nullptr)
        {
            for (auto const& response : responses)
            {
                minimum = std::min(minimum, response.ClientDeltaTimestamp());
                maximum = std::max(maximum, response.ClientDeltaTimestamp());
            }
        }

        WriteSectionHeading(ResourceString(IDS_SERVICE_PING_SUMMARY));

        WriteField(ResourceString(IDS_SERVICE_PING_SENT),
            fmt::format("{}", responses == nullptr ? 0u : responses.Size()), numberTextStyle);

        WriteField(ResourceString(IDS_SERVICE_PING_AVERAGE),
            FormatDecimal(midi2::MidiClock::ConvertTimestampTicksToMicroseconds(
                summary.AveragePingRoundTripMidiClock()), 2) + " \u03BCs", timestampTextStyle);

        if (minimum != UINT64_MAX)
        {
            WriteField(ResourceString(IDS_SERVICE_PING_MINIMUM),
                FormatDecimal(midi2::MidiClock::ConvertTimestampTicksToMicroseconds(minimum), 2) + " \u03BCs",
                timestampTextStyle);

            WriteField(ResourceString(IDS_SERVICE_PING_MAXIMUM),
                FormatDecimal(midi2::MidiClock::ConvertTimestampTicksToMicroseconds(maximum), 2) + " \u03BCs",
                timestampTextStyle);
        }

        WriteField(ResourceString(IDS_SERVICE_PING_TOTAL),
            FormatDecimal(midi2::MidiClock::ConvertTimestampTicksToMilliseconds(
                summary.TotalPingRoundTripMidiClock()), 2) + " ms", timestampTextStyle);

        return 0;
    }

    int RunWatchEndpointsCommand(_In_ WatchEndpointsOptions const& options)
    {
        auto const filters = BuildEndpointFilters(options.IncludeDiagnosticLoopback, false);

        auto watcher = midi2enum::MidiEndpointDeviceWatcher::Create(filters);

        if (watcher == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_GENERAL_FAILURE));
            return 1;
        }

        auto const addedToken = watcher.Added([&options](auto&&, midi2enum::MidiEndpointDeviceInformationAddedEventArgs const& args)
        {
            auto const device = args.AddedDevice();

            WriteBlankLine();
            WriteLine(fmt::format("{} {} {}",
                Styled(ResourceString(IDS_WATCH_ADDED), successTextStyle),
                Styled(GetEndpointIcon(device), normalTextStyle),
                Styled(ToUtf8(device.Name()), endpointNameTextStyle)));

            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(device.EndpointDeviceId()), endpointIdTextStyle);

            if (options.Verbose)
            {
                WriteField(ResourceString(IDS_LABEL_PURPOSE), FormatEndpointPurpose(device.EndpointPurpose()));
            }
        });

        auto const removedToken = watcher.Removed([](auto&&, midi2enum::MidiEndpointDeviceInformationRemovedEventArgs const& args)
        {
            WriteBlankLine();
            WriteLine(fmt::format("{}", Styled(ResourceString(IDS_WATCH_REMOVED), warningTextStyle)));
            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(args.RemovedDevice().EndpointDeviceId()), endpointIdTextStyle);
        });

        auto const updatedToken = watcher.Updated([&options](auto&&, midi2enum::MidiEndpointDeviceInformationUpdatedEventArgs const& args)
        {
            WriteBlankLine();
            WriteLine(fmt::format("{}", Styled(ResourceString(IDS_WATCH_UPDATED), infoTextStyle)));
            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(args.UpdatedDevice().EndpointDeviceId()), endpointIdTextStyle);

            auto const writeFlag = [](bool set, UINT resourceId)
            {
                if (set)
                {
                    WriteLine(fmt::format("    {}", Styled(ResourceString(resourceId), normalTextStyle)));
                }
            };

            writeFlag(args.IsNameUpdated(), IDS_WATCH_UPDATE_NAME);
            writeFlag(args.IsEndpointInformationUpdated(), IDS_WATCH_UPDATE_ENDPOINT_INFORMATION);
            writeFlag(args.IsStreamConfigurationUpdated(), IDS_WATCH_UPDATE_STREAM_CONFIGURATION);
            writeFlag(args.AreFunctionBlocksUpdated(), IDS_WATCH_UPDATE_FUNCTION_BLOCKS);
            writeFlag(args.IsDeviceIdentityUpdated(), IDS_WATCH_UPDATE_DEVICE_IDENTITY);
            writeFlag(args.IsUserMetadataUpdated(), IDS_WATCH_UPDATE_USER_METADATA);
            writeFlag(args.AreAdditionalCapabilitiesUpdated(), IDS_WATCH_UPDATE_ADDITIONAL_CAPABILITIES);

            if (options.Verbose)
            {
                auto const update = args.DeviceInformationUpdate();

                if (update != nullptr && update.Properties() != nullptr)
                {
                    for (auto const& property : update.Properties())
                    {
                        WriteLine(fmt::format("      {}", Styled(ToUtf8(property.Key()), propertyKeyTextStyle)));
                    }
                }
            }
        });

        auto const completedToken = watcher.EnumerationCompleted([](auto&&, auto&&)
        {
            WriteInfoLine(ResourceString(IDS_WATCH_ENUMERATION_COMPLETED));
        });

        auto const stoppedToken = watcher.Stopped([](auto&&, auto&&)
        {
            WriteInfoLine(ResourceString(IDS_WATCH_STOPPED));
        });

        auto const revoke = wil::scope_exit([&]
        {
            watcher.Added(addedToken);
            watcher.Removed(removedToken);
            watcher.Updated(updatedToken);
            watcher.EnumerationCompleted(completedToken);
            watcher.Stopped(stoppedToken);
        });

        WriteInfoLine(ResourceString(IDS_WATCH_ENDPOINTS_STARTING));

        watcher.Start();

        WaitForEscape();

        watcher.Stop();

        return 0;
    }

    int RunWatchPortsCommand(_In_ WatchPortsOptions const& options)
    {
        auto watcher = midi2legacy::MidiLegacyPortDeviceWatcher::Create();

        if (watcher == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_GENERAL_FAILURE));
            return 1;
        }

        auto const addedToken = watcher.Added([&options](auto&&, midi2legacy::MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            auto const port = args.AddedDevice();

            WriteBlankLine();
            WriteLine(fmt::format("{} {}",
                Styled(ResourceString(IDS_WATCH_ADDED), successTextStyle),
                Styled(ToUtf8(port.Name()), endpointNameTextStyle)));

            WriteField(ResourceString(IDS_LABEL_PORT_NUMBER), fmt::format("{}", port.Number()), numberTextStyle);
            WriteField(ResourceString(IDS_LABEL_DIRECTION), FormatPortFlow(port.Flow()));

            if (options.Verbose)
            {
                WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(port.PortDeviceId()), endpointIdTextStyle);
            }
        });

        auto const removedToken = watcher.Removed([](auto&&, midi2legacy::MidiLegacyPortDeviceInformationRemovedEventArgs const& args)
        {
            WriteBlankLine();
            WriteLine(fmt::format("{}", Styled(ResourceString(IDS_WATCH_REMOVED), warningTextStyle)));
            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(args.RemovedDevice().PortDeviceId()), endpointIdTextStyle);
        });

        auto const updatedToken = watcher.Updated([](auto&&, midi2legacy::MidiLegacyPortDeviceInformationUpdatedEventArgs const& args)
        {
            WriteBlankLine();
            WriteLine(fmt::format("{}", Styled(ResourceString(IDS_WATCH_UPDATED), infoTextStyle)));
            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(args.UpdatedDevice().PortDeviceId()), endpointIdTextStyle);

            if (args.IsNameUpdated())
            {
                WriteLine(fmt::format("    {}", Styled(ResourceString(IDS_WATCH_UPDATE_NAME), normalTextStyle)));
            }

            if (args.IsNumberUpdated())
            {
                WriteLine(fmt::format("    {}", Styled(ResourceString(IDS_WATCH_UPDATE_PORT_NUMBER), normalTextStyle)));
            }
        });

        auto const completedToken = watcher.EnumerationCompleted([](auto&&, auto&&)
        {
            WriteInfoLine(ResourceString(IDS_WATCH_ENUMERATION_COMPLETED));
        });

        auto const stoppedToken = watcher.Stopped([](auto&&, auto&&)
        {
            WriteInfoLine(ResourceString(IDS_WATCH_STOPPED));
        });

        auto const revoke = wil::scope_exit([&]
        {
            watcher.Added(addedToken);
            watcher.Removed(removedToken);
            watcher.Updated(updatedToken);
            watcher.EnumerationCompleted(completedToken);
            watcher.Stopped(stoppedToken);
        });

        WriteInfoLine(ResourceString(IDS_WATCH_PORTS_STARTING));

        watcher.Start();

        WaitForEscape();

        watcher.Stop();

        return 0;
    }
}
