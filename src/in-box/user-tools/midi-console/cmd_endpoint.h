// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    // Options shared by every command under "endpoint", matching the base settings class in the
    // shipping console.
    struct EndpointCommonOptions
    {
        std::string EndpointDeviceId;
        int DelayBetweenMessages{ 2 };
        std::string WordDataFormat{ "Hex" };
        bool NoWait{ false };
    };

    struct EndpointPropertiesOptions
    {
        std::string EndpointDeviceId;
        bool Verbose{ false };
        bool IncludeRawProperties{ false };
        bool IncludeNameTable{ false };
    };

    struct EndpointSendMessageOptions : EndpointCommonOptions
    {
        std::vector<std::string> Words;
        int Count{ 1 };
        int64_t TimestampOffsetMicroseconds{ 0 };
        bool HasTimestamp{ false };
        uint64_t Timestamp{ 0 };
        bool DebugAutoIncrementLastWord{ false };
    };

    struct EndpointSendMessageFileOptions : EndpointCommonOptions
    {
        std::string InputFile;
        std::string FieldDelimiter{ "Auto" };
        bool Verbose{ false };
        bool HasNewGroupIndex{ false };
        int NewGroupIndex{ 0 };
    };

    struct EndpointPlayNotesOptions
    {
        std::string EndpointDeviceId;
        std::vector<std::string> NoteIndexes;
        int Length{ 250 };
        int Rest{ 250 };
        int GroupNumber{ 1 };
        int ChannelNumber{ 1 };
        double Velocity{ 75.0 };
        bool Forever{ false };
        bool Midi2{ false };
        bool AutoReconnect{ true };
    };

    struct EndpointRequestFunctionBlocksOptions
    {
        std::string EndpointDeviceId;
        bool RequestAll{ false };
        int FunctionBlockNumber{ 0 };
        bool RequestInfo{ true };
        bool RequestName{ true };
    };

    struct EndpointRequestEndpointInfoOptions
    {
        std::string EndpointDeviceId;
        bool RequestAll{ false };
        bool RequestEndpointInfo{ true };
        bool RequestDeviceIdentity{ false };
        bool RequestEndpointName{ false };
        bool RequestProductInstanceId{ false };
        bool RequestStreamConfiguration{ false };
        int UmpVersionMajor{ 1 };
        int UmpVersionMinor{ 1 };
    };

    struct EndpointCustomizeOptions
    {
        std::string EndpointDeviceId;
        std::string Name;
        std::string Description;
        std::string Image;
        std::string PortNaming;
        bool HasName{ false };
        bool HasDescription{ false };
        bool HasImage{ false };
        bool HasPortNaming{ false };
        bool Clear{ false };
        bool NoteOffTranslation{ false };
        bool HasNoteOffTranslation{ false };
        bool MidiPolyphonicExpression{ false };
        bool HasMidiPolyphonicExpression{ false };
        int ControlChangeIntervalMilliseconds{ 0 };
        bool HasControlChangeInterval{ false };
        uint64_t OutgoingLatencyTicks{ 0 };
        bool HasOutgoingLatencyTicks{ false };
        bool Temporary{ false };
    };

    struct EndpointIdOptions
    {
        std::string Value;
    };

    struct EndpointSendClockOptions
    {
        std::string EndpointDeviceId;
        double Tempo{ 120.0 };
        int PulsesPerQuarterNote{ 24 };
        std::vector<int> GroupNumbers;
        bool SendStartMessage{ false };
        bool SendStopMessage{ false };
    };

    int RunEndpointPropertiesCommand(_In_ EndpointPropertiesOptions const& options);
    int RunEndpointSendMessageCommand(_In_ EndpointSendMessageOptions const& options);
    int RunEndpointSendMessageFileCommand(_In_ EndpointSendMessageFileOptions const& options);
    int RunEndpointPlayNotesCommand(_In_ EndpointPlayNotesOptions const& options);
    int RunEndpointRequestFunctionBlocksCommand(_In_ EndpointRequestFunctionBlocksOptions const& options);
    int RunEndpointRequestEndpointInfoCommand(_In_ EndpointRequestEndpointInfoOptions const& options);
    int RunEndpointCustomizeCommand(_In_ EndpointCustomizeOptions const& options);
    int RunEndpointShortIdCommand(_In_ EndpointIdOptions const& options);
    int RunEndpointFullIdCommand(_In_ EndpointIdOptions const& options);
    int RunEndpointSendClockCommand(_In_ EndpointSendClockOptions const& options);
}
