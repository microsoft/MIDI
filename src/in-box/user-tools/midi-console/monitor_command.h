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
    struct MonitorOptions
    {
        std::string EndpointDeviceId;
        bool Verbose{ false };
        bool IncludeTimestamp{ false };
        bool DecodeMessages{ true };
        bool IncludeRealTimeMessages{ false };
        bool IncludeUtilityMessages{ true };
        bool AutoReconnect{ true };
        bool SingleMessage{ false };
        std::string CaptureToFile;
        bool AnnotateCapture{ false };
        std::string CaptureFieldDelimiter{ "Space" };
    };

    int RunMonitorCommand(_In_ MonitorOptions const& options);
}
