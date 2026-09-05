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
    struct SysExSendFileOptions
    {
        std::string EndpointDeviceId;
        std::string InputFile;
        int GroupNumber{ 1 };
        int DelayBetweenMessages{ 50 };
        int MessageTransferCount{ 64 };
    };

    struct SysExReceiveFileOptions
    {
        std::string EndpointDeviceId;
        std::string OutputFile;
        int GroupNumber{ 1 };
        int TimeoutSeconds{ 10 };
        bool Overwrite{ false };
    };

    int RunSysExSendFileCommand(_In_ SysExSendFileOptions const& options);
    int RunSysExReceiveFileCommand(_In_ SysExReceiveFileOptions const& options);
}
