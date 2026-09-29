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
    struct RtpListOptions
    {
        bool Verbose{ false };
    };

    int RunRtpHostsCommand(_In_ RtpListOptions const& options);
    int RunRtpClientsCommand(_In_ RtpListOptions const& options);
    int RunRtpBrowseCommand(_In_ RtpListOptions const& options);
    int RunRtpPendingCommand();
    int RunRtpStatusCommand(_In_ RtpListOptions const& options);
}
