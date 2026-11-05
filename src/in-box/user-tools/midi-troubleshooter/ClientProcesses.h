// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    struct ClientModule
    {
        std::wstring Path{};
        std::wstring Version{};
    };

    struct ClientProcess
    {
        DWORD ProcessId{ 0 };
        std::wstring ImageName{};

        // Only the modules that matter to MIDI: the WinRT MIDI APIs, WinMM, wdmaud2 and the
        // Windows MIDI Services client libraries.
        std::vector<ClientModule> MidiModules{};

        // False when the module list could not be read, for example across architectures
        bool ModulesRead{ false };
    };

    // The command-line and GUI apps that the Windows MIDI Services installers ship. Keep in step
    // with $InBoxClientProcessNames in CollectMidiLogs\tttraceall.psm1.
    std::vector<std::wstring> const& InBoxClientProcessNames() noexcept;

    // Every running process whose image name is one of the in-box apps, except this one: the
    // app driving the capture is not the client being diagnosed, and attaching a recorder to
    // the process that is waiting on that recorder is asking for trouble.
    std::vector<ClientProcess> FindRunningInBoxClients() noexcept;

    // Plain text for the capture package, one block per process with its MIDI modules.
    std::wstring DescribeClientProcesses(_In_ std::vector<ClientProcess> const& processes) noexcept;
}
