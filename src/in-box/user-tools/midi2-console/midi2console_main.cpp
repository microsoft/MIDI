// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>

#include <CLI/CLI.hpp>

#include "cmd_bluetooth.h"
#include "cmd_endpoint.h"
#include "cmd_enumerate.h"
#include "cmd_loopback.h"
#include "cmd_sysex.h"
#include "cmd_system.h"
#include "console_output.h"
#include "help_formatter.h"
#include "midi_formatting.h"
#include "monitor_command.h"
#include "strings.h"

using namespace midi2console;

namespace
{
    std::vector<std::string> GetUtf8CommandLineArguments()
    {
        std::vector<std::string> arguments;

        int argumentCount{ 0 };

        wil::unique_hlocal_ptr<PWSTR[]> wideArguments{ CommandLineToArgvW(GetCommandLineW(), &argumentCount) };

        if (wideArguments == nullptr)
        {
            return arguments;
        }

        for (int i = 0; i < argumentCount; i++)
        {
            arguments.push_back(ToUtf8(std::wstring_view{ wideArguments[i] }));
        }

        return arguments;
    }

    // The shipping console takes the endpoint id between the branch and the sub-command
    // ("midi endpoint <id> monitor"). CLI11 cannot express that, so the id is moved to the end
    // where it is parsed as the sub-command's positional. Existing scripts keep working.
    void NormalizeEndpointArgumentOrder(_Inout_ std::vector<std::string>& arguments)
    {
        static const std::vector<std::string> endpointSubcommands
        {
            "monitor", "listen",
            "send-message", "send-ump", "send",
            "send-message-file", "send-ump-file", "send-file",
            "play-notes", "play",
            "properties", "props", "information", "info",
            "request", "req"
        };

        if (arguments.size() < 4)
        {
            return;
        }

        if (!EqualsIgnoreCase(arguments[1], "endpoint") && !EqualsIgnoreCase(arguments[1], "ep"))
        {
            return;
        }

        auto const& candidate = arguments[2];

        if (candidate.empty() || candidate[0] == '-')
        {
            return;
        }

        auto const isSubcommand = std::any_of(endpointSubcommands.begin(), endpointSubcommands.end(),
            [&candidate](auto const& name) { return EqualsIgnoreCase(candidate, name); });

        if (isSubcommand)
        {
            return;
        }

        auto const endpointId = candidate;

        arguments.erase(arguments.begin() + 2);
        arguments.push_back(endpointId);
    }
}

int main()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    InitializeConsole();

    WriteBlankLine();
    WriteLine(fmt::format("{}", Styled(ResourceString(IDS_APP_TITLE), appTitleTextStyle)));
    WriteBlankLine();

    // The banner above stands in for the description, so help does not repeat it.
    CLI::App app{ "", "midi2" };

    app.require_subcommand(1);
    app.set_version_flag("--version", std::string{ "1.0.0.0" });
    app.set_help_all_flag("--help-all", "Show help for every command");
    app.footer(ResourceString(IDS_APP_BANNER));

    app.formatter(std::make_shared<MidiHelpFormatter>());

    // ---------------------------------------------------------------- enumerate

    auto enumerateCommand = app.add_subcommand("enumerate", ResourceString(IDS_CMD_ENUMERATE));
    enumerateCommand->alias("enum");
    enumerateCommand->alias("list");
    enumerateCommand->require_subcommand(1);

    EnumEndpointsOptions enumEndpointsOptions{};

    auto enumEndpointsCommand = enumerateCommand->add_subcommand(
        "midi-services-endpoints", ResourceString(IDS_CMD_ENUM_ENDPOINTS));
    enumEndpointsCommand->alias("endpoints");
    enumEndpointsCommand->alias("ump-endpoints");
    enumEndpointsCommand->alias("ep");
    enumEndpointsCommand->add_flag("-i,--show-endpoint-id", enumEndpointsOptions.ShowEndpointId,
        ResourceString(IDS_OPT_SHOW_ENDPOINT_ID));
    enumEndpointsCommand->add_flag("-l,--include-diagnostic-loopback", enumEndpointsOptions.IncludeDiagnosticLoopback,
        ResourceString(IDS_OPT_INCLUDE_DIAGNOSTIC_LOOPBACK));
    enumEndpointsCommand->add_flag("-a,--all", enumEndpointsOptions.IncludeAll,
        ResourceString(IDS_OPT_INCLUDE_ALL_ENDPOINTS));
    enumEndpointsCommand->add_flag("-v,--verbose", enumEndpointsOptions.Verbose,
        ResourceString(IDS_OPT_VERBOSE));

    EnumLegacyOptions enumLegacyOptions{};

    auto enumLegacyCommand = enumerateCommand->add_subcommand(
        "legacy-winrt-api-endpoints", ResourceString(IDS_CMD_ENUM_LEGACY));
    enumLegacyCommand->alias("legacy-endpoints");
    enumLegacyCommand->alias("bytestream-endpoints");
    enumLegacyCommand->alias("legacy");
    enumLegacyCommand->alias("winrt1");
    enumLegacyCommand->add_option("-d,--direction", enumLegacyOptions.Direction,
        ResourceString(IDS_OPT_LEGACY_DIRECTION));
    enumLegacyCommand->add_flag("-i,--include-endpoint-id,!--no-include-endpoint-id", enumLegacyOptions.IncludePortId,
        ResourceString(IDS_OPT_INCLUDE_PORT_ID));

    EnumSessionsOptions enumSessionsOptions{};

    auto enumSessionsCommand = enumerateCommand->add_subcommand(
        "active-sessions", ResourceString(IDS_CMD_ENUM_SESSIONS));
    enumSessionsCommand->alias("sessions");
    enumSessionsCommand->add_flag("-a,--all", enumSessionsOptions.All, ResourceString(IDS_OPT_SESSIONS_ALL));
    enumSessionsCommand->add_flag("-v,--verbose", enumSessionsOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));

    EnumTransportsOptions enumTransportsOptions{};

    auto enumTransportsCommand = enumerateCommand->add_subcommand(
        "transport-plugins", ResourceString(IDS_CMD_ENUM_TRANSPORTS));
    enumTransportsCommand->alias("transports");
    enumTransportsCommand->add_flag("-v,--verbose", enumTransportsOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));

    auto enumPropertyKeysCommand = enumerateCommand->add_subcommand(
        "endpoint-property-keys", ResourceString(IDS_CMD_ENUM_PROPERTY_KEYS));
    enumPropertyKeysCommand->alias("property-keys");

    // ---------------------------------------------------------------- endpoint

    auto endpointCommand = app.add_subcommand("endpoint", ResourceString(IDS_CMD_ENDPOINT));
    endpointCommand->alias("ep");
    endpointCommand->require_subcommand(1);

    MonitorOptions monitorOptions{};

    auto monitorCommand = endpointCommand->add_subcommand("monitor", ResourceString(IDS_CMD_EP_MONITOR));
    monitorCommand->alias("listen");
    monitorCommand->add_option("endpoint-id", monitorOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    monitorCommand->add_flag("-s,--single-message", monitorOptions.SingleMessage, ResourceString(IDS_OPT_SINGLE_MESSAGE));
    monitorCommand->add_flag("-v,--verbose", monitorOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));
    monitorCommand->add_flag("-t,--include-timestamp", monitorOptions.IncludeTimestamp, ResourceString(IDS_OPT_INCLUDE_TIMESTAMP));
    monitorCommand->add_flag("-d,--decode-messages,!-D,!--no-decode-messages", monitorOptions.DecodeMessages, ResourceString(IDS_OPT_DECODE_MESSAGES));
    monitorCommand->add_flag("-r,--include-real-time-messages", monitorOptions.IncludeRealTimeMessages, ResourceString(IDS_OPT_INCLUDE_REAL_TIME));
    monitorCommand->add_flag("-u,--include-utility-messages,!-U,!--no-include-utility-messages", monitorOptions.IncludeUtilityMessages, ResourceString(IDS_OPT_INCLUDE_UTILITY));
    monitorCommand->add_flag("-a,--auto-reconnect,!-A,!--no-auto-reconnect", monitorOptions.AutoReconnect, ResourceString(IDS_OPT_AUTO_RECONNECT));

    EndpointPropertiesOptions propertiesOptions{};

    auto propertiesCommand = endpointCommand->add_subcommand("properties", ResourceString(IDS_CMD_EP_PROPERTIES));
    propertiesCommand->alias("props");
    propertiesCommand->alias("information");
    propertiesCommand->alias("info");
    propertiesCommand->add_option("endpoint-id", propertiesOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    propertiesCommand->add_flag("-v,--verbose", propertiesOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));
    propertiesCommand->add_flag("-r,--include-raw-properties,--include-raw", propertiesOptions.IncludeRawProperties, ResourceString(IDS_OPT_INCLUDE_RAW_PROPERTIES));
    propertiesCommand->add_flag("-n,--include-name-table,--include-names", propertiesOptions.IncludeNameTable, ResourceString(IDS_OPT_INCLUDE_NAME_TABLE));

    EndpointSendMessageOptions sendMessageOptions{};

    auto sendMessageCommand = endpointCommand->add_subcommand("send-message", ResourceString(IDS_CMD_EP_SEND_MESSAGE));
    sendMessageCommand->alias("send-ump");
    sendMessageCommand->alias("send");
    sendMessageCommand->add_option("words", sendMessageOptions.Words, ResourceString(IDS_OPT_MIDI_WORDS_ARGUMENT))->expected(0, 5);
    sendMessageCommand->add_option("--endpoint-id", sendMessageOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    sendMessageCommand->add_option("-p,--pause,--delay", sendMessageOptions.DelayBetweenMessages, ResourceString(IDS_OPT_PAUSE));
    sendMessageCommand->add_option("-w,--word-format", sendMessageOptions.WordDataFormat, ResourceString(IDS_OPT_WORD_FORMAT));
    sendMessageCommand->add_flag("-n,--no-wait", sendMessageOptions.NoWait, ResourceString(IDS_OPT_NO_WAIT));
    sendMessageCommand->add_option("-c,--count", sendMessageOptions.Count, ResourceString(IDS_OPT_COUNT));
    sendMessageCommand->add_option("-o,--offset-microseconds", sendMessageOptions.TimestampOffsetMicroseconds, ResourceString(IDS_OPT_OFFSET_MICROSECONDS));
    auto timestampOption = sendMessageCommand->add_option("-t,--timestamp", sendMessageOptions.Timestamp, ResourceString(IDS_OPT_TIMESTAMP));
    sendMessageCommand->add_flag("-i,--debug-auto-increment,--increment", sendMessageOptions.DebugAutoIncrementLastWord, ResourceString(IDS_OPT_DEBUG_AUTO_INCREMENT));

    EndpointSendMessageFileOptions sendMessageFileOptions{};

    auto sendMessageFileCommand = endpointCommand->add_subcommand("send-message-file", ResourceString(IDS_CMD_EP_SEND_MESSAGE_FILE));
    sendMessageFileCommand->alias("send-ump-file");
    sendMessageFileCommand->alias("send-file");
    sendMessageFileCommand->add_option("input-file", sendMessageFileOptions.InputFile, ResourceString(IDS_OPT_INPUT_FILE_ARGUMENT))->required();
    sendMessageFileCommand->add_option("--endpoint-id", sendMessageFileOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    sendMessageFileCommand->add_option("-p,--pause,--delay", sendMessageFileOptions.DelayBetweenMessages, ResourceString(IDS_OPT_PAUSE));
    sendMessageFileCommand->add_option("-w,--word-format", sendMessageFileOptions.WordDataFormat, ResourceString(IDS_OPT_WORD_FORMAT));
    sendMessageFileCommand->add_flag("-n,--no-wait", sendMessageFileOptions.NoWait, ResourceString(IDS_OPT_NO_WAIT));
    sendMessageFileCommand->add_option("-d,--delimiter", sendMessageFileOptions.FieldDelimiter, ResourceString(IDS_OPT_FILE_DELIMITER));
    sendMessageFileCommand->add_flag("-v,--verbose", sendMessageFileOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));
    auto newGroupOption = sendMessageFileCommand->add_option("-g,--new-group-index", sendMessageFileOptions.NewGroupIndex, ResourceString(IDS_OPT_NEW_GROUP_INDEX));

    EndpointPlayNotesOptions playNotesOptions{};

    auto playNotesCommand = endpointCommand->add_subcommand("play-notes", ResourceString(IDS_CMD_EP_PLAY_NOTES));
    playNotesCommand->alias("play");
    playNotesCommand->add_option("notes", playNotesOptions.NoteIndexes, ResourceString(IDS_OPT_NOTE_INDEXES_ARGUMENT));
    playNotesCommand->add_option("--endpoint-id", playNotesOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    playNotesCommand->add_option("-l,--length,--length-ms,--length-milliseconds", playNotesOptions.Length, ResourceString(IDS_OPT_NOTE_LENGTH));
    playNotesCommand->add_option("-r,--rest,--rest-ms,--rest-milliseconds", playNotesOptions.Rest, ResourceString(IDS_OPT_NOTE_REST));
    playNotesCommand->add_option("-g,--group,--group-number", playNotesOptions.GroupNumber, ResourceString(IDS_OPT_GROUP_NUMBER));
    playNotesCommand->add_option("-c,--channel,--channel-number", playNotesOptions.ChannelNumber, ResourceString(IDS_OPT_CHANNEL_NUMBER));
    playNotesCommand->add_option("-v,--velocity,--velocity-percent", playNotesOptions.Velocity, ResourceString(IDS_OPT_VELOCITY));
    playNotesCommand->add_flag("-f,--forever,--repeat-forever", playNotesOptions.Forever, ResourceString(IDS_OPT_FOREVER));
    playNotesCommand->add_flag("-m,--midi2", playNotesOptions.Midi2, ResourceString(IDS_OPT_MIDI2));
    playNotesCommand->add_flag("-a,--auto-reconnect,!-A,!--no-auto-reconnect", playNotesOptions.AutoReconnect, ResourceString(IDS_OPT_AUTO_RECONNECT));

    auto requestCommand = endpointCommand->add_subcommand("request", ResourceString(IDS_CMD_EP_REQUEST));
    requestCommand->alias("req");
    requestCommand->require_subcommand(1);

    EndpointRequestFunctionBlocksOptions requestFunctionBlocksOptions{};

    auto requestFunctionBlocksCommand = requestCommand->add_subcommand("function-blocks", ResourceString(IDS_CMD_EP_REQUEST_FUNCTION_BLOCKS));
    requestFunctionBlocksCommand->alias("function-block");
    requestFunctionBlocksCommand->alias("fb");
    requestFunctionBlocksCommand->alias("function");
    requestFunctionBlocksCommand->alias("functions");
    requestFunctionBlocksCommand->add_option("endpoint-id", requestFunctionBlocksOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    requestFunctionBlocksCommand->add_flag("-a,--all", requestFunctionBlocksOptions.RequestAll, ResourceString(IDS_OPT_REQUEST_ALL));
    requestFunctionBlocksCommand->add_option("-n,--function-block-number,--number", requestFunctionBlocksOptions.FunctionBlockNumber, ResourceString(IDS_OPT_FUNCTION_BLOCK_NUMBER));
    requestFunctionBlocksCommand->add_flag("-i,--request-info,!--no-request-info", requestFunctionBlocksOptions.RequestInfo, ResourceString(IDS_OPT_REQUEST_INFO));
    requestFunctionBlocksCommand->add_flag("-f,--request-name,!--no-request-name", requestFunctionBlocksOptions.RequestName, ResourceString(IDS_OPT_REQUEST_NAME_NOTIFICATION));

    EndpointRequestEndpointInfoOptions requestEndpointInfoOptions{};

    auto requestEndpointInfoCommand = requestCommand->add_subcommand("endpoint-info", ResourceString(IDS_CMD_EP_REQUEST_ENDPOINT_INFO));
    requestEndpointInfoCommand->alias("endpoint-metadata");
    requestEndpointInfoCommand->alias("endpoint-data");
    requestEndpointInfoCommand->alias("em");
    requestEndpointInfoCommand->alias("metadata");
    requestEndpointInfoCommand->add_option("endpoint-id", requestEndpointInfoOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    requestEndpointInfoCommand->add_flag("-a,--all", requestEndpointInfoOptions.RequestAll, ResourceString(IDS_OPT_REQUEST_ALL));
    requestEndpointInfoCommand->add_flag("-i,--endpoint-info,!--no-endpoint-info", requestEndpointInfoOptions.RequestEndpointInfo, ResourceString(IDS_OPT_REQUEST_ENDPOINT_INFO));
    requestEndpointInfoCommand->add_flag("-d,--device-identity", requestEndpointInfoOptions.RequestDeviceIdentity, ResourceString(IDS_OPT_REQUEST_DEVICE_IDENTITY));
    requestEndpointInfoCommand->add_flag("-n,--name", requestEndpointInfoOptions.RequestEndpointName, ResourceString(IDS_OPT_REQUEST_ENDPOINT_NAME));
    requestEndpointInfoCommand->add_flag("-p,--product-instance-id", requestEndpointInfoOptions.RequestProductInstanceId, ResourceString(IDS_OPT_REQUEST_PRODUCT_INSTANCE_ID));
    requestEndpointInfoCommand->add_flag("-s,--stream-configuration", requestEndpointInfoOptions.RequestStreamConfiguration, ResourceString(IDS_OPT_REQUEST_STREAM_CONFIGURATION));
    requestEndpointInfoCommand->add_option("-j,--ump-version-major", requestEndpointInfoOptions.UmpVersionMajor, ResourceString(IDS_OPT_UMP_VERSION_MAJOR));
    requestEndpointInfoCommand->add_option("-m,--ump-version-minor", requestEndpointInfoOptions.UmpVersionMinor, ResourceString(IDS_OPT_UMP_VERSION_MINOR));

    // ---------------------------------------------------------------- sysex

    auto sysExCommand = app.add_subcommand("sysex", ResourceString(IDS_CMD_SYSEX));
    sysExCommand->alias("system-exclusive");
    sysExCommand->require_subcommand(1);

    SysExSendFileOptions sysExSendOptions{};

    auto sysExSendCommand = sysExCommand->add_subcommand("send-file", ResourceString(IDS_CMD_SYSEX_SEND_FILE));
    sysExSendCommand->alias("send");
    sysExSendCommand->add_option("input-file", sysExSendOptions.InputFile, ResourceString(IDS_OPT_INPUT_FILE_ARGUMENT))->required();
    sysExSendCommand->add_option("endpoint-id", sysExSendOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    sysExSendCommand->add_option("-g,--group,--group-number", sysExSendOptions.GroupNumber, ResourceString(IDS_OPT_SYSEX_GROUP_NUMBER));
    sysExSendCommand->add_option("-p,--pause,--delay", sysExSendOptions.DelayBetweenMessages, ResourceString(IDS_OPT_PAUSE));
    sysExSendCommand->add_option("-m,--message-transfer-count,--messages", sysExSendOptions.MessageTransferCount, ResourceString(IDS_OPT_MESSAGE_TRANSFER_COUNT));

    SysExReceiveFileOptions sysExReceiveOptions{};

    auto sysExReceiveCommand = sysExCommand->add_subcommand("receive-file", ResourceString(IDS_CMD_SYSEX_RECEIVE_FILE));
    sysExReceiveCommand->alias("receive");
    sysExReceiveCommand->add_option("output-file", sysExReceiveOptions.OutputFile, ResourceString(IDS_OPT_SYSEX_OUTPUT_FILE_ARGUMENT))->required();
    sysExReceiveCommand->add_option("endpoint-id", sysExReceiveOptions.EndpointDeviceId, ResourceString(IDS_OPT_ENDPOINT_DEVICE_ID));
    sysExReceiveCommand->add_option("-g,--group,--group-number", sysExReceiveOptions.GroupNumber, ResourceString(IDS_OPT_SYSEX_GROUP_NUMBER));
    sysExReceiveCommand->add_option("-t,--timeout-seconds,--timeout", sysExReceiveOptions.TimeoutSeconds, ResourceString(IDS_OPT_SYSEX_TIMEOUT));
    sysExReceiveCommand->add_flag("-o,--overwrite", sysExReceiveOptions.Overwrite, ResourceString(IDS_OPT_SYSEX_OVERWRITE));

    // ---------------------------------------------------------------- loopback

    auto loopbackCommand = app.add_subcommand("loopback", ResourceString(IDS_CMD_LOOPBACK));
    loopbackCommand->alias("midi2-loopback");
    loopbackCommand->alias("bidirectional-loopback");
    loopbackCommand->require_subcommand(1);

    auto loopbackListCommand = loopbackCommand->add_subcommand("list", ResourceString(IDS_CMD_LOOPBACK_LIST));

    LoopbackCreateOptions loopbackCreateOptions{};

    auto loopbackCreateCommand = loopbackCommand->add_subcommand("create", ResourceString(IDS_CMD_LOOPBACK_CREATE));
    loopbackCreateCommand->add_option("-a,--name-a", loopbackCreateOptions.NameA, ResourceString(IDS_OPT_LOOPBACK_NAME_A));
    loopbackCreateCommand->add_option("-b,--name-b", loopbackCreateOptions.NameB, ResourceString(IDS_OPT_LOOPBACK_NAME_B));
    loopbackCreateCommand->add_option("-r,--root-name", loopbackCreateOptions.RootName, ResourceString(IDS_OPT_LOOPBACK_ROOT_NAME));
    loopbackCreateCommand->add_option("-u,--unique-identifier", loopbackCreateOptions.UniqueIdentifier, ResourceString(IDS_OPT_UNIQUE_IDENTIFIER));
    loopbackCreateCommand->add_flag("-s,--save-to-config", loopbackCreateOptions.SaveToConfig, ResourceString(IDS_OPT_SAVE_TO_CONFIG));

    LoopbackRemoveOptions loopbackRemoveOptions{};

    auto loopbackRemoveCommand = loopbackCommand->add_subcommand("remove", ResourceString(IDS_CMD_LOOPBACK_REMOVE));
    loopbackRemoveCommand->alias("delete");
    loopbackRemoveCommand->add_option("-i,--association-id", loopbackRemoveOptions.AssociationId, ResourceString(IDS_OPT_ASSOCIATION_ID))->required();
    loopbackRemoveCommand->add_flag("-s,--save-to-config", loopbackRemoveOptions.SaveToConfig, ResourceString(IDS_OPT_SAVE_TO_CONFIG));

    auto basicLoopbackCommand = app.add_subcommand("basic-loopback", ResourceString(IDS_CMD_BASIC_LOOPBACK));
    basicLoopbackCommand->alias("midi1-loopback");
    basicLoopbackCommand->alias("simple-loopback");
    basicLoopbackCommand->require_subcommand(1);

    auto basicLoopbackListCommand = basicLoopbackCommand->add_subcommand("list", ResourceString(IDS_CMD_BASIC_LOOPBACK_LIST));

    BasicLoopbackCreateOptions basicLoopbackCreateOptions{};

    auto basicLoopbackCreateCommand = basicLoopbackCommand->add_subcommand("create", ResourceString(IDS_CMD_BASIC_LOOPBACK_CREATE));
    basicLoopbackCreateCommand->add_option("-n,--name", basicLoopbackCreateOptions.Name, ResourceString(IDS_OPT_BASIC_LOOPBACK_NAME));
    basicLoopbackCreateCommand->add_option("-u,--unique-identifier", basicLoopbackCreateOptions.UniqueIdentifier, ResourceString(IDS_OPT_UNIQUE_IDENTIFIER));
    basicLoopbackCreateCommand->add_flag("-s,--save-to-config", basicLoopbackCreateOptions.SaveToConfig, ResourceString(IDS_OPT_SAVE_TO_CONFIG));

    LoopbackRemoveOptions basicLoopbackRemoveOptions{};

    auto basicLoopbackRemoveCommand = basicLoopbackCommand->add_subcommand("remove", ResourceString(IDS_CMD_BASIC_LOOPBACK_REMOVE));
    basicLoopbackRemoveCommand->alias("delete");
    basicLoopbackRemoveCommand->add_option("-i,--association-id", basicLoopbackRemoveOptions.AssociationId, ResourceString(IDS_OPT_ASSOCIATION_ID))->required();
    basicLoopbackRemoveCommand->add_flag("-s,--save-to-config", basicLoopbackRemoveOptions.SaveToConfig, ResourceString(IDS_OPT_SAVE_TO_CONFIG));

    // ---------------------------------------------------------------- service, time, watch

    auto serviceCommand = app.add_subcommand("service", ResourceString(IDS_CMD_SERVICE));
    serviceCommand->alias("svc");
    serviceCommand->require_subcommand(1);

    ServiceStatusOptions serviceStatusOptions{};

    auto serviceStatusCommand = serviceCommand->add_subcommand("status", ResourceString(IDS_CMD_SERVICE_STATUS));
    serviceStatusCommand->add_flag("-v,--verbose", serviceStatusOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));

    ServicePingOptions servicePingOptions{};

    auto servicePingCommand = serviceCommand->add_subcommand("ping", ResourceString(IDS_CMD_SERVICE_PING));
    servicePingCommand->add_option("-c,--count", servicePingOptions.Count, ResourceString(IDS_OPT_PING_COUNT));
    servicePingCommand->add_option("-t,--timeout", servicePingOptions.Timeout, ResourceString(IDS_OPT_PING_TIMEOUT));
    servicePingCommand->add_flag("-v,--verbose", servicePingOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));

    auto timeCommand = app.add_subcommand("time", ResourceString(IDS_CMD_TIME));
    timeCommand->alias("clock");

    WatchEndpointsOptions watchEndpointsOptions{};

    auto watchEndpointsCommand = app.add_subcommand("watch-endpoints", ResourceString(IDS_CMD_WATCH_ENDPOINTS));
    watchEndpointsCommand->alias("watch-ump");
    watchEndpointsCommand->add_flag("-l,--include-loopback", watchEndpointsOptions.IncludeDiagnosticLoopback, ResourceString(IDS_OPT_WATCH_INCLUDE_LOOPBACK));
    watchEndpointsCommand->add_flag("-v,--verbose", watchEndpointsOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));

    WatchPortsOptions watchPortsOptions{};

    auto watchPortsCommand = app.add_subcommand("watch-ports", ResourceString(IDS_CMD_WATCH_PORTS));
    watchPortsCommand->alias("watch-legacy");
    watchPortsCommand->add_flag("-v,--verbose", watchPortsOptions.Verbose, ResourceString(IDS_OPT_VERBOSE));

    // ---------------------------------------------------------------- bluetooth

    auto bluetoothCommand = app.add_subcommand("bluetooth", ResourceString(IDS_CMD_BLUETOOTH));
    bluetoothCommand->alias("ble");
    bluetoothCommand->require_subcommand(1);

    auto bluetoothListCommand = bluetoothCommand->add_subcommand("list", ResourceString(IDS_CMD_BT_LIST));
    bluetoothListCommand->alias("list-devices");

    BluetoothDeviceOptions bluetoothConnectOptions{};

    auto bluetoothConnectCommand = bluetoothCommand->add_subcommand("connect", ResourceString(IDS_CMD_BT_CONNECT));
    bluetoothConnectCommand->add_option("bluetooth-device-id", bluetoothConnectOptions.BluetoothDeviceId, ResourceString(IDS_OPT_BT_DEVICE_ID_ARGUMENT))->required();
    bluetoothConnectCommand->add_flag("-t,--temporary", bluetoothConnectOptions.Temporary, ResourceString(IDS_OPT_BT_TEMPORARY));

    BluetoothDeviceOptions bluetoothDisconnectOptions{};

    auto bluetoothDisconnectCommand = bluetoothCommand->add_subcommand("disconnect", ResourceString(IDS_CMD_BT_DISCONNECT));
    bluetoothDisconnectCommand->add_option("bluetooth-device-id", bluetoothDisconnectOptions.BluetoothDeviceId, ResourceString(IDS_OPT_BT_DEVICE_ID_ARGUMENT))->required();
    bluetoothDisconnectCommand->add_flag("-f,--forget", bluetoothDisconnectOptions.Forget, ResourceString(IDS_OPT_BT_FORGET));

    BluetoothCustomizeOptions bluetoothCustomizeOptions{};

    auto bluetoothCustomizeCommand = bluetoothCommand->add_subcommand("customize", ResourceString(IDS_CMD_BT_CUSTOMIZE));
    bluetoothCustomizeCommand->add_option("bluetooth-device-id", bluetoothCustomizeOptions.BluetoothDeviceId, ResourceString(IDS_OPT_BT_DEVICE_ID_ARGUMENT))->required();
    auto btNameOption = bluetoothCustomizeCommand->add_option("-n,--name", bluetoothCustomizeOptions.Name, ResourceString(IDS_OPT_BT_NAME));
    auto btDescriptionOption = bluetoothCustomizeCommand->add_option("-d,--description", bluetoothCustomizeOptions.Description, ResourceString(IDS_OPT_BT_DESCRIPTION));
    auto btImageOption = bluetoothCustomizeCommand->add_option("-i,--image", bluetoothCustomizeOptions.Image, ResourceString(IDS_OPT_BT_IMAGE));
    bluetoothCustomizeCommand->add_flag("-c,--clear", bluetoothCustomizeOptions.Clear, ResourceString(IDS_OPT_BT_CLEAR));
    bluetoothCustomizeCommand->add_option("-k,--keep-when-offline", bluetoothCustomizeOptions.KeepWhenOffline, ResourceString(IDS_OPT_BT_KEEP_WHEN_OFFLINE));
    bluetoothCustomizeCommand->add_flag("-t,--temporary", bluetoothCustomizeOptions.Temporary, ResourceString(IDS_OPT_BT_TEMPORARY));

    auto peripheralCommand = bluetoothCommand->add_subcommand("peripheral", ResourceString(IDS_CMD_BT_PERIPHERAL));
    peripheralCommand->require_subcommand(1);

    BluetoothPeripheralStartOptions peripheralStartOptions{};

    auto peripheralStartCommand = peripheralCommand->add_subcommand("start", ResourceString(IDS_CMD_BT_PERIPHERAL_START));
    peripheralStartCommand->add_option("-p,--protocol", peripheralStartOptions.Protocol, ResourceString(IDS_OPT_BT_PROTOCOL));
    peripheralStartCommand->add_flag("-t,--temporary", peripheralStartOptions.Temporary, ResourceString(IDS_OPT_BT_TEMPORARY));

    auto peripheralStopCommand = peripheralCommand->add_subcommand("stop", ResourceString(IDS_CMD_BT_PERIPHERAL_STOP));
    auto peripheralStatusCommand = peripheralCommand->add_subcommand("status", ResourceString(IDS_CMD_BT_PERIPHERAL_STATUS));

    BluetoothCustomizeOptions peripheralCustomizeOptions{};

    auto peripheralCustomizeCommand = peripheralCommand->add_subcommand("customize", ResourceString(IDS_CMD_BT_PERIPHERAL_CUSTOMIZE));
    auto pNameOption = peripheralCustomizeCommand->add_option("-n,--name", peripheralCustomizeOptions.Name, ResourceString(IDS_OPT_BT_NAME));
    auto pDescriptionOption = peripheralCustomizeCommand->add_option("-d,--description", peripheralCustomizeOptions.Description, ResourceString(IDS_OPT_BT_DESCRIPTION));
    auto pImageOption = peripheralCustomizeCommand->add_option("-i,--image", peripheralCustomizeOptions.Image, ResourceString(IDS_OPT_BT_IMAGE));
    peripheralCustomizeCommand->add_flag("-c,--clear", peripheralCustomizeOptions.Clear, ResourceString(IDS_OPT_BT_CLEAR));
    peripheralCustomizeCommand->add_flag("-t,--temporary", peripheralCustomizeOptions.Temporary, ResourceString(IDS_OPT_BT_TEMPORARY));

    BluetoothPeripheralClientOptions approveOptions{};

    auto peripheralApproveCommand = peripheralCommand->add_subcommand("approve", ResourceString(IDS_CMD_BT_PERIPHERAL_APPROVE));
    peripheralApproveCommand->add_option("bluetooth-address", approveOptions.BluetoothAddress, ResourceString(IDS_OPT_BT_ADDRESS_ARGUMENT))->required();
    peripheralApproveCommand->add_option("-s,--scope", approveOptions.Scope, ResourceString(IDS_OPT_BT_SCOPE));

    BluetoothPeripheralClientOptions denyOptions{};

    auto peripheralDenyCommand = peripheralCommand->add_subcommand("deny", ResourceString(IDS_CMD_BT_PERIPHERAL_DENY));
    peripheralDenyCommand->add_option("bluetooth-address", denyOptions.BluetoothAddress, ResourceString(IDS_OPT_BT_ADDRESS_ARGUMENT))->required();
    peripheralDenyCommand->add_option("-s,--scope", denyOptions.Scope, ResourceString(IDS_OPT_BT_SCOPE));

    BluetoothPeripheralClientOptions forgetOptions{};

    auto peripheralForgetCommand = peripheralCommand->add_subcommand("forget", ResourceString(IDS_CMD_BT_PERIPHERAL_FORGET));
    peripheralForgetCommand->add_option("bluetooth-address", forgetOptions.BluetoothAddress, ResourceString(IDS_OPT_BT_ADDRESS_ARGUMENT))->required();

    // ---------------------------------------------------------------- examples

    SetCommandExamples(enumEndpointsCommand, { "midi2 enumerate endpoints", "midi2 enum ep --all --verbose" });
    SetCommandExamples(monitorCommand, { "midi2 endpoint monitor", "midi2 endpoint monitor --verbose --include-timestamp" });
    SetCommandExamples(propertiesCommand, { "midi2 endpoint properties", "midi2 endpoint properties --verbose --include-raw" });
    SetCommandExamples(sendMessageCommand, { "midi2 endpoint send-message 0x21234567", "midi2 endpoint send-message 0x41905000 0x87654321 --count 10" });
    SetCommandExamples(sendMessageFileCommand, { "midi2 endpoint send-message-file .\\messages.txt" });
    SetCommandExamples(playNotesCommand, { "midi2 endpoint play-notes 60 62 64 65 67" });
    SetCommandExamples(sysExSendCommand, { "midi2 sysex send-file .\\patch.syx", "midi2 sysex send-file .\\patch.syx --group 1 --pause 20" });
    SetCommandExamples(sysExReceiveCommand, { "midi2 sysex receive-file .\\dump.syx", "midi2 sysex receive-file .\\dump.syx --overwrite" });
    SetCommandExamples(loopbackCreateCommand, { "midi2 loopback create --name-a \"Loop A\" --name-b \"Loop B\"" });
    SetCommandExamples(servicePingCommand, { "midi2 service ping", "midi2 service ping --count 20" });

    // ---------------------------------------------------------------- parse and dispatch

    auto arguments = GetUtf8CommandLineArguments();

    NormalizeEndpointArgumentOrder(arguments);

    std::vector<const char*> argumentPointers;
    argumentPointers.reserve(arguments.size());

    for (auto const& argument : arguments)
    {
        argumentPointers.push_back(argument.c_str());
    }

    try
    {
        app.parse(static_cast<int>(argumentPointers.size()), argumentPointers.data());
    }
    catch (CLI::ParseError const& e)
    {
        return app.exit(e);
    }

    sendMessageOptions.HasTimestamp = timestampOption->count() > 0;
    sendMessageFileOptions.HasNewGroupIndex = newGroupOption->count() > 0;

    bluetoothCustomizeOptions.HasName = btNameOption->count() > 0;
    bluetoothCustomizeOptions.HasDescription = btDescriptionOption->count() > 0;
    bluetoothCustomizeOptions.HasImage = btImageOption->count() > 0;

    peripheralCustomizeOptions.HasName = pNameOption->count() > 0;
    peripheralCustomizeOptions.HasDescription = pDescriptionOption->count() > 0;
    peripheralCustomizeOptions.HasImage = pImageOption->count() > 0;

    try
    {
        if (enumEndpointsCommand->parsed())         return RunEnumEndpointsCommand(enumEndpointsOptions);
        if (enumLegacyCommand->parsed())            return RunEnumLegacyCommand(enumLegacyOptions);
        if (enumSessionsCommand->parsed())          return RunEnumSessionsCommand(enumSessionsOptions);
        if (enumTransportsCommand->parsed())        return RunEnumTransportsCommand(enumTransportsOptions);
        if (enumPropertyKeysCommand->parsed())      return RunEnumPropertyKeysCommand();

        if (monitorCommand->parsed())               return RunMonitorCommand(monitorOptions);
        if (propertiesCommand->parsed())            return RunEndpointPropertiesCommand(propertiesOptions);
        if (sendMessageCommand->parsed())           return RunEndpointSendMessageCommand(sendMessageOptions);
        if (sendMessageFileCommand->parsed())       return RunEndpointSendMessageFileCommand(sendMessageFileOptions);
        if (playNotesCommand->parsed())             return RunEndpointPlayNotesCommand(playNotesOptions);
        if (requestFunctionBlocksCommand->parsed()) return RunEndpointRequestFunctionBlocksCommand(requestFunctionBlocksOptions);
        if (requestEndpointInfoCommand->parsed())   return RunEndpointRequestEndpointInfoCommand(requestEndpointInfoOptions);

        if (sysExSendCommand->parsed())             return RunSysExSendFileCommand(sysExSendOptions);
        if (sysExReceiveCommand->parsed())          return RunSysExReceiveFileCommand(sysExReceiveOptions);

        if (loopbackListCommand->parsed())          return RunLoopbackListCommand();        if (loopbackCreateCommand->parsed())        return RunLoopbackCreateCommand(loopbackCreateOptions);
        if (loopbackRemoveCommand->parsed())        return RunLoopbackRemoveCommand(loopbackRemoveOptions);
        if (basicLoopbackListCommand->parsed())     return RunBasicLoopbackListCommand();
        if (basicLoopbackCreateCommand->parsed())   return RunBasicLoopbackCreateCommand(basicLoopbackCreateOptions);
        if (basicLoopbackRemoveCommand->parsed())   return RunBasicLoopbackRemoveCommand(basicLoopbackRemoveOptions);

        if (serviceStatusCommand->parsed())         return RunServiceStatusCommand(serviceStatusOptions);
        if (servicePingCommand->parsed())           return RunServicePingCommand(servicePingOptions);
        if (timeCommand->parsed())                  return RunTimeCommand();
        if (watchEndpointsCommand->parsed())        return RunWatchEndpointsCommand(watchEndpointsOptions);
        if (watchPortsCommand->parsed())            return RunWatchPortsCommand(watchPortsOptions);

        if (bluetoothListCommand->parsed())         return RunBluetoothListCommand();
        if (bluetoothConnectCommand->parsed())      return RunBluetoothConnectCommand(bluetoothConnectOptions);
        if (bluetoothDisconnectCommand->parsed())   return RunBluetoothDisconnectCommand(bluetoothDisconnectOptions);
        if (bluetoothCustomizeCommand->parsed())    return RunBluetoothCustomizeCommand(bluetoothCustomizeOptions);
        if (peripheralStartCommand->parsed())       return RunBluetoothPeripheralStartCommand(peripheralStartOptions);
        if (peripheralStopCommand->parsed())        return RunBluetoothPeripheralStopCommand();
        if (peripheralStatusCommand->parsed())      return RunBluetoothPeripheralStatusCommand();
        if (peripheralCustomizeCommand->parsed())   return RunBluetoothPeripheralCustomizeCommand(peripheralCustomizeOptions);
        if (peripheralApproveCommand->parsed())     return RunBluetoothPeripheralApproveCommand(approveOptions);
        if (peripheralDenyCommand->parsed())        return RunBluetoothPeripheralDenyCommand(denyOptions);
        if (peripheralForgetCommand->parsed())      return RunBluetoothPeripheralForgetCommand(forgetOptions);
    }
    catch (winrt::hresult_error const& e)
    {
        WriteErrorLine(FormatResourceString(IDS_ERROR_UNHANDLED,
            fmt::format("{:08X}", static_cast<uint32_t>(e.code())), ToUtf8(e.message())));

        return 1;
    }

    return 0;
}
